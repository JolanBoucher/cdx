/**
 * @file test_cdx_format.cpp
 * @brief Unit tests for the CdxFormat binary serialization utility class (cdx_format.h/.cpp).
 *
 * Covers:
 *  - Endianness conversion helpers (to/from little-endian, 32- and 64-bit).
 *  - Pack/unpack round trips for FileHeader, ComponentHeader, and NodeRecord,
 *    including boundary values (0 and the maximum representable value for
 *    each field width).
 *  - convert_node_records_to_native() on a batch of records.
 *  - The has_valid_magic() and has_valid_widths() validation helpers, on
 *    both valid and corrupted headers.
 *
 * All tests run on the host's native endianness. On a little-endian host
 * (the only platform exercised here), the to_little_endian / from_little_endian
 * helpers are identity functions; the round-trip tests below remain meaningful
 * regardless of host endianness because they only check that pack(unpack(x))
 * reproduces the original logical values, not the raw byte order.
 */

#include "../include/cdx_format.h"
#include "test_utils.h"

#include <array>
#include <cstdint>
#include <limits>

using namespace cdx;

/**
 * @brief CdxFormat::to_little_endian32 / from_little_endian32
 *
 * Converts a 32-bit value between native and little-endian byte order.
 *
 * Invariants:
 *  - from_little_endian32(to_little_endian32(x)) == x for every x (the
 *    conversion is an involution).
 *  - Both are noexcept and have no side effects.
 */
// Round-tripping arbitrary and boundary 32-bit values preserves the original value.
CDX_TEST(endian32_round_trip_preserves_value) {
    const std::array<uint32_t, 5> values = {
        0u, 1u, 0x12345678u, std::numeric_limits<uint32_t>::max(),
        std::numeric_limits<uint32_t>::max() / 2
    };
    for (const uint32_t value: values) {
        const uint32_t packed = CdxFormat::to_little_endian32(value);
        const uint32_t unpacked = CdxFormat::from_little_endian32(packed);
        CDX_ASSERT_EQ(unpacked, value);
    }
}

/**
 * @brief CdxFormat::to_little_endian64 / from_little_endian64
 *
 * Same contract as the 32-bit variants, for 64-bit values.
 */
// Round-tripping arbitrary and boundary 64-bit values preserves the original value.
CDX_TEST(endian64_round_trip_preserves_value) {
    const std::array<uint64_t, 5> values = {
        0ull, 1ull, 0x0123456789ABCDEFull, std::numeric_limits<uint64_t>::max(),
        std::numeric_limits<uint64_t>::max() / 2
    };
    for (const uint64_t value: values) {
        const uint64_t packed = CdxFormat::to_little_endian64(value);
        const uint64_t unpacked = CdxFormat::from_little_endian64(packed);
        CDX_ASSERT_EQ(unpacked, value);
    }
}

/**
 * @brief CdxFormat::convert_node_records_to_native
 *
 * Converts every NodeRecord in a contiguous array from little-endian wire
 * format to native byte order, in place.
 *
 * Invariants:
 *  - On a little-endian host, this is a logical no-op: the numeric value of
 *    every field is unchanged.
 *  - Applies to all `count` records in the array, not just the first one.
 *  - count == 0 is a valid no-op call.
 */
// Converting a batch of records leaves their logical field values unchanged on a little-endian host.
CDX_TEST(convert_node_records_to_native_preserves_values_on_little_endian_host) {
    static_assert(CdxFormat::IS_LITTLE_ENDIAN, "This test assumes a little-endian host.");

    std::array<NodeRecord, 3> records = {
        NodeRecord{1, 0, 100},
        NodeRecord{2, 1, 200},
        NodeRecord{3, 2, 300}
    };
    const std::array<NodeRecord, 3> expected = records;

    CdxFormat::convert_node_records_to_native(records.data(), records.size());

    for (std::size_t i = 0; i < records.size(); ++i) {
        CDX_ASSERT_EQ(records[i].node_id, expected[i].node_id);
        CDX_ASSERT_EQ(records[i].idx, expected[i].idx);
        CDX_ASSERT_EQ(records[i].seq_len, expected[i].seq_len);
    }
}

// Converting zero records is a valid no-op and must not crash or read out of bounds.
CDX_TEST(convert_node_records_to_native_handles_zero_count) {
    std::array<NodeRecord, 1> records = {NodeRecord{42, 0, 1}};
    CdxFormat::convert_node_records_to_native(records.data(), 0);
    CDX_ASSERT_EQ(records[0].node_id, static_cast<Nid>(42));
}

