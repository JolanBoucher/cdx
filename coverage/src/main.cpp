/**
 * @file main.cpp
 * @brief Standalone entry point for the cdx_coverage executable.
 *
 * Thin wrapper around cdx_coverage::run() (see coverage_app.h/.cpp), which
 * holds the actual application logic. Kept as its own executable target for
 * independent development/testing; the merged "cdx" toolkit executable
 * calls cdx_coverage::run() directly as a library function instead of
 * spawning this binary.
 */

#include "coverage_app.h"

int main(int argc, char **argv) {
    return cdx_coverage::run(argc, argv);
}
