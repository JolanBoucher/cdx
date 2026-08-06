/**
 * @file main.cpp
 * @brief Standalone entry point for the cdx_builder executable.
 *
 * Thin wrapper around cdx_builder::run() (see builder_app.h/.cpp), which
 * holds the actual pipeline logic. Kept as its own executable target for
 * independent development/testing; the merged "cdx" toolkit executable
 * calls cdx_builder::run() directly as a library function instead of
 * spawning this binary.
 */

#include "builder_app.h"

int main(const int argc, char** argv) {
    return cdx_builder::run(argc, argv);
}
