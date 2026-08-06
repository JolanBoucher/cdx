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
# Pull in cdx_builder and cdx_coverage, reused here as libraries
# (cdx_builder_core / cdx_coverage_core - see their own CMakeLists.txt).
# Each subproject still also produces its own standalone executable
# (cdx_builder / cdx_coverage) for independent branch-level development;
# only their unit test suites are switched off here to keep this merged
# configure/build lean - run `ctest` inside each subproject directly to
# test a branch in isolation.
#
# Resolved in two possible layouts, checked in this order:
#   1. Git submodules living directly under this project (cdx/cdx_builder,
#      cdx/cdx_coverage) - the supported layout for `git clone --recursive`.
#   2. Sibling checkouts next to this project (../cdx_builder,
#      ../cdx_coverage) - kept for local development against independently
#      cloned working copies of either repo (e.g. testing an unpushed change
#      to cdx_builder without first updating the submodule pointer).
# ---------------------------------------------------------------------------
set(CDX_BUILDER_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(CDX_BUILD_TESTS OFF CACHE BOOL "" FORCE)

get_filename_component(CDX_SIBLING_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/cdx_builder/CMakeLists.txt")
    set(CDX_BUILDER_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/cdx_builder")
else()
    set(CDX_BUILDER_SOURCE_DIR "${CDX_SIBLING_ROOT}/cdx_builder")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/cdx_coverage/CMakeLists.txt")
    set(CDX_COVERAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/cdx_coverage")
else()
    set(CDX_COVERAGE_SOURCE_DIR "${CDX_SIBLING_ROOT}/cdx_coverage")
endif()

if(NOT EXISTS "${CDX_BUILDER_SOURCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
            "cdx_builder was not found, neither as a submodule "
            "(cdx/cdx_builder) nor as a sibling directory (${CDX_SIBLING_ROOT}/cdx_builder).\n"
            "Run 'git submodule update --init --recursive', or check out "
            "cdx_builder next to this project."
    )
endif()

if(NOT EXISTS "${CDX_COVERAGE_SOURCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
            "cdx_coverage was not found, neither as a submodule "
            "(cdx/cdx_coverage) nor as a sibling directory (${CDX_SIBLING_ROOT}/cdx_coverage).\n"
            "Run 'git submodule update --init --recursive', or check out "
            "cdx_coverage next to this project."
    )
endif()

add_subdirectory("${CDX_BUILDER_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/cdx_builder")
add_subdirectory("${CDX_COVERAGE_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/cdx_coverage")

# ---------------------------------------------------------------------------
# cdx executable (dispatcher)
# ---------------------------------------------------------------------------
# Deliberately NOT a CLI11::App: the dispatch logic here (sniff.h/.cpp +
# main.cpp) only ever needs to look at the leading file argument(s) to pick
# a branch, then hands the *entire, unmodified* argv to that branch's own
# CLI11::App (cdx_builder::run / cdx_coverage::run). See main.cpp's file
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
# resolvePythonExecutable() in cdx_coverage/src/python_circular_plot.cpp,
# which use /proc/self/exe - not argv[0]). The cdx_coverage subproject above
# already provisions both next to its own standalone binary; replicate the
# same two steps next to the merged cdx executable so circular plots work
# from `cdx` too, not just from the standalone cdx_coverage binary.
add_custom_command(TARGET cdx POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${CDX_COVERAGE_SOURCE_DIR}/python_script/circular_plot.py"
        "$<TARGET_FILE_DIR:cdx>/circular_plot.py"
        COMMENT "Copying circular_plot.py next to cdx"
)

find_package(Python3 COMPONENTS Interpreter)
if(Python3_FOUND)
    # Never fails the build (see cdx_coverage/cmake/setup_circular_env.sh):
    # any problem, e.g. no network access, is a warning, not a build error.
    add_custom_command(TARGET cdx POST_BUILD
            COMMAND bash
            "${CDX_COVERAGE_SOURCE_DIR}/cmake/setup_circular_env.sh"
            "$<TARGET_FILE_DIR:cdx>/pyenv"
            "${CDX_COVERAGE_SOURCE_DIR}/python_script/requirements.txt"
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
install(FILES "${CDX_COVERAGE_SOURCE_DIR}/python_script/circular_plot.py" DESTINATION bin)
install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/pyenv" DESTINATION bin OPTIONAL)
