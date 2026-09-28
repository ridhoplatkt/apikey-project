// ============================================================
// API Key Server — C++17 + cpp-httplib + nlohmann/json
// Listen: 0.0.0.0:8080
// ============================================================
#include "httplib.h"
#include "json.hpp"

#include <iostream>
#include <fstream>
#include <string>
#include <random>
#include <chrono>
#include <mutex>
#include <unordered_map>
#include <ctime>
#include <cstdio>
#include <cstdint>
#include <algorithm>

#ifdef HAVE_OPENSSL
  #include <openssl/sha.h>
#endif

using json = nlohmann::json;
using namespace std::chrono;

// ------------------------------------------------------------
// Global state
// ------------------------------------------------------------
static std::mutex                                     g_mutex;
static std::unordered_map<std::string, json>          g_keys;   // api_key -> {created_at}
static std::unordered_map<std::string,
                          std::pair<int, long long>>  g_rate;   // api_key -> (count, window_start_sec)

static const char* DB_FILE  = "keys.db";
static const int   RATE_MAX = 100;   // 100 req / menit
static const int   RATE_WIN = 60;    // detik

// ------------------------------------------------------------
// SHA-256 / fallback
// ------------------------------------------------------------
static std::string sha_hex(const std::string& in) {
#ifdef HAVE_OPENSSL
    unsigned char h[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(in.data()), in.size(), h);
    static const char* HEX = "0123456789abcdef";
    std::string out; out.reserve(64);
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
        out.push_back(HEX[h[i] >> 4]);
        out.push_back(HEX[h[i] & 0xF]);
    }
    return out;
#else
    // FNV-1a + mix — bukan kriptografi, hanya untuk fallback
    uint64_t h1 = 0xcbf29ce484222325ULL;
    uint64_t h2 = 0x9e3779b97f4a7c15ULL;
    for (unsigned char c : in) {
        h1 ^= c; h1 *= 0x100000001b3ULL;
        h2 = (h2 ^ c) * 0x9e3779b97f4a7c15ULL;
        h2 ^= (h2 >> 29);
    }
    uint64_t a = h1;
    uint64_t b = h2;
    uint64_t c = h1 ^ h2;
    uint64_t d = h1 + h2 * 0x9e3779b97f4a7c15ULL;
    char buf[80];
    std::snprintf(buf, sizeof(buf),
                  "%016llx%016llx%016llx%016llx",
                  (unsigned long long)a, (unsigned long long)b,
                  (unsigned long long)c, (unsigned long long)d);
    return std::string(buf);
#endif
}

// ------------------------------------------------------------
// Helpers
// ------------------------------------------------------------
static std::string hex64() {
    std::random_device rd;
    std::mt19937_64 gen(((uint64_t)rd() << 32) ^ rd() ^
                        (uint64_t)high_resolution_clock::now().time_since_epoch().count());
    std::uniform_int_distribution<int> dist(0, 15);
    static const char* H = "0123456789abcdef";
    std::string s; s.reserve(64);
    for (int i = 0; i < 64; ++i) s.push_back(H[dist(gen)]);
    return s;
}

static std::string now_iso() {
    std::time_t t = std::time(nullptr);
    std::tm tm{};
    gmtime_r(&t, &tm);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
    return std::string(buf);
}

// ------------------------------------------------------------
// Persistence (keys.db, JSON)
// ------------------------------------------------------------
static void load_db() {
    std::ifstream in(DB_FILE);
    if (!in) return;
    try {
        json j; in >> j;
        if (j.is_object()) {
            for (auto it = j.begin(); it != j.end(); ++it) {
                g_keys[it.key()] = it.value();
            }
        }
    } catch (...) { /* corrupt file — abaikan */ }
}

// dipanggil dengan g_mutex sudah dikunci
static void save_db_locked() {
    json j = json::object();
    for (auto& kv : g_keys) j[kv.first] = kv.second;
    std::ofstream out(DB_FILE, std::ios::trunc);
    out << j.dump(2);
}

// ------------------------------------------------------------
// Path publik (tanpa auth):  /health, /auth/*, file statis
// ------------------------------------------------------------
static bool is_public_path(const std::string& p) {
    if (p.empty() || p == "/" || p == "/health") return true;
    if (p.rfind("/auth/", 0) == 0) return true;
    // Punya ekstensi (file statis) → publik
    auto slash = p.find_last_of('/');
    auto dot   = p.find_last_of('.');
    if (dot != std::string::npos &&
        (slash == std::string::npos || dot > slash)) return true;
    return false;
}

