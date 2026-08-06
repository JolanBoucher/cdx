/**
 * @file cdx_io.cpp
 * @brief Binary I/O utilities for reading and seeking uncompressed CDX index.
 */

#include "cdx_IO.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <ios>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
    /**
     * @brief Reads exactly byte_count bytes from a binary input stream.
     *
     * Converts stream read failures or premature EOF into explicit, contextual exceptions.
     *
     * @param input Source binary stream.
     * @param destination Destination raw byte buffer.
     * @param byte_count Number of bytes to read.
     * @param context Descriptive string describing the buffer for error context.
     *
     * @throws std::overflow_error If byte_count exceeds std::streamsize capacity.
     * @throws std::runtime_error If stream read fails or EOF is encountered before byte_count.
     */
    void readExact(
        std::istream &input,
        char *destination,
        const std::size_t byte_count,
        const char *context
    ) {
        if (byte_count == 0) {
            return;
        }
        if (byte_count > static_cast<std::size_t>(std::numeric_limits<std::streamsize>::max())) {
            throw std::overflow_error(std::string(context) + " exceeds std::streamsize capacity.");
        }
        input.read(destination, static_cast<std::streamsize>(byte_count));
        if (!input) {
            throw std::runtime_error(std::string("Unable to read ") + context +
                                     ". The CDX archive may be truncated or corrupt.");
        }
    }

    /**
     * @brief Safely computes the total byte payload size for a given number of node records.
     *
     * @param record_count Number of records in the component payload.
     * @return Total payload size in bytes as std::streamoff for seek operations.
     *
     * @throws std::overflow_error If total byte size exceeds std::streamoff capacity.
     */
    [[nodiscard]] std::streamoff computePayloadSize(
        const cdx::RecordCount record_count
    ) {
        constexpr std::uint64_t record_size = cdx::CdxFormat::RECORD_SIZE;
        constexpr auto streamoff_max = static_cast<std::uint64_t>(std::numeric_limits<std::streamoff>::max());

        if (record_count > streamoff_max / record_size) {
            throw std::overflow_error("CDX component payload exceeds stream offset capacity.");
        }

        return static_cast<std::streamoff>(record_count * record_size);
    }

    /**
     * @brief Converts std::streampos into a validated 64-bit unsigned file offset.
     *
     * @param position Current stream position.
     * @return Absolute 64-bit byte offset from the start of the stream.
     *
     * @throws std::runtime_error If the stream position is invalid or negative.
     */
    [[nodiscard]] std::uint64_t streamPositionToOffset(
        const std::streampos &position
    ) {
        if (position == std::streampos(-1)) {
            throw std::runtime_error("Unable to determine current CDX stream position.");
        }

        const std::streamoff offset = position - std::streampos(0);
        if (offset < 0) {
            throw std::runtime_error("CDX stream position returned a negative file offset.");
        }

        return static_cast<std::uint64_t>(offset);
    }
} // anonymous namespace

namespace cdx {
    FileHeader readGlobalHeader(std::istream &input) {
        std::array<char, CdxFormat::FILE_HEADER_SIZE> buffer{};

        readExact(input, buffer.data(), buffer.size(), "CDX global file header");
        const FileHeader header = CdxFormat::unpack_file_header(buffer.data());

        if (!CdxFormat::has_valid_magic(header)) {
            throw std::runtime_error("Invalid CDX magic signature.");
        }
        if (!CdxFormat::has_valid_widths(header)) {
            throw std::runtime_error("Unsupported CDX node-ID or sequence-length width.");
        }
        if (header.n_components == 0) {
            throw std::runtime_error("The CDX archive contains no components.");
        }

        return header;
    }

