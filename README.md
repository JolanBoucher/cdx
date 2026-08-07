# cdx

## Overview

Entry point for the CDX vgan tool. One executable, mode picked
automatically from the input file(s)' real binary signature - not the file
extension - so `cdx <file> [file2] [OPTIONS]` is the only interface to
learn, regardless of which mode ends up running:

```
cdx graph.gbz                 build a CDX index from a GBZ graph
cdx index.cdx                 inspect the contents of a CDX index (implicit)
cdx index.cdx alignment.gam   compute GAM coverage against a CDX index
```

What used to be three separate repositories now live directly here as a
monorepo, developed and committed together:

```
cdx/
├── src/        dispatcher (sniffs the input file's binary signature, routes
│               to the right mode - see src/main.cpp, src/sniff.h/.cpp)
├── lib/        CDX format types and binary I/O, shared by build and coverage
├── builder/    build mode: GBZ -> CDX index construction
└── coverage/   coverage/inspect mode: GAM coverage computation/reporting
```

This one document covers all three - there's no separate README per
subdirectory to keep in sync.

## Features

- **Automatic mode dispatch** from the real binary signature of the input
  file(s), never the extension.
- **Detailed, per-mode `--help`**: `cdx --help` (no file) prints each
  mode's full, real option list in its own labeled section - not a
  generic cheat-sheet. `cdx <file> --help` (file doesn't need to exist,
  only its extension is used) shows a single mode's options.
