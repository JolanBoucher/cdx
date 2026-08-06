/**
 * @file test_cdx_IO.cpp
 * @brief Unit tests for the CDX archive reading functions (cdx_IO.h/.cpp).
 *
 * Each test builds a small, hand-crafted binary CDX archive in memory (as a
 * std::string of raw bytes fed through a std::istringstream) using
 * CdxFormat::pack_* to stay in sync with the on-disk layout, then exercises
 * one of readGlobalHeader / readComponentHeader / seekComponent /
 * readComponentPayload against it.
 *
 * Coverage focuses on:
 *  - the documented happy path for each function, including the exact
 *    stream position left behind on success;
 *  - every explicitly documented error condition (truncation, invalid
 *    magic/widths, empty archive, invalid node-ID range, empty/embedded-NUL
 *    component name, out-of-range component ID, truncated payload);
 *  - the debug-only structural invariants checked by readComponentPayload
 *    (zero sequence length, out-of-range local index, non-strictly-ordered
 *    node IDs), which only apply in builds without NDEBUG defined.
 *
 * A note on debug-only checks: the record-level invariant checks in
 * readComponentPayload are compiled out under NDEBUG (release builds). The
 * tests for those cases are themselves guarded by `#ifndef NDEBUG` so this
 * file behaves correctly (and still compiles) in both build configurations.
 */

#include "cdx_IO.h"
#include "../include/cdx_format.h"
#include "../include/cdx_types.h"
#include "test_utils.h"

#include <sstream>
#include <stdexcept>
#include <string>

using namespace cdx;

namespace {
    /// Appends `count` raw bytes from `buffer` to `out`.
    void appendBytes(std::string &out, const char *buffer, std::size_t count) {
        out.append(buffer, count);
    }

    /// Builds and appends a serialized FileHeader for `component_count` components.
    void appendFileHeader(std::string &out, uint32_t component_count) {
        std::array<char, CdxFormat::FILE_HEADER_SIZE> buffer{};
        CdxFormat::pack_file_header(buffer.data(), component_count);
        appendBytes(out, buffer.data(), buffer.size());
    }

    /// Builds and appends a serialized ComponentHeader (without the following name/payload).
    void appendComponentHeader(
        std::string &out,
        RecordCount n_records,
        Nid node_id_min,
        Nid node_id_max,
        uint32_t name_size
    ) {
        std::array<char, CdxFormat::COMPONENT_HEADER_SIZE> buffer{};
        CdxFormat::pack_component_header(buffer.data(), n_records, node_id_min, node_id_max, name_size);
        appendBytes(out, buffer.data(), buffer.size());
    }

    /// Builds and appends a single serialized NodeRecord.
    void appendNodeRecord(std::string &out, Nid node_id, Idx idx, SeqLen seq_len) {
        std::array<char, CdxFormat::RECORD_SIZE> buffer{};
        CdxFormat::pack_node_record(buffer.data(), node_id, idx, seq_len);
        appendBytes(out, buffer.data(), buffer.size());
    }

    /**
     * @brief Builds a minimal, fully valid single-component CDX archive.
     *
     * Layout: FileHeader(n_components=1) + ComponentHeader + name "compA" +
     * one NodeRecord{node_id=1, idx=0, seq_len=100}.
     */
    std::string buildValidSingleComponentArchive() {
        std::string bytes;
        appendFileHeader(bytes, 1);
        appendComponentHeader(bytes, /*n_records=*/1, /*nid_min=*/1, /*nid_max=*/1, /*name_size=*/5);
        bytes += "compA";
        appendNodeRecord(bytes, /*node_id=*/1, /*idx=*/0, /*seq_len=*/100);
        return bytes;
    }

