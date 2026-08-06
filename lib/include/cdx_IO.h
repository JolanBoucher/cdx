#pragma once

/**
 * @file cdx_io.h
 * @brief Generic binary reading utilities for CDX archives.
 *
 * This module provides validated access to:
 *  - the global CDX file header;
 *  - component headers and names;
 *  - component metadata and payload locations;
 *  - decoded NodeRecord payloads.
 *
 * The functions in this module operate only on the generic CDX format.
 * They do not depend on cdx_builder or cdx_coverage configuration values.
 */

#include "cdx_format.h"
#include "cdx_types.h"

#include <cstddef>
#include <istream>
#include <vector>

namespace cdx {
    /**
     * @brief Reads and validates the global CDX file header.
     *
     * The stream must be positioned at the beginning of the archive.
     * On success, it is positioned immediately after FileHeader.
     *
     * @param input Open binary CDX input stream.
     * @return Validated file header converted to native byte order.
     *
     * @throws std::runtime_error If reading fails.
     * @throws std::runtime_error If the magic signature is invalid.
     * @throws std::runtime_error If the encoded field widths are unsupported.
     */
     FileHeader readGlobalHeader(std::istream &input);

    /**
     * @brief Reads component metadata at the current stream position.
     *
     * Reads the fixed ComponentHeader followed by the variable-length
     * component name. On success, the stream is positioned at the first
     * NodeRecord of the component payload.
     *
     * @param input Open binary CDX input stream.
     * @param component_id Sequential component identifier.
     * @return Component metadata and payload location.
     *
     * @throws std::runtime_error If the header or component name is truncated.
     * @throws std::runtime_error If the component metadata is invalid.
     * @throws std::overflow_error If the payload size cannot be represented.
     */
    [[nodiscard]] ComponentInfo readComponentHeader(
        std::istream &input,
        Cid component_id
    );

    /**
     * @brief Locates a component inside a seekable CDX archive.
     *
     * Resets the stream to the beginning, validates the global header,
     * sequentially scans component headers, and skips the payloads of
     * preceding components.
     *
     * On success, the stream is positioned at the first NodeRecord of the
     * requested component.
     *
     * @param input Open seekable binary CDX input stream.
     * @param component_id Sequential component identifier to locate.
     *
     * @return Metadata describing the requested component.
     *
     * @throws std::out_of_range If component_id does not exist.
     * @throws std::runtime_error If seeking or reading fails.
     */
    [[nodiscard]] ComponentInfo seekComponent(
        std::istream &input,
        Cid component_id
    );

    /**
     * @brief Reads and decodes all node records from a component payload.
     *
     * The stream must already be positioned at the first NodeRecord of the
     * target component.
     *
     * @param input Open binary CDX input stream.
     * @param record_count Number of serialized node records to read.
     * @return Node records converted to native byte order.
     *
     * @throws std::runtime_error If the payload is truncated.
     * @throws std::overflow_error If the required allocation is too large.
     */
    [[nodiscard]] std::vector<NodeRecord> readComponentPayload(
        std::istream &input,
        RecordCount record_count
    );
} // namespace cdx