/**
 * @file cdx_types.h
 * @brief Core data structures and type aliases for the CDX domain.
 *
 * This header contains only the plain data structures shared across the CDX
 * modules: type aliases for identifiers and offsets, the position-index
 * bitvector representation, and the intermediate/result structures produced
 * while loading a CDX archive (global layout, per-component metadata, query
 * results, and globally aggregated data).
 *
 * It has no dependency on cdx_format or cdx_IO and defines no behavior of
 * its own; it is purely a vocabulary of types used by the rest of the
 * library.
 */

#ifndef CDX_COVERAGE_CDX_TYPES_H
#define CDX_COVERAGE_CDX_TYPES_H

#include <cstdint>
#include <utility>
#include <vector>
#include <string>
#include <ios>

namespace cdx {
    // --- Type Aliases ---
    using Cid = std::uint32_t; ///< Sequential component identifier.
    using Nid = std::uint64_t; ///< Global node identifier.
    using Idx = std::uint32_t; ///< Local (component-relative) node index.
    using FlatIdx = std::uint64_t; ///< Global flattened index across all components.
    using PosBp = std::uint64_t; ///< Genomic position expressed in base pairs.
    using SeqLen = std::uint32_t; ///< Sequence length, in base pairs.
    using Coverage = std::uint32_t; ///< Coverage depth value for a node/position.
    using RecordCount = std::uint64_t; ///< Number of records (e.g. node records) in a block.
}

namespace cdx {
    /**
     * @brief Bit vector paired with a rank index for fast rank/select queries.
     *
     * Used to project base-pair positions onto local indices: `bitvector`
     * marks the set of covered positions, and `rank_index` caches
     * precomputed rank counts to accelerate rank queries over `bitvector`.
     */
    struct PositionIndex {
        std::vector<std::uint64_t> bitvector; ///< Packed bits, one per position, marking covered positions.
        std::vector<std::uint32_t> rank_index; ///< Precomputed rank counts used to accelerate rank queries.
    };


    /**
     * @brief Global archive layout information, computed prior to construction.
     *
     * Summarizes the node-ID range and component boundaries of a CDX
     * archive before the per-component data is materialized.
     */
    struct GraphLayout {
        Nid graph_nid_min = 0; ///< Minimum node identifier across the whole graph.
        Nid graph_nid_max = 0; ///< Maximum node identifier across the whole graph.
        RecordCount total_nodes = 0; ///< Total number of nodes across all components.
        std::size_t component_count = 0; ///< Number of components in the archive.

        std::vector<RecordCount> component_offsets; ///< Cumulative node-count offset of each component.
        std::vector<std::string> component_names; ///< Name of each component, indexed by component ID.
    };

    /**
     * @brief Metadata describing a single graph component within the archive.
     *
     * Captures the identifying information, node-ID range, and payload
     * location of one component, as read from its ComponentHeader.
     */
    struct ComponentInfo {
        Cid compo_id; ///< Sequential identifier of the component.
        std::string compo_name; ///< Human-readable name of the component.

        Nid nid_min; ///< Minimum node identifier contained in the component.
        Nid nid_max; ///< Maximum node identifier contained in the component.
        RecordCount nb_nodes; ///< Number of node records contained in the component.

        PosBp component_length; ///< Total length of the component, in base pairs.
        std::uint64_t payload_offset; ///< Byte offset of the component's NodeRecord payload in the stream.
        std::streamoff payload_size; ///< Size in bytes of the component's NodeRecord payload.
    };


    /**
     * @brief Result of a query built by loadQuery().
     *
     * Bundles the translation tables and derived indices needed to answer
     * coverage/position queries restricted to a single component and range.
     */
    struct QueryData {
        std::vector<Idx> nid2idx; ///< Maps a node identifier to its local index.
        std::vector<PosBp> idx2bp; ///< Maps a local index to its cumulative base-pair position.
        std::vector<Coverage> node_coverage; ///< Local coverage table, indexed like nid2idx/idx2bp.
        PositionIndex position_index; ///< Projection from base-pair position to local index.
        std::pair<PosBp, PosBp> query_range_bp{0, 0}; ///< Queried range expressed in base pairs [min, max].
        std::pair<Idx, Idx> query_range_idx{0, 0}; ///< Queried range expressed in local indices [min, max].
        ComponentInfo component; ///< Metadata of the component the query was run against.

        /**
         * @brief Returns the total length of the component, in base pairs.
         * @return The last cumulative position in idx2bp, or 0 if empty.
         */
        [[nodiscard]] PosBp getComponentLength() const noexcept {
            return idx2bp.empty() ? 0 : idx2bp.back();
        }
    };


    /**
     * @brief Result of loading the whole archive, built by loadGlobal().
     *
     * Aggregates layout information and translation tables across every
     * component of the archive, as opposed to QueryData which is scoped to
     * a single component/range.
     */
    struct GlobalData {
        GraphLayout layout; ///< Global archive layout (node-ID range, component boundaries).

        // translation tables
        std::vector<FlatIdx> nid2flat_idx; ///< Maps a node identifier to its global flattened index.
        std::vector<Coverage> node_coverage; ///< Coverage table indexed by flattened index.

        std::vector<PosBp> idx2bp; ///< Maps a flattened index to its cumulative base-pair position.
        std::vector<RecordCount> idx2bp_offsets; ///< Per-component starting offset into idx2bp.

        std::vector<std::string> component_names; ///< Name of each component, indexed by component ID.
        std::vector<PosBp> component_lengths; ///< Length in base pairs of each component, indexed by component ID.
    };
} // namespace cdx

#endif //CDX_COVERAGE_CDX_TYPES_H