    /**
     * @brief Builds a valid two-component CDX archive.
     *
     * Component 0: name "compA", node IDs [1, 1], 1 record.
     * Component 1: name "compB", node IDs [2, 3], 2 strictly-ordered records.
     */
    std::string buildValidTwoComponentArchive() {
        std::string bytes;
        appendFileHeader(bytes, 2);

        appendComponentHeader(bytes, /*n_records=*/1, /*nid_min=*/1, /*nid_max=*/1, /*name_size=*/5);
        bytes += "compA";
        appendNodeRecord(bytes, /*node_id=*/1, /*idx=*/0, /*seq_len=*/100);

        appendComponentHeader(bytes, /*n_records=*/2, /*nid_min=*/2, /*nid_max=*/3, /*name_size=*/5);
        bytes += "compB";
        appendNodeRecord(bytes, /*node_id=*/2, /*idx=*/0, /*seq_len=*/50);
        appendNodeRecord(bytes, /*node_id=*/3, /*idx=*/1, /*seq_len=*/60);

        return bytes;
    }
} // namespace

/**
 * @brief readGlobalHeader
 *
 * Reads and validates the FileHeader at the current stream position.
 *
 * Invariants:
 *  - On success, returns a FileHeader in native byte order and leaves the
 *    stream positioned immediately after it.
 *  - Throws std::runtime_error if the stream is truncated before
 *    FILE_HEADER_SIZE bytes are available.
 *  - Throws std::runtime_error if the magic signature does not match
 *    CdxFormat::MAGIC.
 *  - Throws std::runtime_error if nid_width/seqlen_width are unsupported.
 *  - Throws std::runtime_error if n_components == 0.
 */
// A well-formed header is parsed successfully with the correct component count.
CDX_TEST(readGlobalHeader_parses_valid_header) {
    std::string bytes;
    appendFileHeader(bytes, 3);
    std::istringstream input(bytes, std::ios::binary);

    const FileHeader header = readGlobalHeader(input);
    CDX_ASSERT_EQ(header.n_components, static_cast<uint32_t>(3));
    CDX_ASSERT_TRUE(CdxFormat::has_valid_magic(header));
}

// A stream shorter than FILE_HEADER_SIZE bytes is rejected as truncated.
CDX_TEST(readGlobalHeader_rejects_truncated_stream) {
    std::string bytes;
    appendFileHeader(bytes, 1);
    bytes.resize(bytes.size() - 2); // Truncate the last two bytes.
    std::istringstream input(bytes, std::ios::binary);

    CDX_ASSERT_THROWS(readGlobalHeader(input), std::runtime_error);
}

// A corrupted magic signature is rejected.
CDX_TEST(readGlobalHeader_rejects_invalid_magic) {
    std::string bytes;
    appendFileHeader(bytes, 1);
    bytes[0] = 'X'; // Corrupt the first magic byte.
    std::istringstream input(bytes, std::ios::binary);

    CDX_ASSERT_THROWS(readGlobalHeader(input), std::runtime_error);
}

// An unsupported nid_width/seqlen_width is rejected.
CDX_TEST(readGlobalHeader_rejects_invalid_widths) {
    std::string bytes;
    appendFileHeader(bytes, 1);
    bytes[8] = static_cast<char>(CdxFormat::NID_WIDTH + 1); // Corrupt nid_width byte.
    std::istringstream input(bytes, std::ios::binary);

    CDX_ASSERT_THROWS(readGlobalHeader(input), std::runtime_error);
}

// An archive declaring zero components is rejected, even with valid magic/widths.
CDX_TEST(readGlobalHeader_rejects_zero_components) {
    std::string bytes;
    appendFileHeader(bytes, 0);
    std::istringstream input(bytes, std::ios::binary);

    CDX_ASSERT_THROWS(readGlobalHeader(input), std::runtime_error);
}

/**
 * @brief readComponentHeader
 *
 * Reads a fixed-size ComponentHeader followed by its variable-length name
 * at the current stream position.
 *
 * Invariants:
 *  - On success, the stream is positioned at the first NodeRecord of the
 *    component's payload, and the returned ComponentInfo's payload_offset
 *    reflects that position.
 *  - Throws std::runtime_error if the header or the name is truncated.
 *  - Throws std::runtime_error if n_records == 0.
 *  - Throws std::runtime_error if node_id_min > node_id_max.
 *  - Throws std::runtime_error if n_records - 1 exceeds the node-ID span
 *    (more records than the ID range can contain).
 *  - Throws std::runtime_error if the component name is empty.
 *  - Throws std::runtime_error if the component name contains an embedded
 *    NUL byte.
 */