/**
 * @brief CdxFormat::pack_file_header / unpack_file_header
 *
 * pack_file_header serializes the fixed MAGIC/NID_WIDTH/SEQLEN_WIDTH
 * constants plus the given component count into FILE_HEADER_SIZE bytes.
 * unpack_file_header reverses this into a FileHeader with fields converted
 * to native byte order.
 *
 * Invariants:
 *  - unpack_file_header(pack_file_header(n)).n_components == n.
 *  - The packed magic always equals CdxFormat::MAGIC.
 *  - The packed widths always equal NID_WIDTH / SEQLEN_WIDTH.
 */
// Packing then unpacking a file header preserves the component count and fixed constants.
CDX_TEST(file_header_pack_unpack_round_trip) {
    std::array<char, CdxFormat::FILE_HEADER_SIZE> buffer{};
    constexpr uint32_t component_count = 7;

    CdxFormat::pack_file_header(buffer.data(), component_count);
    const FileHeader header = CdxFormat::unpack_file_header(buffer.data());

    CDX_ASSERT_EQ(header.n_components, component_count);
    CDX_ASSERT_TRUE(header.magic == CdxFormat::MAGIC);
    CDX_ASSERT_EQ(header.nid_width, CdxFormat::NID_WIDTH);
    CDX_ASSERT_EQ(header.seqlen_width, CdxFormat::SEQLEN_WIDTH);
}

// A component count of zero is a valid (if semantically empty) archive header and must round-trip.
CDX_TEST(file_header_pack_unpack_handles_zero_component_count) {
    std::array<char, CdxFormat::FILE_HEADER_SIZE> buffer{};
    CdxFormat::pack_file_header(buffer.data(), 0);
    const FileHeader header = CdxFormat::unpack_file_header(buffer.data());
    CDX_ASSERT_EQ(header.n_components, static_cast<uint32_t>(0));
}

// The maximum representable component count round-trips without truncation.
CDX_TEST(file_header_pack_unpack_handles_max_component_count) {
    std::array<char, CdxFormat::FILE_HEADER_SIZE> buffer{};
    constexpr uint32_t max_count = std::numeric_limits<uint32_t>::max();
    CdxFormat::pack_file_header(buffer.data(), max_count);
    const FileHeader header = CdxFormat::unpack_file_header(buffer.data());
    CDX_ASSERT_EQ(header.n_components, max_count);
}

/**
 * @brief CdxFormat::pack_component_header / unpack_component_header
 *
 * Serializes/deserializes a ComponentHeader's four fields
 * (n_records, node_id_min, node_id_max, name_size).
 *
 * Invariants:
 *  - Every field round-trips exactly, independently of the others.
 */
// Packing then unpacking a component header preserves all four fields.
CDX_TEST(component_header_pack_unpack_round_trip) {
    std::array<char, CdxFormat::COMPONENT_HEADER_SIZE> buffer{};

    CdxFormat::pack_component_header(buffer.data(), 1000, 5, 1004, 12);
    const ComponentHeader header = CdxFormat::unpack_component_header(buffer.data());

    CDX_ASSERT_EQ(header.n_records, static_cast<RecordCount>(1000));
    CDX_ASSERT_EQ(header.node_id_min, static_cast<Nid>(5));
    CDX_ASSERT_EQ(header.node_id_max, static_cast<Nid>(1004));
    CDX_ASSERT_EQ(header.name_size, static_cast<uint32_t>(12));
}

// Boundary values (zero and maximum) for every field round-trip without corruption.
CDX_TEST(component_header_pack_unpack_handles_boundary_values) {
    std::array<char, CdxFormat::COMPONENT_HEADER_SIZE> buffer{};
    constexpr uint64_t max64 = std::numeric_limits<uint64_t>::max();
    constexpr uint32_t max32 = std::numeric_limits<uint32_t>::max();

    CdxFormat::pack_component_header(buffer.data(), 0, max64, max64, max32);
    const ComponentHeader header = CdxFormat::unpack_component_header(buffer.data());

    CDX_ASSERT_EQ(header.n_records, static_cast<RecordCount>(0));
    CDX_ASSERT_EQ(header.node_id_min, static_cast<Nid>(max64));
    CDX_ASSERT_EQ(header.node_id_max, static_cast<Nid>(max64));
    CDX_ASSERT_EQ(header.name_size, max32);
}

/**
 * @brief CdxFormat::pack_node_record / unpack_node_record
 *
 * Serializes/deserializes a NodeRecord's three fields
 * (node_id, idx, seq_len).
 *
 * Invariants:
 *  - Every field round-trips exactly.
 */
// Packing then unpacking a node record preserves all three fields.
CDX_TEST(node_record_pack_unpack_round_trip) {
    std::array<char, CdxFormat::RECORD_SIZE> buffer{};

    CdxFormat::pack_node_record(buffer.data(), 42, 3, 250);
    const NodeRecord record = CdxFormat::unpack_node_record(buffer.data());

    CDX_ASSERT_EQ(record.node_id, static_cast<Nid>(42));
    CDX_ASSERT_EQ(record.idx, static_cast<Idx>(3));
    CDX_ASSERT_EQ(record.seq_len, static_cast<SeqLen>(250));
}

