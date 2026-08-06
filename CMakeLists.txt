cmake_minimum_required(VERSION 3.16)

if(POLICY CMP0074)
    cmake_policy(SET CMP0074 NEW)
endif()

project(
        cdx_toolkit
        VERSION 0.1.0
        DESCRIPTION "Single entry point for the CDX pangenome toolkit (build / inspect / coverage)"
        LANGUAGES CXX
)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# ---------------------------------------------------------------------------
# Monorepo layout
# ---------------------------------------------------------------------------
# cdx_lib, cdx_builder and cdx_coverage - previously three separate
# repositories - live directly under this project as plain subdirectories:
#
#   lib/       CDX format types and binary I/O, shared by both branches.
#   builder/   GBZ -> CDX index construction.
#   coverage/  CDX inspection + GAM coverage computation/reporting.
#
# Third-party dependencies each branch vendors for itself (CLI11, libbdsg,
# GBWT, GBWTGraph, sdsl-lite under builder/deps/; libvgio under
# coverage/deps/) remain git submodules pointing at their real upstream
# repositories - only the code that's actually part of *this* project
# (lib/builder/coverage) was merged.
#
# add_subdirectory(lib) runs first so the `cdx_lib` target it defines
# already exists by the time builder/ and coverage/ each ask for it (see
# their own CMakeLists.txt / cmake/ExternalDeps.cmake - both guard their own
# cdx_lib resolution with `if(NOT TARGET cdx_lib)` so exactly one real
# target ever gets defined, regardless of add_subdirectory order).
#
# Each of builder/ and coverage/ still also produces its own standalone
# executable (cdx_builder / cdx_coverage) and its own unit test suite.
#
# Both test suites default to ON here, same as each branch's own standalone
# default - all three suites (lib/builder/coverage) build together in this
# one merged configure, share ONE GoogleTest build, and are all runnable
# from a single CLion profile / `ctest` invocation. This relies on one
# cross-branch bridge, right below add_subdirectory(builder): see the
# comment there for the full story (short version: builder/deps/libbdsg
# already vendors a modern, perfectly usable googletest v1.15.2 copy -
# add_subdirectory()'d raw, so missing the namespaced `GTest::` alias
# coverage's tests want - and this project adds that one missing alias
# instead of letting coverage fetch + build a second, separate copy, which
# is what used to collide with libbdsg's copy - CMake/CMP0002 - when both
# suites were enabled together).
#
# Both remain fully independently buildable/testable outside this merged
# configure too, exactly as before (`cmake -S builder -B ...`, `cmake -S
# coverage -B ...`).
set(CDX_BUILDER_BUILD_TESTS ON CACHE BOOL "Build cdx_builder's own unit test suite")
set(CDX_BUILD_TESTS ON CACHE BOOL "Build cdx_coverage's own unit test suite")

# lib/CMakeLists.txt, and builder's/coverage's own CMakeLists.txt (when their
# respective *_BUILD_TESTS is ON), each call enable_testing() themselves -
# that's enough to make `ctest`/"Run All Tests" work when any of the three is
# configured standalone (its own CMakeLists.txt is then the top of the build
# tree). But CTest only walks into a subdirectory's tests if *every* level
# from the actual top of the build tree down to it also called
# enable_testing() - lib/builder/coverage being add_subdirectory()'d here
# without this project itself ever calling it left the root
# CTestTestfile.cmake with no entries at all pointing into any of them, so
# `ctest`/CLion's "All CTest"/"CoverageTest" run configurations (which invoke
# ctest from this project's own build dir root) always reported "No tests
# were found!!!" - even though every individual test executable still built
# and ran fine on its own (that doesn't go through CTest's directory tree at
# all). This one call closes that gap.
enable_testing()

add_subdirectory(lib)
add_subdirectory(builder)