// A well-formed component header + name is parsed with the correct metadata and payload location.
CDX_TEST(readComponentHeader_parses_valid_header) {
    std::string bytes;
    appendComponentHeader(bytes, /*n_records=*/10, /*nid_min=*/0, /*nid_max=*/9, /*name_size=*/5);
    bytes += "compA";
    std::istringstream input(bytes, std::ios::binary);

    const ComponentInfo info = readComponentHeader(input, /*component_id=*/0);

    CDX_ASSERT_EQ(info.compo_id, static_cast<Cid>(0));
    CDX_ASSERT_EQ(info.compo_name, std::string("compA"));
    CDX_ASSERT_EQ(info.nid_min, static_cast<Nid>(0));
    CDX_ASSERT_EQ(info.nid_max, static_cast<Nid>(9));
    CDX_ASSERT_EQ(info.nb_nodes, static_cast<RecordCount>(10));
    CDX_ASSERT_EQ(info.payload_offset, static_cast<std::uint64_t>(bytes.size()));
    CDX_ASSERT_EQ(info.payload_size, static_cast<std::streamoff>(10 * CdxFormat::RECORD_SIZE));
}

// A truncated fixed-size header is rejected.
CDX_TEST(readComponentHeader_rejects_truncated_header) {
    std::string bytes;
    appendComponentHeader(bytes, 1, 0, 0, 5);
    bytes.resize(bytes.size() - 3); // Truncate the header itself.
    std::istringstream input(bytes, std::ios::binary);

    CDX_ASSERT_THROWS(readComponentHeader(input, 0), std::runtime_error);
}

// A name shorter than the declared name_size is rejected as truncated.
CDX_TEST(readComponentHeader_rejects_truncated_name) {
    std::string bytes;
    appendComponentHeader(bytes, 1, 0, 0, 5);
    bytes += "ab"; // Only 2 of the declared 5 name bytes are present.
    std::istringstream input(bytes, std::ios::binary);

    CDX_ASSERT_THROWS(readComponentHeader(input, 0), std::runtime_error);
}

// A component declaring zero records is rejected.
CDX_TEST(readComponentHeader_rejects_zero_records) {
    std::string bytes;
    appendComponentHeader(bytes, /*n_records=*/0, /*nid_min=*/0, /*nid_max=*/0, /*name_size=*/5);
    bytes += "compA";
    std::istringstream input(bytes, std::ios::binary);

    CDX_ASSERT_THROWS(readComponentHeader(input, 0), std::runtime_error);
}

// A node-ID range where min > max is rejected.
CDX_TEST(readComponentHeader_rejects_inverted_node_id_range) {
    std::string bytes;
    appendComponentHeader(bytes, /*n_records=*/1, /*nid_min=*/10, /*nid_max=*/5, /*name_size=*/5);
    bytes += "compA";
    std::istringstream input(bytes, std::ios::binary);

    CDX_ASSERT_THROWS(readComponentHeader(input, 0), std::runtime_error);
}

// A record count larger than the node-ID range can contain is rejected.
CDX_TEST(readComponentHeader_rejects_record_count_exceeding_id_span) {
    std::string bytes;
    // node_id_min == node_id_max == 0 (span 0) cannot hold 2 records (n_records - 1 == 1 > 0).
    appendComponentHeader(bytes, /*n_records=*/2, /*nid_min=*/0, /*nid_max=*/0, /*name_size=*/5);
    bytes += "compA";
    std::istringstream input(bytes, std::ios::binary);

    CDX_ASSERT_THROWS(readComponentHeader(input, 0), std::runtime_error);
}

// A record count that exactly fills the node-ID span is accepted (boundary case).
CDX_TEST(readComponentHeader_accepts_record_count_exactly_filling_id_span) {
    std::string bytes;
    // span = 9, n_records - 1 == 9 == span: valid boundary.
    appendComponentHeader(bytes, /*n_records=*/10, /*nid_min=*/0, /*nid_max=*/9, /*name_size=*/5);
    bytes += "compA";
    std::istringstream input(bytes, std::ios::binary);

    CDX_ASSERT_NO_THROW(readComponentHeader(input, 0));
}

