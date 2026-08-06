/**
 * @file main.cpp
 * @brief Entry point of the merged "cdx" toolkit executable.
 *
 * cdx wraps two logical phases of the pangenome pipeline behind a single
 * flat command: `cdx <file> [file2] [OPTIONS]`. There are no
 * sub-command verbs (no "cdx build"/"cdx inspect"/"cdx coverage") - the
 * intended toolkit context is `<toolkit> cdx <file> [OPTIONS]`, and
 * adding a third command level there would defeat the point.
 *
 * The branch to run is decided purely from the binary signature of the
 * input file(s) (see sniff.h):
 *
 *   - a lone .gbz file            -> cdx_builder::run()   (build a CDX index)
 *   - a lone .cdx file            -> cdx_coverage::run()  (inspect mode, implicit)
 *   - a .cdx file then a .gam file -> cdx_coverage::run()  (coverage mode)
 *
 * cdx_builder and cdx_coverage each keep their own independent CLI11::App
 * (see cdx_builder/src/cli.cpp and cdx_coverage/src/cli.cpp) - they are
 * never parsed in the same process invocation, so their option names never
 * collide even though a couple of short flags are reused with different
 * meanings across branches (e.g. -t is "threshold" for the builder but
 * "worker threads" for coverage). Once this dispatcher has picked a branch,
 * the full, unmodified argv is handed to that branch's own parser, so all
 * of its existing options/validation/--help formatting keep working as-is.
 */

#include "sniff.h"

#include <cstdlib>
#include <cstddef>
#include <iostream>
#include <string>
#include <vector>

// Each branch owns its own main-equivalent entry point, linked in from
// cdx_builder_core / cdx_coverage_core (see CMakeLists.txt).
namespace cdx_builder {
    int run(int argc, char** argv);
}

namespace cdx_coverage {
    int run(int argc, char** argv);
}

namespace {

constexpr const char* CHEAT_SHEET =
    "cdx <file> [file2] [OPTIONS]\n"
    "\n"
    "The mode is determined automatically from the file(s) given:\n"
    "\n"
    "  cdx graph.gbz                build a CDX index from a GBZ graph\n"
    "  cdx index.cdx                 inspect the contents of a CDX index\n"
    "  cdx index.cdx alignment.gam   compute GAM coverage against a CDX index\n"
    "\n"
    "For a mode's detailed help (file doesn't need to exist):\n"
    "  cdx example.gbz --help\n"
    "  cdx example.cdx --help\n"
    "  cdx example.cdx example.gam --help\n";

bool has_help_flag(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        const std::string tok = argv[i];
        if (tok == "-h" || tok == "--help") {
            return true;
        }
    }
    return false;
}

// Collects the leading run of non-flag tokens (before the first token that
// starts with '-'), up to `max_count` entries. This mirrors the documented
// usage of both branches ("cdx <CDX> [GAM] [OPTIONS]" / "cdx <input.gbz>
// [OPTIONS]"): input files always come first, options after - so this
// lightweight pre-scan is enough to pick a branch without re-implementing a
// full argv parser here.
std::vector<std::string> leading_positionals(int argc, char** argv, std::size_t max_count) {
    std::vector<std::string> out;
    for (int i = 1; i < argc && out.size() < max_count; ++i) {
        const std::string tok = argv[i];
        if (!tok.empty() && tok[0] == '-') {
            break;
        }
        out.push_back(tok);
    }
    return out;
}

// Sniffs the real file content first; only falls back to guessing from the
// extension when that fails AND the user is asking for --help (so
// documentation examples like `cdx example.gbz --help` work without the
// file needing to actually exist). A real run always requires the genuine
// binary signature.
cdx_toolkit::InputType detect(const std::string& path, bool help_requested) {
    const cdx_toolkit::InputType sniffed = cdx_toolkit::sniff_file(path);
    if (sniffed != cdx_toolkit::InputType::Unknown) {
        return sniffed;
    }
    if (help_requested) {
        return cdx_toolkit::guess_type_from_extension(path);
    }
    return cdx_toolkit::InputType::Unknown;
}

} // namespace

int main(int argc, char** argv) {
    const bool help_requested = has_help_flag(argc, argv);
    const std::vector<std::string> positionals = leading_positionals(argc, argv, 2);

    if (positionals.empty()) {
        std::cerr << CHEAT_SHEET;
        return help_requested ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    const cdx_toolkit::InputType type1 = detect(positionals[0], help_requested);

    if (positionals.size() == 1) {
        switch (type1) {
            case cdx_toolkit::InputType::Gbz:
                return cdx_builder::run(argc, argv);
            case cdx_toolkit::InputType::Cdx:
                return cdx_coverage::run(argc, argv);
            default:
                std::cerr << "[ERROR] '" << positionals[0]
                          << "' is neither a valid .gbz file nor a valid .cdx file.\n\n"
                          << CHEAT_SHEET;
                return EXIT_FAILURE;
        }
    }

    // Two (or more) leading files: only CDX followed by GAM is a valid
    // combination.
    const cdx_toolkit::InputType type2 = detect(positionals[1], help_requested);

    if (type1 == cdx_toolkit::InputType::Cdx && type2 == cdx_toolkit::InputType::Gam) {
        return cdx_coverage::run(argc, argv);
    }

    std::cerr << "[ERROR] With two files, the expected order is: index.cdx then alignment.gam.\n\n"
              << CHEAT_SHEET;
    return EXIT_FAILURE;
}
