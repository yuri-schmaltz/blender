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

# Automatically install missing dependencies on Debian/Ubuntu systems
install_packages() {
  local packages=("$@")
  local missing=()

  for pkg in "${packages[@]}"; do
    dpkg -s "$pkg" >/dev/null 2>&1 || missing+=("$pkg")
  done

  if [ ${#missing[@]} -gt 0 ]; then
    echo "Installing missing packages: ${missing[*]}"
    local sudo_cmd=""
    if command -v sudo >/dev/null 2>&1; then
      sudo_cmd="sudo"
    fi
    $sudo_cmd apt-get update
    # shellcheck disable=SC2086
    $sudo_cmd apt-get install -y ${missing[*]}
  fi
}

# Only attempt installation if apt-get and dpkg are available
if command -v apt-get >/dev/null 2>&1 && command -v dpkg >/dev/null 2>&1; then
  deps=(libzstd-dev libjpeg-dev)
  install_packages "${deps[@]}"
else
  echo "Warning: apt-get or dpkg not found, skipping dependency installation" >&2
fi

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