// A zero-length component name is rejected as empty.
CDX_TEST(readComponentHeader_rejects_empty_name) {
    std::string bytes;
    appendComponentHeader(bytes, /*n_records=*/1, /*nid_min=*/0, /*nid_max=*/0, /*name_size=*/0);
    std::istringstream input(bytes, std::ios::binary);

    CDX_ASSERT_THROWS(readComponentHeader(input, 0), std::runtime_error);
}

// A component name containing an embedded NUL byte is rejected.
CDX_TEST(readComponentHeader_rejects_embedded_nul_in_name) {
    std::string bytes;
    appendComponentHeader(bytes, /*n_records=*/1, /*nid_min=*/0, /*nid_max=*/0, /*name_size=*/3);
    bytes += std::string("a\0b", 3);
    std::istringstream input(bytes, std::ios::binary);

    CDX_ASSERT_THROWS(readComponentHeader(input, 0), std::runtime_error);
}

/**
 * @brief seekComponent
 *
 * Locates a component by ID inside a seekable archive, starting from the
 * beginning of the stream and skipping the payloads of preceding
 * components.
 *
 * Invariants:
 *  - Resets and validates the global header first, so any global-header
 *    error (see readGlobalHeader) propagates from here too.
 *  - On success, the stream is positioned at the first NodeRecord of the
 *    requested component, and the returned ComponentInfo matches that
 *    component.
 *  - Throws std::out_of_range if component_id >= n_components.
 */
// Seeking component 0 in a multi-component archive returns its correct metadata.
CDX_TEST(seekComponent_locates_first_component) {
    const std::string bytes = buildValidTwoComponentArchive();
    std::istringstream input(bytes, std::ios::binary);

    const ComponentInfo info = seekComponent(input, 0);
    CDX_ASSERT_EQ(info.compo_name, std::string("compA"));
    CDX_ASSERT_EQ(info.nb_nodes, static_cast<RecordCount>(1));
}

// Seeking a later component correctly skips the payloads of preceding ones.
CDX_TEST(seekComponent_locates_second_component_after_skipping_first_payload) {
    const std::string bytes = buildValidTwoComponentArchive();
    std::istringstream input(bytes, std::ios::binary);

    const ComponentInfo info = seekComponent(input, 1);
    CDX_ASSERT_EQ(info.compo_name, std::string("compB"));
    CDX_ASSERT_EQ(info.nid_min, static_cast<Nid>(2));
    CDX_ASSERT_EQ(info.nid_max, static_cast<Nid>(3));
    CDX_ASSERT_EQ(info.nb_nodes, static_cast<RecordCount>(2));

    // The stream must now be positioned exactly at component B's payload, so its
    // records can be read directly.
    const std::vector<NodeRecord> records = readComponentPayload(input, info.nb_nodes);
    CDX_ASSERT_EQ(records.size(), static_cast<std::size_t>(2));
    CDX_ASSERT_EQ(records[0].node_id, static_cast<Nid>(2));
    CDX_ASSERT_EQ(records[1].node_id, static_cast<Nid>(3));
}

// Requesting a component ID equal to n_components (one past the last valid ID) is out of range.
CDX_TEST(seekComponent_rejects_component_id_equal_to_count) {
    const std::string bytes = buildValidTwoComponentArchive();
    std::istringstream input(bytes, std::ios::binary);

    CDX_ASSERT_THROWS(seekComponent(input, 2), std::out_of_range);
}

// Requesting a component ID far beyond n_components is also out of range.
CDX_TEST(seekComponent_rejects_component_id_far_beyond_count) {
    const std::string bytes = buildValidTwoComponentArchive();
    std::istringstream input(bytes, std::ios::binary);

    CDX_ASSERT_THROWS(seekComponent(input, 999), std::out_of_range);
}

// A corrupt global header is reported even when reached through seekComponent.
CDX_TEST(seekComponent_propagates_invalid_global_header) {
    std::string bytes = buildValidSingleComponentArchive();
    bytes[0] = 'X'; // Corrupt the magic signature.
    std::istringstream input(bytes, std::ios::binary);

    CDX_ASSERT_THROWS(seekComponent(input, 0), std::runtime_error);
}

