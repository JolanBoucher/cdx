# cdx

Single entry point for the CDX pangenome toolkit. One executable, mode picked
automatically from the input file(s) - no sub-command verbs to learn:

```
cdx graph.gbz                 build a CDX index from a GBZ graph      (builder/)
cdx index.cdx                 inspect the contents of a CDX index     (coverage/, implicit)
cdx index.cdx alignment.gam   compute GAM coverage against a CDX index (coverage/)
```

Run `cdx --help` for a summary, or `cdx <file> --help` (the file doesn't need
to exist, only its extension is used) for a mode's full option list.

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

`builder/` and `coverage/` each still also build their own standalone
executable and unit test suite, for branch-focused development - see their
own README.md for details.

Third-party dependencies each branch vendors for itself remain git
submodules, pointing at their real upstream repositories:

- `builder/deps/CLI11`, `builder/deps/libbdsg`, `builder/deps/gbwt`,
  `builder/deps/gbwtgraph`, `builder/deps/sdsl-lite`
- `coverage/deps/libvgio`

## Getting the source

```bash
git clone --recursive https://github.com/JolanBoucher/cdx.git
```

Already cloned without `--recursive`?

```bash
git submodule update --init --recursive
```

## Building

```bash
cmake -S . -B build
cmake --build build -j
```

The resulting `cdx` executable links `cdx_builder_core` and
`cdx_coverage_core` (static libraries built from `builder/` and
`coverage/`). See each branch's own README for its individual dependency
requirements (Boost/OpenSSL/zstd/Jansson/OpenMP for the builder branch;
libvgio, Protobuf, Abseil, HTSlib, Cairo, and optionally Python 3 for
circular graphs, for the coverage branch). `lib/` and CLI11 (vendored under
`builder/deps/CLI11`) have no further external dependencies.

Each branch's own unit test suite is off by default in this merged
configure (see the comment in the top-level `CMakeLists.txt` for why - a
googletest target-naming wrinkle when both are built together). Build/test
a branch in isolation from its own directory instead:

```bash
cd builder && cmake -S . -B build && cmake --build build -j && ctest --test-dir build
cd coverage && cmake -S . -B build && cmake --build build -j && ctest --test-dir build
```