# Bridge builder/deps/libbdsg's vendored googletest (plain `gtest`/
# `gtest_main` targets, defined unconditionally the moment libbdsg itself is
# configured above - see builder/cmake/FetchGoogleTest.cmake) to the
# namespaced `GTest::gtest`/`GTest::gtest_main` names coverage/test/'s suite
# links against. Without this, coverage/test/CMakeLists.txt's own
# `if(NOT TARGET GTest::gtest_main)` guard would find nothing and fetch a
# second, separate googletest copy - which then collides with libbdsg's
# already-vendored plain `gtest` target at CMake generation time
# (add_library cannot create target 'gtest' twice: CMP0002). Both copies are
# pinned to the exact same v1.15.2 release, so this is a genuine, safe reuse
# of the same code, not a version downgrade/mismatch. Guarded so a
# standalone `cmake -S builder ...` configure (CDX_BUILD_TESTS/coverage not
# involved at all) doesn't try to alias a target that was never fetched.
if(TARGET gtest AND NOT TARGET GTest::gtest_main)
    add_library(GTest::gtest ALIAS gtest)
    add_library(GTest::gtest_main ALIAS gtest_main)
endif()

add_subdirectory(coverage)

# ---------------------------------------------------------------------------
# cdx executable (dispatcher)
# ---------------------------------------------------------------------------
# Deliberately NOT a CLI11::App: the dispatch logic here (src/sniff.h/.cpp +
# src/main.cpp) only ever needs to look at the leading file argument(s) to
# pick a branch, then hands the *entire, unmodified* argv to that branch's
# own CLI11::App (cdx_builder::run / cdx_coverage::run, from
# cdx_builder_core / cdx_coverage_core above). See src/main.cpp's file
# docstring for why the two branches are never merged into one App.
add_executable(cdx
        src/main.cpp
        src/sniff.h
        src/sniff.cpp
)

target_link_libraries(cdx PRIVATE
        cdx_builder_core
        cdx_coverage_core
)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(cdx PRIVATE
            -Wall
            -Wextra
            -Wpedantic
    )
endif()

# ---------------------------------------------------------------------------
# Circular-plot runtime assets (Python subprocess backend)
# ---------------------------------------------------------------------------
# cdx_coverage's circular-graph rendering (--component-type circular) shells
# out to python_script/circular_plot.py and is looked up next to the
# *running* executable at runtime (see resolveCircularPlotScript() /
# resolvePythonExecutable() in coverage/src/python_circular_plot.cpp, which
# use /proc/self/exe - not argv[0]). The coverage subproject above already
# provisions both next to its own standalone binary; replicate the same two
# steps next to the merged cdx executable so circular plots work from `cdx`
# too, not just from the standalone cdx_coverage binary.
add_custom_command(TARGET cdx POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/coverage/python_script/circular_plot.py"
        "$<TARGET_FILE_DIR:cdx>/circular_plot.py"
        COMMENT "Copying circular_plot.py next to cdx"
)

find_package(Python3 COMPONENTS Interpreter)
if(Python3_FOUND)
    # Never fails the build (see coverage/cmake/setup_circular_env.sh): any
    # problem, e.g. no network access, is a warning, not a build error.
    add_custom_command(TARGET cdx POST_BUILD
            COMMAND bash
            "${CMAKE_CURRENT_SOURCE_DIR}/coverage/cmake/setup_circular_env.sh"
            "$<TARGET_FILE_DIR:cdx>/pyenv"
            "${CMAKE_CURRENT_SOURCE_DIR}/coverage/python_script/requirements.txt"
            "${Python3_EXECUTABLE}"
            COMMENT "Provisioning bundled Python environment for circular graphs (best-effort)"
    )
else()
    message(STATUS
            "Python 3 interpreter not found at configure time. This is fine: "
            "cdx compiles without it. It is only needed at RUNTIME to render "
            "circular graphs (--component-type circular)."
    )
endif()

# ---------------------------------------------------------------------------
# Installation
# ---------------------------------------------------------------------------
install(TARGETS cdx DESTINATION bin)
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/coverage/python_script/circular_plot.py" DESTINATION bin)
install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/pyenv" DESTINATION bin OPTIONAL)