/**
 * @brief readComponentPayload
 *
 * Reads and decodes `record_count` NodeRecords from the current stream
 * position.
 *
 * Invariants:
 *  - record_count == 0 returns an empty vector without reading from the
 *    stream.
 *  - On success, returns exactly `record_count` records converted to
 *    native byte order, in the order they appear on the stream.
 *  - Throws std::runtime_error if the stream contains fewer than
 *    record_count * RECORD_SIZE remaining bytes.
 *  - In builds without NDEBUG, additionally throws std::runtime_error if
 *    any record has seq_len == 0, has idx >= record_count, or if node IDs
 *    are not strictly increasing from one record to the next.
 */
// A record count of zero returns an empty vector and does not require any stream data.
CDX_TEST(readComponentPayload_returns_empty_for_zero_record_count) {
    std::istringstream input(std::string(), std::ios::binary);
    const std::vector<NodeRecord> records = readComponentPayload(input, 0);
    CDX_ASSERT_TRUE(records.empty());
}

// A well-formed payload is decoded into the expected records, in order.
CDX_TEST(readComponentPayload_decodes_valid_records_in_order) {
    std::string bytes;
    appendNodeRecord(bytes, 1, 0, 100);
    appendNodeRecord(bytes, 2, 1, 200);
    appendNodeRecord(bytes, 3, 2, 300);
    std::istringstream input(bytes, std::ios::binary);

    const std::vector<NodeRecord> records = readComponentPayload(input, 3);
    CDX_ASSERT_EQ(records.size(), static_cast<std::size_t>(3));
    CDX_ASSERT_EQ(records[0].node_id, static_cast<Nid>(1));
    CDX_ASSERT_EQ(records[1].node_id, static_cast<Nid>(2));
    CDX_ASSERT_EQ(records[2].node_id, static_cast<Nid>(3));
    CDX_ASSERT_EQ(records[2].seq_len, static_cast<SeqLen>(300));
}

// A stream with fewer bytes than declared is rejected as truncated.
CDX_TEST(readComponentPayload_rejects_truncated_payload) {
    std::string bytes;
    appendNodeRecord(bytes, 1, 0, 100);
    appendNodeRecord(bytes, 2, 1, 200);
    // Declare 3 records but only provide 2.
    std::istringstream input(bytes, std::ios::binary);

    CDX_ASSERT_THROWS(readComponentPayload(input, 3), std::runtime_error);
}

#ifndef NDEBUG
// Debug-only invariant: a record with seq_len == 0 is rejected.
CDX_TEST(readComponentPayload_rejects_zero_sequence_length_in_debug_builds) {
    std::string bytes;
    appendNodeRecord(bytes, 1, 0, 0); // Invalid: seq_len == 0.
    std::istringstream input(bytes, std::ios::binary);

    CDX_ASSERT_THROWS(readComponentPayload(input, 1), std::runtime_error);
}

// Debug-only invariant: a record with idx >= record_count is rejected.
CDX_TEST(readComponentPayload_rejects_out_of_range_idx_in_debug_builds) {
    std::string bytes;
    appendNodeRecord(bytes, 1, 5, 100); // Invalid: idx (5) >= record_count (1).
    std::istringstream input(bytes, std::ios::binary);

    CDX_ASSERT_THROWS(readComponentPayload(input, 1), std::runtime_error);
}

// Debug-only invariant: node IDs must be strictly increasing between consecutive records.
CDX_TEST(readComponentPayload_rejects_non_increasing_node_ids_in_debug_builds) {
    std::string bytes;
    appendNodeRecord(bytes, 5, 0, 100);
    appendNodeRecord(bytes, 5, 1, 200); // Invalid: equal to the previous node_id.
    std::istringstream input(bytes, std::ios::binary);

    CDX_ASSERT_THROWS(readComponentPayload(input, 2), std::runtime_error);
}
#endif // NDEBUG

int main() {
    return cdx_test::run_all("cdx_IO");
}
