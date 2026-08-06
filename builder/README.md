# cdx_builder

## Overview

cdx_builder generates a compact CDX coordinate index from a pangenome graph in GBZ format.

By combining graph topology with haplotype path information, it assigns continuous local
coordinates to nodes within connected components. The resulting CDX index is designed to
enable high-throughput read coverage calculations from GAM alignment files.

### Pipeline at a glance

1. **Graph Loading:** Reads GBZ graph and extracts node metadata.
2. **Component Analysis:** Partitions graph topology into connected components.
3. **Topology Relaxation:** Optimizes node spatial positioning using path weights and sparse graph layout algorithms.
4. **Coordinate Mapping:** Assigns continuous local coordinates across each component.
5. **Serialization:** Exports the final CDX index.

## Features

- **GBZ Native:** Direct loading and validation of GBWT/GBZ pangenome graphs.
- **Haplotype-Aware Indexing:** Incorporates path information to compute stable node coordinates.
- **Component-Level Mapping:** Indexes nodes and metadata per connected component.
- **Flexible Export:** Outputs binary `.cdx`, Zstandard-compressed `.cdx.zst`, or TSV format for debugging.

## Usage

```bash
cdx_builder <input.gbz> [OPTIONS]
```

### Examples

```bash
# Build a standard CDX index
cdx_builder graph.gbz

# Specify output file
cdx_builder graph.gbz -o graph.cdx

# Compressed output
cdx_builder graph.gbz -c 9

# TSV debug output
cdx_builder graph.gbz -d > debug.tsv

# More accurate relaxation
cdx_builder graph.gbz -t 0.001

# Highly tangled graph with sparse haplotype support
cdx_builder graph.gbz -l 0.3 -i 1000

# Well-supported graph with consistent haplotype structure
cdx_builder graph.gbz -l 0.95

# Display help
cdx_builder -h
```

## Command-line reference

| Option | Description |
|---|---|
| `<input.gbz>` | Path to the input GBZ pangenome graph (required). |
| `-i, --iteration INT` | Maximum relaxation iterations. Default: `100`. |
| `-t, --threshold FLOAT` | Convergence threshold; smaller values increase accuracy but may need more iterations. Default: `0.01`. |
| `-l, --lambda-anchor FLOAT` | Balance between path-derived coordinates (`1.0`) and topology smoothing (`0.0`), in `[0.0, 1.0]`. Recommended: `0.6`-`0.8`. Default: `0.7`. |
| `-o, --output FILE` | Output CDX file path. Default: `<input>.cdx`. |
| `-c, --compress LEVEL` | Write a Zstandard-compressed `.cdx.zst` file instead. Level `1`-`22`. Default (flag with no value): `3`. Mutually exclusive with `-d/--debug`. |
| `-d, --debug` | Write a human-readable TSV representation to stdout instead of a binary CDX file. Mutually exclusive with `-c/--compress`. |

Run `cdx_builder --help` (or, from the merged toolkit, `cdx <file.gbz> --help`)
for the authoritative, up-to-date list.

## Output files

| Output | Produced when | Description |
|---|---|---|
| `<input>.cdx` (or `-o` path) | default | Binary CDX index: per-component node coordinates, ready for `cdx_coverage`. |
| `<input>.cdx.zst` | `-c/--compress` | Same content, Zstandard-compressed for archival/transfer. |
| stdout (TSV) | `-d/--debug` | Human-readable table of the same data, for inspection/debugging - no binary file is written in this mode. |

## Requirements

### Bundled (fetched/built automatically by CMake - no manual steps required)

| Dependency    | Purpose                                   |
|---------------|--------------------------------------------|
| SDSL          | Succinct data structures                   |
| GBWT          | Haplotype-aware graph index                |
| GBWTGraph     | Graph representation on top of GBWT        |
| libhandlegraph| Graph interface used by libbdsg            |
| libbdsg       | Pangenome graph backend (GBZ support)      |
| CLI11         | Command-line argument parsing              |
| GoogleTest    | Unit test framework (only when building tests) |
| **cdx_lib**   | CDX format/IO code shared with `cdx_coverage` |

All of the above are fetched automatically at configure time via CMake's
`FetchContent`/`ExternalProject`, pinned to an exact commit each (see
`cmake/ExternalDeps.cmake`) - no git submodules, no manual install step.
`cdx_lib` is the one exception: it's a plain sibling directory (`../lib`) in
this monorepo, added via `add_subdirectory` - not fetched from anywhere.

### System dependencies (must be installed separately)

