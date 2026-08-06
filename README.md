# cdx

Single entry point for the CDX pangenome toolkit. One executable, mode picked
automatically from the input file(s):

```
cdx graph.gbz                 build a CDX index from a GBZ graph      (cdx_builder)
cdx index.cdx                 inspect the contents of a CDX index     (cdx_coverage, implicit)
cdx index.cdx alignment.gam   compute GAM coverage against a CDX index (cdx_coverage)
```

Run `cdx --help` for a summary, or `cdx <file> --help` (file doesn't need to
exist, only its extension is used) for a mode's full option list.

## Getting the source

`cdx_builder` and `cdx_coverage` are linked in as git submodules:

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
`cdx_coverage_core` (static libraries produced by the two submodules) - see
their own repositories for their individual dependency requirements
(libbdsg/GBWT/GBWTGraph/CLI11/cdx_lib for the builder branch; libvgio,
Protobuf, Abseil, Cairo, CLI11, cdx_lib, and optionally Python 3 for
circular graphs, for the coverage branch).

Each submodule can still be built and tested standalone (`cdx_builder` /
`cdx_coverage` executables + their own `ctest` suites) from inside its own
directory, independently of this merged project.
