# cdx

Entry point for the CDX vgan tool. One executable, mode picked
automatically from the input file(s) - no sub-command verbs to learn:

```
cdx graph.gbz                 build a CDX index from a GBZ graph       (builder/)
cdx index.cdx                 inspect the contents of a CDX index      (coverage/, implicit)
cdx index.cdx alignment.gam   compute GAM coverage against a CDX index (coverage/)
```

Run `cdx --help` for a detailed, per-mode option list (build / coverage /
inspect, each in its own section), or `cdx <file> --help` (the file doesn't
need to exist, only its extension is used) for a single mode's options.

## Layout

This is a monorepo: what used to be three separate repositories now live
directly here, developed and committed together.

```
cdx/
├── src/        dispatcher (sniffs the input file's binary signature, routes
│               to the right branch - see src/main.cpp, src/sniff.h/.cpp)
├── lib/        CDX format types and binary I/O, shared by both branches
├── builder/    GBZ -> CDX index construction
└── coverage/   CDX inspection + GAM coverage computation/reporting
```

Each subdirectory has its own README with branch-specific detail - options
reference, output files, internals, standalone dependency list, etc.:

- **[lib/README.md](lib/README.md)** - CDX archive format, API overview.
- **[builder/README.md](builder/README.md)** - linearization pipeline,
  full CLI options/examples, features.
- **[coverage/README.md](coverage/README.md)** - coverage/inspection CLI
  reference, output files, project layout.

> Heads up: those three READMEs mostly predate this monorepo merge, so parts
> of them (git submodule instructions in particular) describe how to build
> each branch as its own *standalone* repository - still valid if you `cd`
> into one and configure it on its own (see [Testing a single
> branch](#testing-a-single-branch) below), but **not** how dependencies are
> fetched when building `cdx` as a whole from this top-level directory (see
> [Building](#building) below instead).

Third-party dependencies are auto-fetched by CMake at configure time
(`FetchContent`/`ExternalProject`, pinned to an exact commit each) - no git
submodules, nothing to install by hand beyond system packages:

- `builder/`: CLI11, libbdsg, GBWT, GBWTGraph, sdsl-lite
- `coverage/`: libvgio, and (only if no system Abseil is found) Abseil

## Getting the source

```bash
git clone https://github.com/JolanBoucher/cdx.git
```

No `--recursive`/submodules needed - everything gets fetched automatically
on first `cmake` configure (see [Building](#building)).

## Building

```bash
cmake -S . -B build
cmake --build build -j
```

The resulting `cdx` executable links `cdx_builder_core` and
`cdx_coverage_core` (static libraries built from `builder/` and
`coverage/`). System dependencies you do need installed: a C++17 compiler
(GCC ≥ 11 or Apple Clang), CMake ≥ 3.16, OpenMP, OpenSSL, zstd, Jansson,
Boost, pkg-config, Protobuf, HTSlib ≥ 1.10, Cairo, and optionally Python 3
(only for circular graph rendering at runtime). See each branch's own
README (linked above) for the full rationale per dependency.

CLion users: `CMakePresets.json` at the repo root defines two ready-made
profiles (auto-detected by CLion - pick one from the profile selector):

- **`default`** - lib + builder + coverage test suites all enabled.
- **`fast`** - test suites off, for quicker iteration on `cdx` itself.

### Testing

`lib`'s tests always build; `builder`'s and `coverage`'s build whenever
`CDX_BUILDER_BUILD_TESTS`/`CDX_BUILD_TESTS` are `ON` (the default in this
merged configure - all three suites share a single GoogleTest build, no
separate fetch per branch):

```bash
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure
```

#### Testing a single branch

Each of `lib/`, `builder/`, `coverage/` is also independently configurable
and buildable/testable from its own directory (their own READMEs, linked
above, cover this in more detail, including their own dependency lists):

```bash
cd builder && cmake -S . -B build && cmake --build build -j && ctest --test-dir build
cd coverage && cmake -S . -B build && cmake --build build -j && ctest --test-dir build
cd lib && cmake -S . -B build && cmake --build build -j && ctest --test-dir build
```

## Portability testing (Ubuntu 20.04)

`scripts/test_ubuntu20.sh` is a standalone shell script (not Docker-specific
- works the same in a container, VM, or CI runner) that installs the exact
system packages needed on a bare Ubuntu 20.04 box, builds `cdx`, and
sanity-checks it. Pass `-test`/`--test` to also build and run all three unit
test suites:

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
