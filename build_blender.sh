#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$ROOT_DIR/build"

check_command() {
  command -v "$1" >/dev/null 2>&1 || {
    echo "Error: required command '$1' not found" >&2
    exit 1
  }
}

for cmd in git cmake make; do
  check_command "$cmd"
done

# Update submodules
if [ -d "$ROOT_DIR/.git" ]; then
  git submodule update --init --recursive
fi

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure
# shellcheck disable=SC2086
cmake .. -DCMAKE_BUILD_TYPE=Release ${CMAKE_ARGS:-}

# Build
make -j"$(nproc)"

# Install
if command -v sudo >/dev/null 2>&1; then
  sudo make install
else
  make install
fi
