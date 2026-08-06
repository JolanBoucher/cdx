# ------------------------------------------------------------------------------
# External dependencies
# ------------------------------------------------------------------------------
#
# Resolves every third-party dependency needed by the cdx_builder target:
#   1. System libraries expected to already be installed (OpenMP, OpenSSL,
#      zstd, Boost, Jansson).
#   2. CLI11 and libbdsg, fetched automatically at configure time via
#      FetchContent and built via add_subdirectory (no manual `git submodule`
#      step required - see the pinned GIT_TAG on each below).
#   3. sdsl-lite, GBWT and GBWTGraph, fetched + built out-of-tree via
#      ExternalProject (they don't build cleanly as plain add_subdirectory
#      targets alongside libbdsg's own copy of sdsl/handlegraph).
#   4. cdx_lib, the CDX format/IO code shared with cdx_coverage (local
#      sibling directory in this monorepo, see section 4 below).
#
# Every fetched dependency is pinned to the exact commit this project was
# last verified against (not a floating branch/tag), so a fresh configure is
# always reproducible. Bump the GIT_TAG deliberately when you want a newer
# version.
#
# All variables set here (GBWT_INCLUDE_DIR, GBWTGRAPH_INCLUDE_DIR,
# ZSTD_LIBRARY, ZSTD_INCLUDE_DIR, JANSSON_*, LIBBDSG_DIR, imported targets
# gbwt_lib/gbwtgraph_lib/libbdsg/cdx_lib, ...) are consumed by the root
# CMakeLists.txt and by tests/CMakeLists.txt through normal directory-scope
# inheritance.

# --- 1. System libraries ------------------------------------------------------

find_package(OpenMP REQUIRED)
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(Boost REQUIRED)

find_library(ZSTD_LIBRARY NAMES zstd REQUIRED)
find_path(ZSTD_INCLUDE_DIR NAMES zstd.h REQUIRED)

pkg_check_modules(JANSSON REQUIRED jansson)

message(STATUS "Found OpenMP")
message(STATUS "Found OpenSSL")
message(STATUS "Found zstd")
message(STATUS "Found Boost")
message(STATUS "Found Jansson")

include(FetchContent)

FetchContent_Declare(
        CLI11
        GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
        GIT_TAG aa4bfa0e5a56995017f060b7e03cffcd36ed265f # v2.7.2-2-gaa4bfa0
        SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/deps/CLI11"
)
FetchContent_MakeAvailable(CLI11)

# --- 2. libbdsg ---------------------------------------------------------------

# Disable tests and optional targets from bundled dependencies.
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(SDSL_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_PYTHON OFF CACHE BOOL "" FORCE)
set(BUILD_PYTHON_BINDINGS OFF CACHE BOOL "" FORCE)
set(RUN_DOXYGEN OFF CACHE BOOL "" FORCE)

# SOURCE_DIR is pinned to deps/libbdsg (rather than FetchContent's default
# build-tree location) because LIBBDSG_DIR below is reused, as-is, for
# several other paths further down this file (BUILD_RPATH, handlegraph
# include dir, GBWT/GBWTGraph's LIBRARY_PATH env for their plain `make`
# builds) that all assume libbdsg's build output lives inside its own
# source tree.
FetchContent_Declare(
        libbdsg
        GIT_REPOSITORY https://github.com/vgteam/libbdsg.git
        GIT_TAG e1ed453abb052c20ce5ff49f04c0efb697db8afc # v0.3-1017-ge1ed453
        SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/deps/libbdsg"
)
FetchContent_MakeAvailable(libbdsg)

# --- 3. sdsl-lite, GBWT, GBWTGraph --------------------------------------------

include(ExternalProject)

