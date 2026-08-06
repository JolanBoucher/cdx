# cdx_lib

`cdx_lib` is a small C++17 static library for reading **CDX archives**: a
compact binary format storing graph nodes (identifier, local index, sequence
length) grouped into named components, such as chromosomes/contigs in a
genome graph.

The library only implements reading and binary (de)serialization. It has no
external dependencies beyond the C++17 standard library.

## Archive format

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

## Building

```bash
cmake -S . -B build
cmake --build build
```

This builds the `cdx_lib` static library and the test executables.

## Testing

```bash
ctest --test-dir build --output-on-failure
```

Each `tests/test_*.cpp` file is its own standalone executable using a
lightweight custom harness (`tests/test_utils.h`) — no external test
framework is required. Building without `NDEBUG` (the default with CMake's
`Debug` configuration) also enables a few extra structural invariant checks
in `readComponentPayload`.
