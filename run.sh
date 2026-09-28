#!/bin/bash
# ============================================================
# All-in-one: download deps → build backend → build CSS gen →
# generate CSS → start server di 0.0.0.0:8080
# TANPA sudo, TANPA root.
# ============================================================
set -e

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT"

echo "══════════════════════════════════════════════════════"
echo "  API KEY SYSTEM — build & run"
echo "══════════════════════════════════════════════════════"

# ------------------------------------------------------------
echo "[1/5] Download dependencies (httplib.h + json.hpp)..."
cd "$ROOT/backend"
chmod +x download_deps.sh
./download_deps.sh
cd "$ROOT"

# ------------------------------------------------------------
echo
echo "[2/5] Build backend..."
BIN=""
if command -v cmake >/dev/null 2>&1; then
  mkdir -p "$ROOT/backend/build"
  cd "$ROOT/backend/build"
  cmake .. -DCMAKE_BUILD_TYPE=Release >/dev/null
  make -j"$(nproc 2>/dev/null || echo 2)"
  cd "$ROOT"
  BIN="$ROOT/backend/build/apiserver"
else
  echo "  cmake tidak ditemukan — fallback ke g++ langsung..."
  g++ -std=c++17 -O2 -pthread \
      -I"$ROOT/backend/include" \
      -o "$ROOT/backend/apiserver" \
      "$ROOT/backend/main.cpp"
  BIN="$ROOT/backend/apiserver"
fi
echo "  → binary: $BIN"

# ------------------------------------------------------------
echo
echo "[3/5] Build CSS generator..."
g++ -std=c++17 -O2 -o "$ROOT/frontend/generate_css" \
    "$ROOT/frontend/generate_css.cpp"
echo "  → frontend/generate_css"

# ------------------------------------------------------------
echo
echo "[4/5] Generate style-mega.css (10.000.000 baris)..."
if [ ! -s "$ROOT/frontend/style-mega.css" ]; then
  time "$ROOT/frontend/generate_css" "$ROOT/frontend/style-mega.css"
else
  echo "  style-mega.css sudah ada — skip (hapus file-nya untuk regenerate)."
fi
ls -lh "$ROOT/frontend/style-mega.css" 2>/dev/null || true

# ------------------------------------------------------------
echo
echo "[5/5] Starting server on 0.0.0.0:8080"
echo "──────────────────────────────────────────────────────"
echo "  Buka tab PORTS di Codespaces → port 8080 → 🌐 icon"
echo "  Tekan Ctrl+C untuk stop server"
echo "──────────────────────────────────────────────────────"
exec "$BIN" "$ROOT/frontend"