set(GBWT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/deps/gbwt")
set(GBWTGRAPH_DIR "${CMAKE_CURRENT_SOURCE_DIR}/deps/gbwtgraph")
set(LIBBDSG_DIR "${CMAKE_CURRENT_SOURCE_DIR}/deps/libbdsg")
set(SDSL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/deps/sdsl-lite")

ExternalProject_Add(
        sdsl_ext
        GIT_REPOSITORY https://github.com/vgteam/sdsl-lite.git
        GIT_TAG 349de444ded81547cb55a718abeada41960531b5 # v2.3.1-vgteam-21-g349de44
        SOURCE_DIR "${SDSL_DIR}"
        BINARY_DIR "${CMAKE_BINARY_DIR}/sdsl_ext-build"
        CMAKE_ARGS
        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
        -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/sdsl-install
        -DCMAKE_POLICY_VERSION_MINIMUM=3.5
        INSTALL_COMMAND ${CMAKE_COMMAND} --build . --target install -- -j1
)

set(HANDLEGRAPH_INCLUDE_DIR
        "${LIBBDSG_DIR}/bdsg/deps/libhandlegraph/src/include")
set(HANDLEGRAPH_BUILD_DIR
        "${LIBBDSG_DIR}/bdsg/deps/libhandlegraph/build")

set(GBWT_LIBRARY "${GBWT_DIR}/lib/libgbwt.a")
set(GBWTGRAPH_LIBRARY "${GBWTGRAPH_DIR}/lib/libgbwtgraph.a")
set(GBWT_INCLUDE_DIR "${GBWT_DIR}/include")
set(GBWTGRAPH_INCLUDE_DIR "${GBWTGRAPH_DIR}/include")

# ---- GBWT ------------------------------------------------------

ExternalProject_Add(
        gbwt_ext
        DEPENDS sdsl_ext
        GIT_REPOSITORY https://github.com/jltsiren/gbwt.git
        GIT_TAG f534d79b4c3265fe964b0b9c44a4859b8a7b6bf8 # v1.5-13-gf534d79
        SOURCE_DIR "${GBWT_DIR}"
        BUILD_IN_SOURCE 1
        CONFIGURE_COMMAND ""
        BUILD_COMMAND
        ${CMAKE_COMMAND} -E env
        CPATH=${CMAKE_CURRENT_SOURCE_DIR}/deps/sdsl-lite/include
        LIBRARY_PATH=${LIBBDSG_DIR}/lib
        LD_LIBRARY_PATH=${LIBBDSG_DIR}/lib
        make -j4
        INSTALL_COMMAND ""
        BUILD_BYPRODUCTS "${GBWT_LIBRARY}"
)

# ---- GBWTGRAPH -------------------------------------------------

ExternalProject_Add(
        gbwtgraph_ext
        DEPENDS gbwt_ext
        GIT_REPOSITORY https://github.com/jltsiren/gbwtgraph.git
        GIT_TAG 26760da46c471d9bb51b21faf6e1a016ab6a5bc6 # v1.4-29-g26760da
        SOURCE_DIR "${GBWTGRAPH_DIR}"
        BUILD_IN_SOURCE 1
        CONFIGURE_COMMAND ""
        BUILD_COMMAND
        ${CMAKE_COMMAND} -E env
        CPATH=${GBWT_INCLUDE_DIR}:${HANDLEGRAPH_INCLUDE_DIR}:/usr/local/include
        LIBRARY_PATH=${LIBBDSG_DIR}/lib:${GBWT_DIR}/lib:/usr/local/lib
        make -j4
        INSTALL_COMMAND ""
        BUILD_BYPRODUCTS "${GBWTGRAPH_LIBRARY}"
)

add_dependencies(gbwtgraph_ext libbdsg)

add_library(gbwt_lib STATIC IMPORTED)
set_target_properties(gbwt_lib PROPERTIES IMPORTED_LOCATION "${GBWT_LIBRARY}")
add_dependencies(gbwt_lib gbwt_ext)

add_library(gbwtgraph_lib STATIC IMPORTED)
set_target_properties(gbwtgraph_lib PROPERTIES IMPORTED_LOCATION "${GBWTGRAPH_LIBRARY}")
add_dependencies(gbwtgraph_lib gbwtgraph_ext)

# --- 4. cdx_lib -----------------------------------------------------------
#
# cdx_lib is shared between cdx_builder and cdx_coverage. In the cdx monorepo
# layout, it lives as a plain sibling directory at ../lib (cdx/lib) - not a
# submodule here, since it is developed together with both branches.
#
# CDX_LIB_DIR can be overridden (e.g. -DCDX_LIB_DIR=/path/to/cdx_lib) to
# build cdx_builder standalone (outside the cdx monorepo) against a separate
# checkout of https://github.com/JolanBoucher/cdx_lib.
set(CDX_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../lib" CACHE PATH
        "Path to the cdx_lib source tree")

if(NOT EXISTS "${CDX_LIB_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
            "cdx_lib not found at ${CDX_LIB_DIR}.\n"
            "Point CDX_LIB_DIR at a local checkout, e.g.:\n"
            "    cmake -S . -B build -DCDX_LIB_DIR=/path/to/cdx_lib\n"
            "(inside the cdx monorepo this resolves automatically to ../lib).")
endif()

# Guarded by `if(NOT TARGET cdx_lib)`: cdx_coverage (add_subdirectory'd
# alongside this project by the top-level cdx/CMakeLists.txt) also needs
# cdx_lib and points at the same ../lib directory. Whichever of the two is
# add_subdirectory'd first defines the real `cdx_lib` target; the other
# reuses it instead of trying to define a second target with the same name
# (CMake/CMP0002 error).
if(NOT TARGET cdx_lib)
    add_subdirectory("${CDX_LIB_DIR}" cdx_lib_build)
endif()
