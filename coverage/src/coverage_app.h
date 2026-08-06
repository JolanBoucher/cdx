/**
 * @file coverage_app.h
 * @brief Entry point of the CDX coverage pipeline, callable as a library function.
 */

#ifndef CDX_COVERAGE_APP_H
#define CDX_COVERAGE_APP_H

namespace cdx_coverage {

/**
 * @brief Runs the CDX coverage application: parses arguments and dispatches
 *        to inspection, single-component/query, or whole-pangenome coverage
 *        analysis as appropriate.
 *
 * Equivalent to what used to be main() for the standalone cdx_coverage
 * executable, extracted so it can be called directly as a library function -
 * both by the standalone cdx_coverage executable (src/main.cpp) and by the
 * merged "cdx" toolkit executable, once the latter has determined (from the
 * input file's binary signature) that the CDX-inspection or coverage branch
 * is the right one to run.
 *
 * @param argc Argument count (same layout main() receives: argv[0] is the
 *             program/branch name, argv[1] is expected to be the CDX path,
 *             argv[2] the optional GAM path).
 * @param argv Argument vector.
 * @return EXIT_SUCCESS or EXIT_FAILURE.
 */
int run(int argc, char** argv);

} // namespace cdx_coverage

#endif // CDX_COVERAGE_APP_H