- CMake ≥ 3.16
- Ninja (or Make)
- A C++17-compatible compiler (GCC ≥ 11 or Apple Clang)
- OpenMP runtime
- OpenSSL development libraries
- zstd development libraries
- zlib development libraries
- pkg-config
- Boost
- Jansson
- Git

## Build

### Linux (Ubuntu 20.04+)

`cmake/install_ubuntu20.sh` installs every system package listed below
(including a newer GCC if needed), configures and builds this branch alone
with CMake+Ninja, and runs its unit test suite. Run it from this directory:

```bash
./cmake/install_ubuntu20.sh
```

Useful flags: `--no-tests` (build only, skip ctest), `--build-dir <dir>`,
`--build-type <type>`, `--jobs <N>`. Run `./cmake/install_ubuntu20.sh --help`
for the full list. The script is safe to re-run.

> For testing the *whole* `cdx` toolkit (all three branches together) on a
> bare Ubuntu 20.04 box or in Docker, use `scripts/test_ubuntu20.sh` at the
> repository root instead - see the top-level README.

#### Prerequisites (manual setup)

```bash
sudo apt update

sudo apt install -y \
    build-essential \
    cmake \
    ninja-build \
    pkg-config \
    libboost-all-dev \
    libjansson-dev \
    libssl-dev \
    libzstd-dev \
    zlib1g-dev \
    git
```

On GCC, OpenMP support (`libgomp`) ships with `build-essential`, so no extra package is
needed. Ubuntu 20.04's default `cmake` (3.16.3) satisfies the minimum requirement, but
its default `g++` (9.x) does not meet the `>= 11` requirement - install a newer one (e.g.
`sudo apt install g++-11`) and select it via `-DCMAKE_CXX_COMPILER=g++-11`.

#### Build

```bash
cmake -S . -B build-linux -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-linux -j$(nproc)
```

### macOS (Apple Silicon / ARM64)

#### Prerequisites

```bash
brew install \
    libomp \
    openssl@3 \
    zstd \
    cli11 \
    pkg-config \
    boost \
    jansson \
    git
```

Apple Clang does not ship an OpenMP runtime, so `libomp` from Homebrew is required. The
build automatically locates your Homebrew prefix via `brew --prefix` — no manual path
configuration is needed even if Homebrew isn't installed at the default `/opt/homebrew`.

#### Build

```bash
cmake -S . -B build-macos -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-macos
```

### Building this branch outside the `cdx` monorepo

`cdx_lib` normally resolves to the sibling `../lib` directory (this
monorepo's layout). To build `builder/` as a fully standalone checkout
elsewhere, point `CDX_LIB_DIR` at a separate `cdx_lib` checkout:

```bash
cmake -S . -B build -DCDX_LIB_DIR=/path/to/cdx_lib
```

### Testing

```bash
ctest --test-dir build --output-on-failure
```

Enabled by default (`CDX_BUILDER_BUILD_TESTS=ON`); pass
`-DCDX_BUILDER_BUILD_TESTS=OFF` to skip building the suite entirely.

### Troubleshooting

- **`cdx_lib not found at ...`** — point `CDX_LIB_DIR` at a valid `cdx_lib`
  checkout (see above), or build from inside the `cdx` monorepo where `../lib`
  resolves automatically.
- **`CMake ... or higher is required` coming from a dependency** — one of the
  fetched dependencies needs a newer CMake than you have installed.
- **OpenMP not found on macOS** — confirm `brew install libomp` succeeded and that
  `brew --prefix libomp` resolves to a valid path.
- **Stale build after a dependency update** — remove the build directory
  (`rm -rf build-linux` / `build-macos`) and reconfigure; `ExternalProject`-based
  dependencies (SDSL, GBWT, GBWTGraph) don't always pick up updates in place.

### Notes

- All bundled graph libraries (SDSL, GBWT, GBWTGraph, libhandlegraph, libbdsg) are
  compiled automatically during the build process — no dependency-specific `install.sh`
  scripts need to be run manually.
- `cdx_lib` is the only bundled dependency that is *not* built by an `ExternalProject`
  step; it's added directly via `add_subdirectory` from `../lib`.
- OpenMP support is enabled automatically when available.
- The root `CMakeLists.txt` is intentionally thin: compiler checks, macOS/Homebrew
  handling, third-party dependency resolution, and the GoogleTest fetch each live in
  their own file under `cmake/`, included from the root file in dependency order.
- `cmake/FetchGoogleTest.cmake` only fetches GoogleTest if a `gtest` target doesn't
  already exist: libbdsg's vendored copy of sdsl-lite unconditionally builds its own
  bundled googletest snapshot, so on most configurations that vendored copy is reused
  instead of building a second one.
