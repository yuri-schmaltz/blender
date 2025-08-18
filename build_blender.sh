#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$ROOT_DIR/build"
LOG_FILE="$ROOT_DIR/install.log"
: >"$LOG_FILE"
exec 2> >(tee -a "$LOG_FILE" >&2)
trap 'echo "An error occurred. Check $LOG_FILE for details." >&2' ERR

check_command() {
  command -v "$1" >/dev/null 2>&1 || {
    echo "Error: required command '$1' not found" >&2
    exit 1
  }
}

# Automatically install missing packages on Debian/Ubuntu systems
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

# Ensure essential build tools are present
for cmd in git cmake make; do
  if ! command -v "$cmd" >/dev/null 2>&1; then
    if command -v apt-get >/dev/null 2>&1 && command -v dpkg >/dev/null 2>&1; then
      install_packages "$cmd"
    fi
  fi
  check_command "$cmd"
done

# Use Blender's official dependency installer when available
if command -v apt-get >/dev/null 2>&1 && command -v dpkg >/dev/null 2>&1; then
  install_packages python3
  install_script="$ROOT_DIR/build_files/build_environment/install_linux_packages.py"
  if command -v python3 >/dev/null 2>&1; then
    echo "Ensuring all build dependencies are installed"
    if [ "$(id -u)" -eq 0 ]; then
      python3 "$install_script" --no-sudo --all
    else
      python3 "$install_script" --all
    fi
  else
    echo "Warning: python3 not found after installation attempt, skipping dependency installation" >&2
  fi
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
