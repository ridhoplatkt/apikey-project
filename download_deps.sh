#!/bin/bash
# Download httplib.h + json.hpp ke folder lokal. TANPA sudo.
set -e
cd "$(dirname "$0")"

mkdir -p include

if [ ! -s include/httplib.h ]; then
  echo "  → Downloading httplib.h ..."
  curl -fsSL -o include/httplib.h \
    https://raw.githubusercontent.com/yhirose/cpp-httplib/master/httplib.h
fi

if [ ! -s include/json.hpp ]; then
  echo "  → Downloading json.hpp ..."
  mkdir -p include/nlohmann
  curl -fsSL -o include/nlohmann/json.hpp \
    https://raw.githubusercontent.com/nlohmann/json/develop/single_include/nlohmann/json.hpp
  # Juga sediakan include/json.hpp sebagai alias
  cp include/nlohmann/json.hpp include/json.hpp
fi

echo "  → Dependencies ready in backend/include/"
ls -lh include/httplib.h include/json.hpp