// Zero values for every field are valid and round-trip.
CDX_TEST(node_record_pack_unpack_handles_zero_values) {
    std::array<char, CdxFormat::RECORD_SIZE> buffer{};
    CdxFormat::pack_node_record(buffer.data(), 0, 0, 0);
    const NodeRecord record = CdxFormat::unpack_node_record(buffer.data());
    CDX_ASSERT_EQ(record.node_id, static_cast<Nid>(0));
    CDX_ASSERT_EQ(record.idx, static_cast<Idx>(0));
    CDX_ASSERT_EQ(record.seq_len, static_cast<SeqLen>(0));
}

/**
 * @brief CdxFormat::has_valid_magic
 *
 * Checks whether a FileHeader's magic bytes equal CdxFormat::MAGIC.
 *
 * Invariants:
 *  - Returns true only for an exact byte-for-byte match with MAGIC.
 *  - A single corrupted byte anywhere in the signature must return false.
 */
// A header built from pack_file_header always has valid magic bytes.
CDX_TEST(has_valid_magic_accepts_freshly_packed_header) {
    std::array<char, CdxFormat::FILE_HEADER_SIZE> buffer{};
    CdxFormat::pack_file_header(buffer.data(), 1);
    const FileHeader header = CdxFormat::unpack_file_header(buffer.data());
    CDX_ASSERT_TRUE(CdxFormat::has_valid_magic(header));
}

// A single corrupted byte in the magic signature is rejected.
CDX_TEST(has_valid_magic_rejects_corrupted_signature) {
    FileHeader header{};
    header.magic = CdxFormat::MAGIC;
    header.magic[0] = 'X'; // Corrupt the first magic byte.
    CDX_ASSERT_TRUE(!CdxFormat::has_valid_magic(header));
}

/**
 * @brief CdxFormat::has_valid_widths
 *
 * Checks whether a FileHeader declares the supported nid_width/seqlen_width.
 *
 * Invariants:
 *  - Returns true only when both widths exactly match NID_WIDTH and
 *    SEQLEN_WIDTH.
 *  - Returns false if either width alone is wrong.
 */
// A header built from pack_file_header always declares valid widths.
CDX_TEST(has_valid_widths_accepts_freshly_packed_header) {
    std::array<char, CdxFormat::FILE_HEADER_SIZE> buffer{};
    CdxFormat::pack_file_header(buffer.data(), 1);
    const FileHeader header = CdxFormat::unpack_file_header(buffer.data());
    CDX_ASSERT_TRUE(CdxFormat::has_valid_widths(header));
}

// An unsupported nid_width alone is enough to invalidate the header.
CDX_TEST(has_valid_widths_rejects_wrong_nid_width) {
    FileHeader header{};
    header.magic = CdxFormat::MAGIC;
    header.nid_width = CdxFormat::NID_WIDTH + 1;
    header.seqlen_width = CdxFormat::SEQLEN_WIDTH;
    CDX_ASSERT_TRUE(!CdxFormat::has_valid_widths(header));
}

// An unsupported seqlen_width alone is enough to invalidate the header.
CDX_TEST(has_valid_widths_rejects_wrong_seqlen_width) {
    FileHeader header{};
    header.magic = CdxFormat::MAGIC;
    header.nid_width = CdxFormat::NID_WIDTH;
    header.seqlen_width = CdxFormat::SEQLEN_WIDTH + 1;
    CDX_ASSERT_TRUE(!CdxFormat::has_valid_widths(header));
}

/**
 * @brief Compile-time layout guarantees relied upon by the tests above.
 *
 * These mirror the static_asserts already present in cdx_format.h; they are
 * re-checked here (as runtime assertions) purely so a layout regression
 * shows up as a normal test failure in this file's report, in addition to a
 * compile error at the header.
 */
// The packed struct sizes match the documented on-disk footprints.
CDX_TEST(packed_struct_sizes_match_documented_footprint) {
    CDX_ASSERT_EQ(sizeof(FileHeader), static_cast<std::size_t>(10));
    CDX_ASSERT_EQ(sizeof(ComponentHeader), static_cast<std::size_t>(28));
    CDX_ASSERT_EQ(sizeof(NodeRecord), static_cast<std::size_t>(16));
    CDX_ASSERT_EQ(CdxFormat::FILE_HEADER_SIZE, sizeof(FileHeader));
    CDX_ASSERT_EQ(CdxFormat::COMPONENT_HEADER_SIZE, sizeof(ComponentHeader));
    CDX_ASSERT_EQ(CdxFormat::RECORD_SIZE, sizeof(NodeRecord));
}

int main() {
    return cdx_test::run_all("cdx_format");
}
