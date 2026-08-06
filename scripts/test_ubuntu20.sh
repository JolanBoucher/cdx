#!/usr/bin/env bash
#
# test_ubuntu20.sh
#
# Standalone portability test: installs the minimal apt dependencies needed
# to configure and build the cdx monorepo, then builds it and runs a basic
# sanity check (`cdx --help`). Deliberately a plain shell script (not
# Docker-specific) so it works the same way on any fresh Ubuntu 20.04
# (focal) box - a clean Docker container, a VM, a CI runner, whatever.
#
# Usage (from a fresh ubuntu:20.04 box, run as root or with sudo):
#   ./scripts/test_ubuntu20.sh
#
# Known compatibility risk (see coverage/CMakeLists.txt's
# `find_package(absl REQUIRED)`): Ubuntu 20.04's default repos do not ship
# libabsl-dev (Abseil packaging for Debian/Ubuntu only starts around
# Ubuntu 22.04/jammy). That find_package() call was written assuming a
# Homebrew-provided modern Protobuf+Abseil pair (as on the developer's
# Mac) and will very likely FAIL here with stock apt packages. This script
# does not try to paper over that - it installs everything else and lets
# cmake configure fail there if it's going to, so the real gap is visible
# instead of silently patched away.

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${REPO_ROOT}/build-ubuntu20-test"

echo "==> Repo root: ${REPO_ROOT}"
echo "==> Build dir: ${BUILD_DIR}"

export DEBIAN_FRONTEND=noninteractive

echo "==> apt-get update"
apt-get update -qq

echo "==> Enabling universe (needed for libhts-dev) and installing packages"
apt-get install -y -qq software-properties-common ca-certificates
add-apt-repository -y universe >/dev/null
apt-get update -qq

apt-get install -y -qq \
    build-essential \
    cmake \
    git \
    pkg-config \
    libssl-dev \
    libboost-all-dev \
    libzstd-dev \
    libjansson-dev \
    libprotobuf-dev \
    protobuf-compiler \
    libcairo2-dev \
    libhts-dev \
    python3 \
    python3-venv \
    python3-pip

echo "==> Checking for libabsl-dev (known gap on Ubuntu 20.04 - see header comment)"
if apt-cache show libabsl-dev >/dev/null 2>&1; then
    echo "    libabsl-dev is available here - installing it."
    apt-get install -y -qq libabsl-dev
else
    echo "    libabsl-dev is NOT available in this environment's repos."
    echo "    coverage/CMakeLists.txt's find_package(absl REQUIRED) will likely"
    echo "    fail below - that failure IS the test result, not a script bug."
fi

echo "==> Configuring (cmake)"
cmake -S "${REPO_ROOT}" -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE=Release

echo "==> Building"
cmake --build "${BUILD_DIR}" -j"$(nproc)"

echo "==> Sanity check: cdx --help"
"${BUILD_DIR}/cdx" --help

echo "==> SUCCESS: cdx built and ran on this environment."
