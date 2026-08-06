/**
 * @file builder_app.h
 * @brief Entry point of the CDX builder pipeline, callable as a library function.
 *
 * @created Jolan on 2026-08-06.
 */

#ifndef CDX_BUILDER_APP_H
#define CDX_BUILDER_APP_H

namespace cdx_builder {

/**
 * @brief Runs the full CDX builder pipeline: parses arguments and executes
 *        the six index-building stages against a GBZ input graph.
 *
 * Equivalent to what used to be main() for the standalone cdx_builder
 * executable, extracted so it can be called directly as a library function -
 * both by the standalone cdx_builder executable (src/main.cpp) and by the
 * merged "cdx" toolkit executable, once the latter has determined (from the
 * input file's binary signature) that the GBZ-building branch is the right
 * one to run.
 *
 * @param argc Argument count (same layout main() receives: argv[0] is the
 *             program/branch name, argv[1] is expected to be the GBZ path).
 * @param argv Argument vector.
 * @return EXIT_SUCCESS or EXIT_FAILURE.
 */
int run(int argc, char** argv);

} // namespace cdx_builder

#endif // CDX_BUILDER_APP_H
