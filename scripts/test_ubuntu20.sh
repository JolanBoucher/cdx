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
#   ./scripts/test_ubuntu20.sh [-test|--test]
#
# -test/--test: also configure, build, and run each branch's own unit test
# suite (lib, builder, coverage) and report results. lib's tests are always
# built as part of the main configure below, so they're just ctest'd there.
# builder's and coverage's test suites are each built from their own,
# separate build directory - NOT together in the main merged build - because
# enabling both CDX_BUILDER_BUILD_TESTS and CDX_BUILD_TESTS in the very same
# configure risks a GoogleTest target collision (see the comment above
# set(CDX_BUILDER_BUILD_TESTS ...) in the top-level CMakeLists.txt for the
# full explanation). Building each branch's tests from its own configure
# sidesteps that entirely, at the cost of a second GoogleTest fetch/build per
# branch.
#
# Known compatibility risks, both left visible rather than silently patched:
#
#   1. builder/cmake/CompilerRequirements.cmake requires GCC/G++ >= 11
#      (C++17 features it relies on). Ubuntu 20.04's default GCC is 9.4.0,
#      so this script pulls GCC 11 from the ubuntu-toolchain-r/test PPA and
#      points cmake at it explicitly (-DCMAKE_C_COMPILER / -DCMAKE_CXX_COMPILER).
#
#   2. coverage/CMakeLists.txt's `find_package(absl REQUIRED)`: Ubuntu
#      20.04's default repos do not ship libabsl-dev (Abseil packaging for
#      Debian/Ubuntu only starts around Ubuntu 22.04/jammy). That call was
#      written assuming a Homebrew-provided modern Protobuf+Abseil pair (as
#      on the developer's Mac) and may still fail here even after installing
#      everything else below - if so, that failure IS the test result.

set -euo pipefail

RUN_TESTS=0
for arg in "$@"; do
    case "${arg}" in
        -test|--test) RUN_TESTS=1 ;;
        *)
            echo "Unknown argument: ${arg} (expected -test or --test)" >&2
            exit 1
            ;;
    esac
done

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${REPO_ROOT}/build-ubuntu20-test"
BUILDER_TEST_BUILD_DIR="${REPO_ROOT}/builder/build-ubuntu20-test"
COVERAGE_TEST_BUILD_DIR="${REPO_ROOT}/coverage/build-ubuntu20-test"

echo "==> Repo root: ${REPO_ROOT}"
echo "==> Build dir: ${BUILD_DIR}"

export DEBIAN_FRONTEND=noninteractive

echo "==> apt-get update"
apt-get update -qq

echo "==> Enabling universe (needed for libhts-dev) and toolchain PPA (needed for GCC 11)"
apt-get install -y -qq software-properties-common ca-certificates
add-apt-repository -y universe >/dev/null
add-apt-repository -y ppa:ubuntu-toolchain-r/test >/dev/null
apt-get update -qq

echo "==> Installing packages"
apt-get install -y -qq \
    build-essential \
    gcc-11 \
    g++-11 \
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

echo "==> Configuring (cmake, forcing GCC 11 as CMAKE_C/CXX_COMPILER)"
cmake -S "${REPO_ROOT}" -B "${BUILD_DIR}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER=gcc-11 \
    -DCMAKE_CXX_COMPILER=g++-11

echo "==> Building"
cmake --build "${BUILD_DIR}" -j"$(nproc)"

echo "==> Sanity check: cdx --help"
"${BUILD_DIR}/cdx" --help

echo "==> SUCCESS: cdx built and ran on this environment."

if [ "${RUN_TESTS}" -eq 1 ]; then
    echo ""
    echo "==> -test passed: running unit test suites"

    echo ""
    echo "==> lib tests (already built as part of the main configure above)"
    ctest --test-dir "${BUILD_DIR}/lib" --output-on-failure

    echo ""
    echo "==> builder tests (own separate configure - see header comment for why)"
    cmake -S "${REPO_ROOT}/builder" -B "${BUILDER_TEST_BUILD_DIR}" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_C_COMPILER=gcc-11 \
        -DCMAKE_CXX_COMPILER=g++-11 \
        -DCDX_BUILDER_BUILD_TESTS=ON
    cmake --build "${BUILDER_TEST_BUILD_DIR}" -j"$(nproc)"
    ctest --test-dir "${BUILDER_TEST_BUILD_DIR}" --output-on-failure

    echo ""
    echo "==> coverage tests (own separate configure - see header comment for why)"
    cmake -S "${REPO_ROOT}/coverage" -B "${COVERAGE_TEST_BUILD_DIR}" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_C_COMPILER=gcc-11 \
        -DCMAKE_CXX_COMPILER=g++-11 \
        -DCDX_BUILD_TESTS=ON
    cmake --build "${COVERAGE_TEST_BUILD_DIR}" -j"$(nproc)"
    ctest --test-dir "${COVERAGE_TEST_BUILD_DIR}" --output-on-failure

    echo ""
    echo "==> SUCCESS: all three test suites ran on this environment."
fi