- **Zero manual dependency setup**: every third-party dependency is fetched
  automatically by CMake, pinned to an exact commit each. No git
  submodules, nothing to install by hand beyond system packages (see
  [Requirements](#requirements)).
- **Portability-tested on Ubuntu 20.04**, including in a disposable Docker
  container - see [Portability testing](#portability-testing-ubuntu-2004).

### Build mode

- **GBZ native**: direct loading and validation of GBWT/GBZ pangenome graphs.
- **Haplotype-aware indexing**: incorporates path information to compute
  stable node coordinates.
- **Component-level mapping**: indexes nodes and metadata per connected
  component.
- **Flexible export**: binary `.cdx`, Zstandard-compressed `.cdx.zst`, or
  TSV for debugging.

### Coverage / inspect mode

- **Implicit inspect mode**: give just a CDX file (no GAM) to list all
  components, or add `-q/--query COMPONENT` to describe a single one - no
  alignment processing happens in this mode.
- **Two query modes for coverage**: whole-graph (all components at once) or
  a single component/sub-region (`-q/--query`), by name or numeric ID.
- **Two plot styles**: linear tracks (default) or circular (Circos-style)
  genome plots, each with a single-panel view for one component/region and
  a multi-panel grid view for the whole graph.
- **Linear, in-process rendering** via Cairo - no subprocess, no external
  interpreter.
- **Multi-core throughout**: GAM decompression, coordinate projection, and
  multi-panel plot rendering are all parallelized.
- **Base-pair-precision coverage** by default, correcting for deletions and
  reads that start/end partway through a node; `-p node` opts into cheaper
  whole-node resolution for memory-constrained machines or very
  large/deep GAM files.
- **Linear or logarithmic coverage scale**, configurable smoothing,
  downsampling, resolution, figure size, and colors.

## Usage

### Build mode

```bash
cdx <input.gbz> [OPTIONS]
```

```bash
# Build a standard CDX index
cdx graph.gbz

# Specify output file
cdx graph.gbz -o graph.cdx

# Compressed output
cdx graph.gbz -c 9

# TSV debug output
cdx graph.gbz -d > debug.tsv

# More accurate relaxation
cdx graph.gbz -t 0.001

# Highly tangled graph with sparse haplotype support
cdx graph.gbz -l 0.3 -i 1000
```

### Coverage / inspect mode

```bash
cdx <index.cdx> [alignment.gam] [OPTIONS]
```

```bash
# Inspect a CDX index - list all components, no alignments processed
cdx graph.cdx

# Describe a single component
cdx graph.cdx -q chr1

# Whole-graph coverage - every component, multi-panel grid plot
cdx graph.cdx reads.gam -o results/

# Single component or sub-region
cdx graph.cdx reads.gam -q chr1               -o results/
cdx graph.cdx reads.gam -q "chr1 1000:5000"   -o results/

# Circular plot
cdx graph.cdx reads.gam -q chr1 -c circular -o results/

# Logarithmic scale, custom styling
cdx graph.cdx reads.gam -q chr1 --log 10 \
    --color-line "#1E3A8A" --color-filling "#93C5FD" \
    --fig-size 7x4.5 --dpi 300 -o results/
```

## Command-line reference

`cdx` itself only ever looks at the leading file argument(s) to pick a
mode, then hands the full, unmodified command line to that mode's own
parser.

### Build mode

| Option | Description |
|---|---|
| `<input.gbz>` | Path to the input GBZ pangenome graph (required). |
| `-i, --iteration INT` | Maximum relaxation iterations. Default: `100`. |
| `-t, --threshold FLOAT` | Convergence threshold; smaller values increase accuracy but may need more iterations. Default: `0.01`. |
| -l, --lambda-anchor FLOAT | Balance: Trade-off between path-derived coordinates (1.0) and topology smoothing (0.0).<br><br>Valid Range: [0.0, 1.0]<br><br>Recommendations:<br>• Standard Graphs: 0.6 – 0.8 (Default: 0.7).<br>• Complex Graphs (10+ paths): Use values closer to 1.0 (path coordinates are more representative).<br>• Discontinuous/Messy Graphs: Use values < 0.5 for better stability. |
| `-o, --output FILE` | Output CDX file path. Default: `<input>.cdx`. |
| `-c, --compress LEVEL` | Write a Zstandard-compressed `.cdx.zst` file instead. Level `1`-`22`. Default (flag with no value): `3`. Mutually exclusive with `-d/--debug`. |
| `-d, --debug` | Write a human-readable TSV representation to stdout instead of a binary CDX file. Mutually exclusive with `-c/--compress`. |

### Coverage / inspect mode

| Option | Description |
|---|---|
| `<CDX>` | Path to the binary CDX graph index (required). |
| `[GAM]` | Path to the GAM alignment file. Omit it to inspect the CDX index instead of computing coverage. |
| `-q, --query TEXT` | Coverage mode: scope the computation to one component, `COMPONENT` or `"COMPONENT START:END"` (0-based), by name or numeric ID - omit to process the whole graph. Inspect mode (no GAM given): selects which single component to describe; any range is ignored. |
| `-c, --component-type` | `linear`/`l` (default) or `circular`/`c`. |
| `-p, --coverage-precision` | `base`/`b` (default): per-base-pair coverage. `node`/`n`: cheaper whole-node resolution. |
| `-o, --output PATH` | Output directory. Default: `.` |
| `--no-graph` / `--no-stats` / `--no-table` | Skip the graph / statistics report / TSV table respectively. |
| `--log [BASE]` | Logarithmic coverage scale; base defaults to 10. |
| `--smoothing FLOAT` | Moving-average window, as a fraction of length, in `[0.0, 1.0]`. Default: `0.01`. |
| `--max-point, --max-points N` | Max points passed to the plotting backend; `0` disables downsampling. Default: `10000`. |
| `--dpi N` | Output graph resolution. Default: `300`. |
| `--fig-size WIDTHxHEIGHT` | Figure size in inches, e.g. `7x4.5`. |
| `--color-line HEX` / `--color-filling HEX` | Line/fill color. Defaults: `#1E3A8A` / `#93C5FD`. |
| `-t, --worker-threads N` | Threads used for computation. Default: `auto` (all cores). |
| `-T, --decompression-threads N` | Threads used for GAM decompression. Default: `auto` (half the cores). |

Run `cdx <file> --help` for the authoritative, up-to-date list for either mode.

## Output files

Inspect mode writes no files - terminal output only.

### Build mode

| Output | Produced when | Description |
|---|---|---|
| `<input>.cdx` (or `-o` path) | default | Binary CDX index, ready for coverage/inspect mode. |
| `<input>.cdx.zst` | `-c/--compress` | Same content, Zstandard-compressed. |
| stdout (TSV) | `-d/--debug` | Human-readable table; no binary file is written in this mode. |

### Coverage mode

Written to the directory given by `-o/--output` (unless disabled):

| File | Description |
|---|---|
| `coverage_profile.tsv` | Per-base coverage table: `component_name`, `position`, `coverage`. |
| `coverage_stats.txt` | Mapping statistics and coverage statistics (breadth, mean, median, stddev, quartiles, min/max) per component or queried region. |
| `coverage_graph.png` | The rendered coverage plot (linear or circular, single panel or multi-panel grid). |

## Requirements

- A C++17 compiler (GCC ≥ 11 or Apple Clang)
- CMake ≥ 3.16
- pkg-config
- Build mode: OpenMP, OpenSSL, zstd, Jansson, Boost
- Coverage/inspect mode: Protobuf, HTSlib ≥ 1.10, Cairo, Abseil
- Optional: Python 3 (only for circular graph rendering at runtime)

Everything else is fetched automatically by CMake at configure time
(`FetchContent`/`ExternalProject`, pinned to an exact commit each) - CLI11,
libbdsg, GBWT, GBWTGraph, sdsl-lite, libvgio, and GoogleTest (shared across
all three test suites, see [Testing](#testing)). Abseil is the one
conditional case: `find_package(absl CONFIG QUIET)` tries the system
package first (e.g. Homebrew on macOS) and only falls back to fetching a
pinned release if none is found - which matters specifically on Ubuntu
20.04, whose default GCC (9.4, too old - `>= 11` required) and repos
(`libabsl-dev` doesn't exist before Ubuntu 22.04) both need workarounds;
see [Portability testing](#portability-testing-ubuntu-2004).

`lib/` (CDX archive format types and binary I/O, shared by build and
coverage) has no dependencies beyond the C++17 standard library.

## Build

### Getting the source

```bash
git clone https://github.com/JolanBoucher/cdx.git
```

No `--recursive`/submodules needed - everything gets fetched automatically
on first `cmake` configure, below.

### Building

```bash
cmake -S . -B build
cmake --build build -j
```

The resulting `cdx` executable links `cdx_builder_core` and
`cdx_coverage_core` (static libraries built from `builder/` and
`coverage/`). To install system-wide: `cmake --install build`.

CLion users: `CMakePresets.json` at the repo root defines two ready-made
profiles (auto-detected by CLion - pick one from the profile selector):

- **`default`** - lib + builder + coverage test suites all enabled.
- **`fast`** - test suites off, for quicker iteration on `cdx` itself.

### Testing

```bash
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure
```

`lib`'s tests always build; `builder`'s and `coverage`'s build whenever
`CDX_BUILDER_BUILD_TESTS`/`CDX_BUILD_TESTS` are `ON` (the default here).
All three suites share a single GoogleTest build rather than fetching one
each: `builder`'s own dependency chain (`builder/deps/libbdsg`'s vendored
sdsl-lite) already vendors a modern googletest copy, and the top-level
`CMakeLists.txt` bridges it to the namespaced `GTest::` alias `coverage`'s
tests need, right after `add_subdirectory(builder)`.

`builder/`, `coverage/`, and `lib/` are also each independently
configurable/buildable/testable from their own directory:

```bash
cd builder  && cmake -S . -B build && cmake --build build -j && ctest --test-dir build
cd coverage && cmake -S . -B build && cmake --build build -j && ctest --test-dir build
cd lib      && cmake -S . -B build && cmake --build build -j && ctest --test-dir build
```

(`builder`/`coverage` built this way normally resolve `cdx_lib` to the
sibling `../lib` directory; to build one fully outside this monorepo, point
`-DCDX_LIB_DIR=/path/to/cdx_lib` at a separate checkout instead.)

### Platform-specific setup

#### macOS (Apple Silicon or Intel, via Homebrew)

```bash
brew install cmake pkg-config protobuf abseil htslib libomp cairo cli11 \
    openssl@3 zstd boost jansson git
```

Apple Clang does not ship an OpenMP runtime, so `libomp` from Homebrew is
required; the build locates your Homebrew prefix automatically.

#### Ubuntu 20.04+

```bash
sudo apt update
sudo apt install -y \
    build-essential g++-11 cmake git pkg-config \
    libboost-all-dev libjansson-dev libssl-dev libzstd-dev zlib1g-dev \
    protobuf-compiler libprotobuf-dev libhts-dev libcairo2-dev
```

Ubuntu 20.04's default `g++` (9.x) doesn't meet the `>= 11` requirement -
install `g++-11` as above and select it with
`-DCMAKE_CXX_COMPILER=g++-11`. Abseil needs no manual install/build even on
20.04 (see [Requirements](#requirements)). If `libcli11-dev` isn't
available in your repositories, install CLI11 as a single header instead:

```bash
mkdir -p include/CLI
curl -L -o include/CLI/CLI.hpp \
    https://github.com/CLIUtils/CLI11/releases/latest/download/CLI11.hpp
```

Or skip all of the above and use the script below, which does it for you.

### Portability testing (Ubuntu 20.04)

`scripts/test_ubuntu20.sh` is a standalone shell script (not
Docker-specific - works the same in a container, VM, or CI runner) that
installs every package above on a bare Ubuntu 20.04 box, builds `cdx`, and
sanity-checks it. Pass `-test`/`--test` to also build and run all three
unit test suites:

```bash
./scripts/test_ubuntu20.sh          # build + sanity check only
./scripts/test_ubuntu20.sh --test   # + build and run every test suite
```

`Dockerfile` wraps this in a disposable, reproducible `ubuntu:20.04`
(amd64) image - useful for testing from a machine that isn't already
Ubuntu 20.04:

```bash
docker build --platform=linux/amd64 -t cdx-ubuntu20-test .
docker run --rm --platform=linux/amd64 cdx-ubuntu20-test ./scripts/test_ubuntu20.sh --test
```

### Troubleshooting

- **`cdx_lib not found at ...`** — point `CDX_LIB_DIR` at a valid `cdx_lib`
  checkout, or build from inside this monorepo where `../lib` resolves
  automatically.
- **`GCC/G++ 11 or newer is required`** — install a newer GCC (see
  [Ubuntu 20.04+](#ubuntu-2004) above) and pass
  `-DCMAKE_CXX_COMPILER=g++-11`.
- **`find_package(absl)` / Abseil errors** — should self-resolve via the
  `FetchContent` fallback (see [Requirements](#requirements)); if it
  doesn't, check for network access at configure time.
- **OpenMP not found on macOS** — confirm `brew install libomp` succeeded
  and that `brew --prefix libomp` resolves to a valid path.
- **Stale build after a dependency bump** — remove the build directory and
  reconfigure; `ExternalProject`-based dependencies don't always pick up
  updates in place.

## Internals

- **`lib/`** - CDX archive format (binary, little-endian: `FileHeader` +
  per-component `ComponentHeader`/name/`NodeRecord`s) and its C++ reader
  API (`cdx::readGlobalHeader`, `readComponentHeader`, `seekComponent`,
  `readComponentPayload`, `CdxFormat`). Read-only, no external
  dependencies. See `lib/include/cdx_IO.h` for full doc comments.
- **`builder/`** - pipeline: load GBZ → partition into connected components
  → relax node coordinates using path/topology weights → assign continuous
  local coordinates per component → serialize.
- **`coverage/`** - given a CDX index (+ optional GAM), resolves the
  requested component(s), computes/projects coverage (or, with no GAM,
  just lists/describes components), and writes the requested outputs.
  Circular plots are rendered by a Python subprocess
  (`coverage/python_script/circular_plot.py`, via
  [pycirclize](https://github.com/moshi4/pyCirclize)); a private venv for
  it is auto-provisioned next to the built executable, best-effort, never
  failing the build - see
  `coverage/python_script/CIRCULAR_PLOT_SETUP.md`.
