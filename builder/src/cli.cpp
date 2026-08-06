#include "cli.hpp"
#include <CLI/CLI.hpp>
#include <filesystem>
#include <memory>

namespace cdx_builder {

namespace {
    /**
     * @brief Builds (but does not parse) the CLI11 application: registers every
     *        positional/option exactly once, shared by parse_args() (real run)
     *        and usage_text() (help-text generation only, no parsing).
     *
     * @param args Destination struct; option bindings write directly into it
     *        once app->parse() is called by the caller (untouched otherwise).
     * @param comp_level Storage for the optional --compress level (kept
     *        outside CliArgs since it's only conditionally copied into
     *        args.compression_level after parsing - see parse_args()).
     * @return Heap-allocated App (CLI::App has a deleted copy constructor,
     *         so a unique_ptr is used rather than returning by value).
     */
    std::unique_ptr<CLI::App> build_app(CliArgs& args, int& comp_level) {
        auto app = std::make_unique<CLI::App>(
            "Build CDX indexes derived from GBZ pangenome graphs."
        );

        app->usage("cdx_builder <input.gbz> [OPTIONS]");

        // Declaration of the mandatory positional input argument
        app->add_option(
            "input",
            args.input_file,
            "Input GBZ file containing the pangenomic graph to index."
        )->required()->type_name("gbz");

        // Option Groups Setup for structured help output
        auto* group_linearization = app->add_option_group("Linearization Options");
        auto* group_output = app->add_option_group("Output Options");

        // Linearization Parameters
        group_linearization->add_option(
            "-i,--iteration",
            args.max_iterations,
            "Maximum number of relaxation iterations used during graph\n"
            "linearization. Larger values may improve coordinate convergence\n"
            "on highly complex graphs at the cost of additional runtime.\n"
            "Default: 100.\n"
        )->check(CLI::PositiveNumber);

        group_linearization->add_option(
            "-t,--threshold",
            args.threshold,
            "Convergence threshold used for early stopping.\n"
            "The relaxation process terminates when the mean coordinate change\n"
            "between successive iterations falls below this value.\n"
            "Smaller values increase accuracy but may require more iterations.\n"
            "Default: 0.01."
        )->check(CLI::PositiveNumber);

        group_linearization->add_option(
            "-l,--lambda-anchor",
            args.lambda_anchor,
            "Anchor regularization coefficient in the range [0.0, 1.0].\n "
            "Controls the balance between path-derived anchor coordinates and\n "
            "topology-driven smoothing.\n\n"
            "  1.0 : preserve path-derived coordinates exactly.\n"
            "  0.0 : rely exclusively on local graph connectivity.\n"
            "  0.5 : equal contribution of anchors and topology.\n\n"
            "Recommended values for most pangenome graphs are between\n "
            "0.6 and 0.8. Default: 0.7.\n"
        )->check(CLI::Range(0.0, 1.0));

        // Output Parameters
        group_output->add_option(
            "-o,--output",
            args.output_file,
            "Destination CDX file. If omitted, the output path is derived\n "
            "automatically from the input filename by replacing the extension\n "
            "with '.cdx'.\n"
        );

        // Optional Zstandard compression flag with an optional level argument
        comp_level = 3;
        auto* opt_compress = group_output->add_option(
            "-c,--compress",
            comp_level,
            "Compress the generated CDX using Zstandard.\n "
            "Valid levels range from 1 (fastest) to 22 (highest compression).\n "
            "Compression reduces storage requirements but is intended primarily\n "
            "for archival and file transfer rather than direct querying.\n "
            "Default 3.\n"
        )->check(CLI::Range(1, 22))
         ->type_size(0, 1)    // Accepts 0 or 1 arguments (flag use defaults to level 3)
         ->default_str("3");

        // Debug flag to dump human-readable text instead of binary
        auto* opt_debug = group_output->add_flag(
            "-d,--debug",
            args.debug,
            "Write the final index as a human-readable TSV table to standard\n "
            "output instead of generating a binary CDX file. Useful for\n "
            "development, testing, debugging, and pipeline validation.\n"
        );

        // Enforce mutual exclusivity between binary compression and debug TSV output
        opt_compress->excludes(opt_debug);

        return app;
    }
} // namespace

CliArgs parse_args(const int argc, char** argv)
{
    CliArgs args;
    int comp_level = 3;
    const std::unique_ptr<CLI::App> app = build_app(args, comp_level);

    // Parse command line arguments.
    try {
        app->parse(argc, argv);
    }
    catch (const CLI::ParseError& e) {
        // --help/--version (and other zero-exit-code CLI11 requests) are not really errors:
        // terminate directly here so their formatted output and exit code behave as usual.
        if (e.get_exit_code() == 0) {
            std::exit(app->exit(e));
        }
        // A genuine parsing error (missing/invalid argument, range violation, mutually
        // exclusive options, ...): let it propagate as an exception instead of calling
        // std::exit() here, so callers -- including unit tests -- can catch and handle it
        // safely rather than the whole process being torn down unconditionally. main()
        // already has a generic `catch (const std::exception&)` that handles this correctly
        // for production use.
        throw;
    }

    // Populate optional compression level if the compression option was triggered
    if (const CLI::Option* opt_compress = app->get_option("--compress"); *opt_compress) {
        args.compression_level = comp_level;
    }

    return args;
}

std::string usage_text()
{
    // Builds the same App as parse_args() but never calls ->parse(), so this
    // is safe to call at any time (no side effects, no argv needed) purely
    // to obtain CLI11's own formatted help text - used by the merged "cdx"
    // dispatcher to print this branch's full option list as part of its own
    // top-level --help (see ../../src/main.cpp).
    CliArgs args;
    int comp_level = 3;
    const std::unique_ptr<CLI::App> app = build_app(args, comp_level);
    return app->help();
}

std::string prepare_output_filepath(
    const std::string& output_filepath,
    const std::string& input_filepath,
    std::string ext)
{
    // Ensure the custom file extension starts with a leading dot
    if (!ext.empty() && ext.front() != '.') {
        ext = "." + ext;
    }

    // Fallback to input filename if no explicit output path was supplied
    const std::string base_path = output_filepath.empty() ? input_filepath : output_filepath;
    if (base_path.empty()) return "";

    // std::filesystem::path::replace_extension() already replaces only the last
    // "<dot><suffix>" component of the filename (its native definition of "extension"),
    // so calling it directly is both sufficient and correct here -- no manual dot-finding
    // needed. (A prior version of this function pre-stripped the extension by hand before
    // calling replace_extension(); that was redundant and actually double-stripped
    // multi-dot names like "sample.chr1.gbz", since replace_extension() would then also
    // strip ".chr1" as if it were the extension, yielding "sample.cdx" instead of the
    // intended "sample.chr1.cdx".)
    std::filesystem::path p(base_path);
    p.replace_extension(ext);
    return p.string();
}

} // namespace cdx_builder
