# cdx_lib

## Overview

`cdx_lib` is a small C++17 static library for reading **CDX archives**: a
compact binary format storing graph nodes (identifier, local index, sequence
length) grouped into named components, such as chromosomes/contigs in a
genome graph. It only implements reading and binary deserialization, and has
no external dependencies beyond the C++17 standard library.

A CDX archive is a flat binary file laid out as:

```
[FileHeader]
[ComponentHeader][ComponentName][NodeRecord × n]   (component 0)
[ComponentHeader][ComponentName][NodeRecord × n]   (component 1)
...
```

- `FileHeader` (10 bytes): magic signature, component count, and the byte
  widths used for node IDs and sequence lengths.
- `ComponentHeader` (28 bytes): record count, min/max node ID, and the size
  of the component's name string that follows it.
- `NodeRecord` (16 bytes): `node_id`, local `idx`, and `seq_len`.

All multi-byte fields are stored little-endian; `CdxFormat` handles the
conversion to/from the host's native byte order.

## Features

- Sequential or random-access (seekable) reading of components by ID.
- Validated header parsing: magic signature and field-width checks up front,
  before any payload is touched.
- Endianness-safe decoding on any host byte order.
- Strict truncation/overflow checking on every read - malformed or cut-off
  archives raise an exception rather than reading garbage.
- No external dependencies: standard library only.

## Usage

```cpp
#include "cdx_IO.h"
#include <fstream>

std::ifstream in("graph.cdx", std::ios::binary);

// Global header: component count, ID/length field widths.
cdx::FileHeader header = cdx::readGlobalHeader(in);

// Random access: jump straight to component 2 (skips 0 and 1).
cdx::ComponentInfo info = cdx::seekComponent(in, /*component_id=*/2);

// Stream is now positioned at that component's first NodeRecord.
std::vector<cdx::NodeRecord> nodes =
    cdx::readComponentPayload(in, info.record_count);

for (const cdx::NodeRecord &n : nodes) {
    // n.node_id, n.idx, n.seq_len
}
```

Reading components in file order instead (no seeking) works the same way,
just call `readComponentHeader`/`readComponentPayload` for each component
in sequence rather than `seekComponent`.

## Command-line reference

Not applicable - `cdx_lib` is a library with no executable or CLI of its
own. It's linked into `cdx_builder`/`cdx_coverage` (see their own READMEs)
and into its own unit test executables (see [Build](#build) below).

## Output files

Not applicable - `cdx_lib` only reads existing `.cdx` archives; it never
writes files itself. See `builder/README.md` for how `.cdx` files are
produced.

## API overview

- `cdx::readGlobalHeader(input)` — reads and validates the archive's `FileHeader`.
- `cdx::readComponentHeader(input, component_id)` — reads one component's header and name.
- `cdx::seekComponent(input, component_id)` — locates a component by ID in a seekable stream.
- `cdx::readComponentPayload(input, record_count)` — reads and decodes a component's `NodeRecord`s.
- `cdx::CdxFormat` — static utility class with the `pack_*` / `unpack_*` (de)serialization
  routines, endianness helpers, and header validation (`has_valid_magic`, `has_valid_widths`).

All reading functions throw `std::runtime_error` (or `std::out_of_range` /
`std::overflow_error` where noted) on malformed or truncated input; see the
docstrings in `include/cdx_IO.h` for the exact contract of each function.

## Requirements

- A C++17 compiler.
- CMake ≥ 3.16.
- No third-party dependencies otherwise.

## Build

```bash
cmake -S . -B build
cmake --build build
```

This builds the `cdx_lib` static library and the test executables.

### Testing

```bash
ctest --test-dir build --output-on-failure
```

Each `tests/test_*.cpp` file is its own standalone executable using a
lightweight custom harness (`tests/test_utils.h`) — no external test
framework is required. Building without `NDEBUG` (the default with CMake's
`Debug` configuration) also enables a few extra structural invariant checks
in `readComponentPayload`.

## Layout

```
include/
  cdx_types.h    Shared type aliases and plain data structures (no behavior).
  cdx_format.h   Binary layout, static assertions, and CdxFormat (pack/unpack, endianness).
  cdx_IO.h       Archive reading: headers, component lookup, payload decoding.
src/
  cdx_format.cpp
  cdx_IO.cpp
tests/
  test_cdx_types.cpp   Unit tests for cdx_types.h
  test_cdx_format.cpp  Unit tests for cdx_format.h/.cpp
  test_cdx_IO.cpp      Unit tests for cdx_IO.h/.cpp
  test_utils.h         Minimal, dependency-free test harness shared by all three files.
```
