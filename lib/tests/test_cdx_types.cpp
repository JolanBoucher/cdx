/**
 * @file test_cdx_types.cpp
 * @brief Unit tests for the plain data structures declared in cdx_types.h.
 *
 * cdx_types.h defines almost no behavior of its own: it is a collection of
 * type aliases and POD-like structures. Consequently these tests focus on:
 *  - the default-construction invariants of each structure (zero-initialized
 *    scalar fields, empty containers), since callers rely on these defaults
 *    before a structure is populated by cdx_IO;
 *  - the one non-trivial piece of behavior in this header,
 *    QueryData::getComponentLength(), including its empty/non-empty cases.
 *
 * These tests do not touch I/O or serialization; see test_cdx_format.cpp and
 * test_cdx_IO.cpp for those.
 */

#include "../include/cdx_types.h"
#include "test_utils.h"

using namespace cdx;

/**
 * @brief QueryData::getComponentLength()
 *
 * Returns the total length of the component in base pairs.
 *
 * Invariants:
 *  - Returns 0 when idx2bp is empty (no positions have been recorded yet).
 *  - Otherwise returns idx2bp.back(), i.e. the last cumulative base-pair
 *    position, which by construction is the length of the component.
 *  - Never throws (noexcept) and never modifies the QueryData instance.
 */
// Empty idx2bp yields a component length of zero.
CDX_TEST(getComponentLength_returns_zero_when_idx2bp_empty) {
    QueryData query{};
    CDX_ASSERT_TRUE(query.idx2bp.empty());
    CDX_ASSERT_EQ(query.getComponentLength(), static_cast<PosBp>(0));
}

// Non-empty idx2bp yields its last cumulative position.
CDX_TEST(getComponentLength_returns_last_cumulative_position) {
    QueryData query{};
    query.idx2bp = {0, 10, 25, 42};
    CDX_ASSERT_EQ(query.getComponentLength(), static_cast<PosBp>(42));
}

// A single-element idx2bp is a valid, non-empty case (component of length equal to that element).
CDX_TEST(getComponentLength_handles_single_element_vector) {
    QueryData query{};
    query.idx2bp = {7};
    CDX_ASSERT_EQ(query.getComponentLength(), static_cast<PosBp>(7));
}

// The method reflects the current state of idx2bp after further mutation (no caching).
CDX_TEST(getComponentLength_reflects_mutations_after_construction) {
    QueryData query{};
    query.idx2bp = {0, 5};
    CDX_ASSERT_EQ(query.getComponentLength(), static_cast<PosBp>(5));

    query.idx2bp.push_back(100);
    CDX_ASSERT_EQ(query.getComponentLength(), static_cast<PosBp>(100));

    query.idx2bp.clear();
    CDX_ASSERT_EQ(query.getComponentLength(), static_cast<PosBp>(0));
}

/**
 * @brief Default-construction invariants of PositionIndex.
 *
 * A default-constructed PositionIndex represents "no position information
 * yet": both the bit vector and its rank index must be empty.
 */
// Default-constructed PositionIndex has empty bitvector and rank_index.
CDX_TEST(position_index_default_constructs_empty) {
    PositionIndex index{};
    CDX_ASSERT_TRUE(index.bitvector.empty());
    CDX_ASSERT_TRUE(index.rank_index.empty());
}

/**
 * @brief Default-construction invariants of GraphLayout.
 *
 * A default-constructed GraphLayout must have all scalar fields at their
 * documented zero defaults and all container fields empty, since consumers
 * (loadGlobal()) build it up incrementally from these defaults.
 */
// Default-constructed GraphLayout has zeroed scalars and empty containers.
CDX_TEST(graph_layout_default_constructs_with_zeroed_fields) {
    GraphLayout layout{};
    CDX_ASSERT_EQ(layout.graph_nid_min, static_cast<Nid>(0));
    CDX_ASSERT_EQ(layout.graph_nid_max, static_cast<Nid>(0));
    CDX_ASSERT_EQ(layout.total_nodes, static_cast<RecordCount>(0));
    CDX_ASSERT_EQ(layout.component_count, static_cast<std::size_t>(0));
    CDX_ASSERT_TRUE(layout.component_offsets.empty());
    CDX_ASSERT_TRUE(layout.component_names.empty());
}

/**
 * @brief Default-construction invariants of QueryData.
 *
 * A default-constructed QueryData must expose empty translation tables and
 * a query range of {0, 0} in both base-pair and index space, matching the
 * in-class default member initializers.
 */
// Default-constructed QueryData has empty tables and a {0, 0} query range.
CDX_TEST(query_data_default_constructs_with_empty_tables_and_zero_range) {
    QueryData query{};
    CDX_ASSERT_TRUE(query.nid2idx.empty());
    CDX_ASSERT_TRUE(query.idx2bp.empty());
    CDX_ASSERT_TRUE(query.node_coverage.empty());
    CDX_ASSERT_TRUE((query.query_range_bp == std::pair<PosBp, PosBp>{0, 0}));
    CDX_ASSERT_TRUE((query.query_range_idx == std::pair<Idx, Idx>{0, 0}));
}

/**
 * @brief Default-construction invariants of GlobalData.
 *
 * A default-constructed GlobalData must have an empty (default) layout and
 * empty translation/aggregation tables, mirroring GraphLayout's own
 * defaults.
 */
// Default-constructed GlobalData has an empty layout and empty aggregate tables.
CDX_TEST(global_data_default_constructs_with_empty_tables) {
    GlobalData data{};
    CDX_ASSERT_EQ(data.layout.component_count, static_cast<std::size_t>(0));
    CDX_ASSERT_TRUE(data.nid2flat_idx.empty());
    CDX_ASSERT_TRUE(data.node_coverage.empty());
    CDX_ASSERT_TRUE(data.idx2bp.empty());
    CDX_ASSERT_TRUE(data.idx2bp_offsets.empty());
    CDX_ASSERT_TRUE(data.component_names.empty());
    CDX_ASSERT_TRUE(data.component_lengths.empty());
}

/**
 * @brief ComponentInfo has no default member initializers.
 *
 * Unlike GraphLayout/QueryData, ComponentInfo's fields are not
 * zero-initialized by default (they are populated explicitly by
 * cdx_IO::readComponentHeader()). This test only checks that explicitly
 * assigned fields round-trip through the structure unmodified, guarding
 * against accidental field reordering/typos in the struct definition.
 */
// Explicitly assigned fields are stored and read back unmodified.
CDX_TEST(component_info_stores_assigned_fields_unmodified) {
    ComponentInfo info{};
    info.compo_id = 3;
    info.compo_name = "chrom3";
    info.nid_min = 100;
    info.nid_max = 199;
    info.nb_nodes = 100;
    info.component_length = 5000;
    info.payload_offset = 1024;
    info.payload_size = 1600;

    CDX_ASSERT_EQ(info.compo_id, static_cast<Cid>(3));
    CDX_ASSERT_EQ(info.compo_name, std::string("chrom3"));
    CDX_ASSERT_EQ(info.nid_min, static_cast<Nid>(100));
    CDX_ASSERT_EQ(info.nid_max, static_cast<Nid>(199));
    CDX_ASSERT_EQ(info.nb_nodes, static_cast<RecordCount>(100));
    CDX_ASSERT_EQ(info.component_length, static_cast<PosBp>(5000));
    CDX_ASSERT_EQ(info.payload_offset, static_cast<std::uint64_t>(1024));
    CDX_ASSERT_EQ(info.payload_size, static_cast<std::streamoff>(1600));
}

int main() {
    return cdx_test::run_all("cdx_types");
}