// ------------------------------------------------------------
// Main
// ------------------------------------------------------------
int main(int argc, char** argv) {
    std::string frontend_dir = (argc > 1) ? argv[1] : "frontend";

    load_db();

    httplib::Server svr;

    // Log setiap request (ringkas)
    svr.set_logger([](const httplib::Request& req, const httplib::Response& res) {
        std::cout << "[" << req.method << "] " << req.path
                  << " → " << res.status << std::endl;
    });

    // CORS (agar fetch dari origin manapun tetap jalan)
    svr.set_post_routing_handler([](const httplib::Request&, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin",  "*");
        res.set_header("Access-Control-Allow-Headers", "Content-Type, X-API-Key");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    });

    // Preflight OPTIONS
    svr.Options(".*", [](const httplib::Request&, httplib::Response& res) {
        res.status = 204;
    });

    // ------------------------------------------------------------
    // Middleware auth + rate-limit
    // ------------------------------------------------------------
    svr.set_pre_routing_handler([](const httplib::Request& req,
                                   httplib::Response& res) {
        if (req.method == "OPTIONS") return httplib::Server::HandlerResponse::Unhandled;
        if (is_public_path(req.path)) return httplib::Server::HandlerResponse::Unhandled;

        std::string key = req.get_header_value("X-API-Key");
        if (key.empty()) {
            res.status = 401;
            res.set_content(json{{"error","missing X-API-Key header"},
                                 {"valid",false}}.dump(), "application/json");
            return httplib::Server::HandlerResponse::Handled;
        }

        {
            std::lock_guard<std::mutex> lk(g_mutex);
            if (g_keys.find(key) == g_keys.end()) {
                res.status = 401;
                res.set_content(json{{"error","invalid api key"},
                                     {"valid",false}}.dump(), "application/json");
                return httplib::Server::HandlerResponse::Handled;
            }
        }

        // Rate-limit
        long long now = duration_cast<seconds>(
                            steady_clock::now().time_since_epoch()).count();
        bool allow = true;
        int  remaining = 0;
        {
            std::lock_guard<std::mutex> lk(g_mutex);
            auto& slot = g_rate[key];
            if (slot.second == 0 || now - slot.second >= RATE_WIN) {
                slot.first  = 1;
                slot.second = now;
                remaining   = RATE_MAX - 1;
            } else if (slot.first >= RATE_MAX) {
                allow = false;
            } else {
                slot.first += 1;
                remaining   = RATE_MAX - slot.first;
            }
        }
        if (!allow) {
            res.status = 429;
            res.set_header("Retry-After", "60");
            res.set_content(json{{"error","rate limit exceeded"},
                                 {"limit", RATE_MAX},
                                 {"window_seconds", RATE_WIN}}.dump(),
                            "application/json");
            return httplib::Server::HandlerResponse::Handled;
        }
        res.set_header("X-RateLimit-Limit",     std::to_string(RATE_MAX));
        res.set_header("X-RateLimit-Remaining", std::to_string(remaining));
        return httplib::Server::HandlerResponse::Unhandled;
    });

    // ------------------------------------------------------------
    // GET /health
    // ------------------------------------------------------------
    svr.Get("/health", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(json{{"status","ok"},
                             {"time", now_iso()}}.dump(), "application/json");
    });

    // ------------------------------------------------------------
    // POST /auth/register
    // ------------------------------------------------------------
    svr.Post("/auth/register", [](const httplib::Request&, httplib::Response& res) {
        std::string key = hex64();
        std::string ts  = now_iso();
        std::string h   = sha_hex(key);
        {
            std::lock_guard<std::mutex> lk(g_mutex);
            g_keys[key] = json{{"created_at", ts}, {"key_hash", h}};
            save_db_locked();
        }
        std::cout << "[+] Registered new key: " << key.substr(0, 12)
                  << "... (hash=" << h.substr(0, 12) << ")" << std::endl;

        res.set_content(json{{"api_key",    key},
                             {"created_at", ts},
                             {"key_hash",   h}}.dump(),
                        "application/json");
    });

    // ------------------------------------------------------------
    // POST /auth/verify
    // ------------------------------------------------------------
    svr.Post("/auth/verify", [](const httplib::Request& req, httplib::Response& res) {
        std::string key = req.get_header_value("X-API-Key");
        bool valid = false;
        std::string created;
        {
            std::lock_guard<std::mutex> lk(g_mutex);
            auto it = g_keys.find(key);
            if (!key.empty() && it != g_keys.end()) {
                valid   = true;
                created = it->second.value("created_at", "");
            }
        }
        res.status = valid ? 200 : 401;
        res.set_content(json{{"valid", valid},
                             {"created_at", created}}.dump(),
                        "application/json");
    });

    // ------------------------------------------------------------
    // GET /data  (protected)
    // ------------------------------------------------------------
    svr.Get("/data", [](const httplib::Request&, httplib::Response& res) {
        json out = {
            {"message", "Hello from protected endpoint"},
            {"server_time", now_iso()},
            {"data", json::array({
                {{"id",1},{"name","alpha"},{"value",42}},
                {{"id",2},{"name","beta"}, {"value",17}},
                {{"id",3},{"name","gamma"},{"value",99}}
            })}
        };
        res.set_content(out.dump(2), "application/json");
    });

    // ------------------------------------------------------------
    // Static file serving (frontend)
    // ------------------------------------------------------------
    if (svr.set_mount_point("/", frontend_dir)) {
        std::cout << "[+] Serving frontend from: " << frontend_dir << std::endl;
    } else {
        std::cerr << "[!] WARNING: failed to mount '" << frontend_dir
                  << "' — UI tidak akan tampil." << std::endl;
    }

    std::cout << "==============================================\n"
              << "  API KEY SERVER\n"
              << "  Listen: 0.0.0.0:8080\n"
              << "  Endpoints: /health /auth/register /auth/verify /data\n"
              << "==============================================\n";

    if (!svr.listen("0.0.0.0", 8080)) {
        std::cerr << "[!] Failed to bind 0.0.0.0:8080" << std::endl;
        return 1;
    }
    return 0;
}
