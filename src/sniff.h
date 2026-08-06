/**
 * @file sniff.h
 * @brief Input-file type detection for the merged "cdx" toolkit executable.
 *
 * The toolkit dispatches to one of three branches purely from the binary
 * content of the file(s) it is given - never from a flag or sub-command
 * name the user has to type (see main.cpp). This header exposes that
 * detection logic.
 */

#ifndef CDX_TOOLKIT_SNIFF_H
#define CDX_TOOLKIT_SNIFF_H

#include <string>

namespace cdx_toolkit {

enum class InputType {
    Gbz,     ///< gbwtgraph GBZ pangenome graph (builder branch input)
    Cdx,     ///< cdx_lib CDX index (inspect / coverage branch input)
    Gam,     ///< vg GAM alignment file (coverage branch second input)
    Unknown  ///< Unreadable file, or content matches none of the above
};

/**
 * @brief Detects a file's type from its binary signature (magic bytes),
 *        never from its extension.
 *
 * Recognizes:
 *   - CDX:  first 4 bytes equal "CDX\x01" (cdx_lib's CdxFormat::MAGIC).
 *   - GBZ:  first 4 bytes equal "GBZ " (gbwtgraph::GBZ::Header::TAG,
 *           0x205A4247 little-endian).
 *   - GAM:  first 2 bytes are the gzip/BGZF magic (0x1F 0x8B) - vg GAM
 *           files are BGZF-compressed Protobuf streams.
 *
 * @param path Path to the file to inspect.
 * @return The detected InputType, or InputType::Unknown if the file cannot
 *         be opened, is too short, or matches none of the signatures above.
 */
InputType sniff_file(const std::string& path);

/**
 * @brief Best-effort fallback used only to decide which branch's --help
 *        text to print when the given path doesn't exist or can't be read
 *        (e.g. a documentation example such as `cdx exemple.gbz --help`).
 *
 * Never used to decide how to actually process a file - only sniff_file()
 * (real binary signature) governs actual execution.
 *
 * @param path Path whose extension should be inspected (".gbz", ".cdx",
 *             ".cdx.zst", ".gam" - case-insensitive).
 * @return The guessed InputType, or InputType::Unknown if the extension is
 *         not recognized.
 */
InputType guess_type_from_extension(const std::string& path);

} // namespace cdx_toolkit

#endif // CDX_TOOLKIT_SNIFF_H
