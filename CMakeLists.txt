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
# Both test suites default to OFF here (unlike each branch's own standalone
# default of ON) because of one remaining cross-branch dependency wrinkle:
# builder/deps/libbdsg bundles an old sdsl-lite snapshot that unconditionally
# vendors its own (old, alias-less) googletest copy. When both test suites
# are enabled in the very same configure, builder's tests happily reuse
# whatever `gtest`/`gtest_main` target already exists (they only ever need
# the plain, non-namespaced names - see builder/cmake/FetchGoogleTest.cmake),
# but coverage's tests need the newer `GTest::gtest_main` alias and will try
# to fetch a second, separate googletest copy if that alias isn't already
# there (see coverage/test/CMakeLists.txt) - which can still collide with
# libbdsg's already-vendored plain `gtest` target (CMake/CMP0002).
#
# The clean fix belongs in cdx_builder (e.g. isolate its googletest the same
# out-of-tree way it already isolates sdsl-lite/GBWT/GBWTGraph via
# ExternalProject_Add, instead of add_subdirectory), not something to
# special-case here. Until then: build/test each branch from its own
# focused configure when you need its suite (`cmake --build build --target
# cdx_builder_test...` from inside builder/, or coverage/'s own `ctest`) -
# both branches remain fully independently buildable/testable outside this
# merged configure, exactly as before.
set(CDX_BUILDER_BUILD_TESTS OFF CACHE BOOL "Build cdx_builder's own unit test suite")
set(CDX_BUILD_TESTS OFF CACHE BOOL "Build cdx_coverage's own unit test suite")

add_subdirectory(lib)
add_subdirectory(builder)
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