    [[nodiscard]]
    ComponentInfo readComponentHeader(
        std::istream &input,
        Cid component_id
    ) {
        std::array<
            char,
            CdxFormat::COMPONENT_HEADER_SIZE
        > header_buffer{};

        readExact(
            input,
            header_buffer.data(),
            header_buffer.size(),
            "CDX component header"
        );

        const ComponentHeader header = CdxFormat::unpack_component_header(header_buffer.data());

        if (header.n_records == 0) {
            throw std::runtime_error("CDX component " + std::to_string(component_id) + " contains no node records.");
        }
        if (header.node_id_min > header.node_id_max) {
            throw std::runtime_error(
                "CDX component " + std::to_string(component_id) + " has an invalid node-ID range.");
        }
        const std::uint64_t node_id_span = header.node_id_max - header.node_id_min;
        if (header.n_records - 1 > node_id_span) {
            throw std::runtime_error(
                "CDX component " + std::to_string(component_id) +
                " declares more records than its node-ID range can contain."
            );
        }

        /*
         * The component name is serialized immediately after the
         * fixed-size ComponentHeader with no terminating NUL byte.
         */
        std::string component_name(static_cast<std::size_t>(header.name_size), '\0');
        if (!component_name.empty()) {
            readExact(
                input,
                component_name.data(),
                component_name.size(),
                "CDX component name"
            );
        }
        if (component_name.empty()) {
            throw std::runtime_error("CDX component " + std::to_string(component_id) + " has an empty name.");
        }
        if (component_name.find('\0') != std::string::npos) {
            throw std::runtime_error(
                "CDX component " + std::to_string(component_id) + " contains an embedded NUL byte in its name.");
        }

        /*
         * At this point, the stream is positioned immediately after:
         *
         *     ComponentHeader
         *     component_name
         *
         * Therefore tellg() identifies the beginning of the NodeRecord
         * payload.
         */
        const std::uint64_t payload_offset = streamPositionToOffset(input.tellg());
        const std::streamoff payload_size = computePayloadSize(header.n_records);

        ComponentInfo component{};

        component.compo_id = component_id;
        component.compo_name = std::move(component_name);
        component.nid_min = header.node_id_min;
        component.nid_max = header.node_id_max;
        component.nb_nodes = header.n_records;
        component.component_length = 0;
        component.payload_offset = payload_offset;
        component.payload_size = payload_size;

        return component;
    }

    [[nodiscard]] ComponentInfo seekComponent(
        std::istream &input,
        const Cid component_id
    ) {
        input.clear();
        input.seekg(0, std::ios::beg);

        if (!input) {
            throw std::runtime_error("Unable to seek to the beginning of the CDX stream.");
        }

        const FileHeader file_header = readGlobalHeader(input);

        if (component_id >= file_header.n_components) {
            throw std::out_of_range("CDX component ID " + std::to_string(component_id) +
                                    " is out of range. Archive contains " +
                                    std::to_string(file_header.n_components) + " components.");
        }

        // Skip preceding component headers and their payload blocks
        for (Cid current_id = 0; current_id < component_id; ++current_id) {
            const ComponentInfo component = readComponentHeader(input, current_id);

            input.seekg(component.payload_size, std::ios::cur);
            if (!input) {
                throw std::runtime_error("Unable to skip payload of CDX component " + std::to_string(current_id) +
                                         ". The archive may be truncated or corrupt.");
            }
        }

        // Stream is now positioned right at the target component header
        return readComponentHeader(input, component_id);
    }

    [[nodiscard]] std::vector<NodeRecord> readComponentPayload(
        std::istream &input,
        const RecordCount record_count
    ) {
        if (record_count == 0) {
            return {};
        }

        std::vector<NodeRecord> records;
        if (record_count > static_cast<RecordCount>(records.max_size())) {
            throw std::length_error("CDX component record count exceeds std::vector<NodeRecord> capacity.");
        }

        const std::size_t count = record_count;
        if (count > std::numeric_limits<std::size_t>::max() / CdxFormat::RECORD_SIZE) {
            throw std::overflow_error("CDX component payload byte size overflow.");
        }

        const std::size_t byte_count = count * CdxFormat::RECORD_SIZE;
        records.resize(count);

        readExact(input, reinterpret_cast<char *>(records.data()), byte_count, "CDX component node-record payload");

        // Native endianness conversion handled directly inside CdxFormat
        CdxFormat::convert_node_records_to_native(records.data(), records.size());

#ifndef NDEBUG
        for (std::size_t i = 0; i < records.size(); ++i) {
            const NodeRecord &record = records[i];

            if (record.seq_len == 0) {
                throw std::runtime_error("CDX node record " + std::to_string(i) +
                                         " has a zero sequence length.");
            }

            if (record.idx >= record_count) {
                throw std::runtime_error("CDX node record " + std::to_string(i) +
                                         " has an out-of-range local index: " +
                                         std::to_string(record.idx) + ".");
            }

            if (i > 0 && records[i - 1].node_id >= record.node_id) {
                throw std::runtime_error("CDX node records are not strictly ordered by node ID at record " +
                                         std::to_string(i) + ".");
            }
        }
#endif

        return records;
    }
} // namespace cdx
