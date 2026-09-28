// Generator CSS 10.000.000 baris utility class (streaming, buffer 512 KB)
// Setiap class < 50 byte. TIDAK pakai box-shadow/filter/backdrop-filter/text-shadow/clip-path.
#include <cstdio>
#include <cstdlib>
#include <string>

int main(int argc, char** argv) {
    const char* out_path = (argc > 1) ? argv[1] : "style-mega.css";
    FILE* f = std::fopen(out_path, "wb");
    if (!f) { std::perror("fopen"); return 1; }

    // Buffer I/O besar biar cepat
    static char iobuf[1 << 20];
    std::setvbuf(f, iobuf, _IOFBF, sizeof(iobuf));

    std::fputs(":root{--bg:#000;--fg:#fff;--muted:#888;--border:#222;--hover:#111;}\n", f);

    const long long TOTAL = 10000000LL;
    std::string buf;
    buf.reserve(1 << 19);

    char line[96];

    for (long long i = 0; i < TOTAL; ++i) {
        int p = (int)(i % 10);
        int n = (int)(i / 10);
        int len = 0;

        switch (p) {
            case 0: len = std::snprintf(line, sizeof(line), ".m-%d{margin:%dpx}",       n, n % 1000); break;
            case 1: len = std::snprintf(line, sizeof(line), ".p-%d{padding:%dpx}",      n, n % 1000); break;
            case 2: len = std::snprintf(line, sizeof(line), ".w-%d{width:%dpx}",        n, n % 2000); break;
            case 3: len = std::snprintf(line, sizeof(line), ".h-%d{height:%dpx}",       n, n % 2000); break;
            case 4: len = std::snprintf(line, sizeof(line), ".o-%d{opacity:0.%02d}",    n, n % 100);  break;
            case 5: len = std::snprintf(line, sizeof(line), ".c-%d{color:#%03x}",       n, n % 4096); break;
            case 6: len = std::snprintf(line, sizeof(line), ".bg-%d{background:#%03x}", n, n % 4096); break;
            case 7: len = std::snprintf(line, sizeof(line), ".t-%d{transform:translateX(%dpx)}", n, n % 500); break;
            case 8: len = std::snprintf(line, sizeof(line), ".r-%d{border-radius:%dpx}", n, n % 200); break;
            case 9: len = std::snprintf(line, sizeof(line), ".f-%d{font-size:%dpx}",     n, n % 100); break;
        }

        buf.append(line, len);
        buf.push_back('\n');

        if (buf.size() >= (1 << 19)) {
            std::fwrite(buf.data(), 1, buf.size(), f);
            buf.clear();
        }
    }
    if (!buf.empty()) std::fwrite(buf.data(), 1, buf.size(), f);

    std::fclose(f);
    std::printf("Generated %lld lines → %s\n", TOTAL, out_path);
    return 0;
}
