cmake_minimum_required(VERSION 3.16)
project(cdx_builder CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# ------------------------------------------------------------------------------
# 1. Build configuration
# ------------------------------------------------------------------------------

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(CompilerRequirements)

# ------------------------------------------------------------------------------
# 2. Dependencies
# ------------------------------------------------------------------------------
#
# See cmake/AppleDependencies.cmake and cmake/ExternalDeps.cmake for details.
# Both define variables/targets consumed below and by tests/CMakeLists.txt.

if(APPLE)
    include(AppleDependencies)
endif()

include(ExternalDeps)

# ------------------------------------------------------------------------------
# 3. Core library target (everything except main.cpp)
# ------------------------------------------------------------------------------
# Split out as its own static library, exposing cdx_builder::run(argc, argv)
# (see src/builder_app.h), so the GBZ-indexing pipeline can be called
# directly as a function - both by this project's own standalone executable
# below, and by the merged "cdx" toolkit executable (../cdx/), which links
# against cdx_builder_core instead of spawning this binary as a subprocess.

add_library(cdx_builder_core STATIC
        src/builder_app.cpp
        src/builder_app.h
        src/gbz_IO.cpp
        src/gbz_IO.h
        src/constant.h
        src/graph_linearization.cpp
        src/graph_linearization.h
        src/CSRMatrix.cpp
        src/graph_indexing.cpp
        src/graph_indexing.h
        src/cli.cpp
        src/cli.hpp
        src/cdx_writer.cpp
        src/cdx_writer.h
)

set_target_properties(cdx_builder_core PROPERTIES
        BUILD_RPATH "${LIBBDSG_DIR}/lib"
)

# PUBLIC: these headers/libraries must also reach any target linking
# cdx_builder_core (e.g. the "cdx" toolkit executable), not just this file.
target_include_directories(cdx_builder_core PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${GBWT_INCLUDE_DIR}
        ${GBWTGRAPH_INCLUDE_DIR}
        ${ZSTD_INCLUDE_DIR}
        ${JANSSON_INCLUDE_DIRS}
        ${Boost_INCLUDE_DIRS}
)

target_link_libraries(cdx_builder_core PUBLIC
        cdx_lib
        libbdsg
        gbwtgraph_lib
        gbwt_lib
        OpenSSL::Crypto
        ${ZSTD_LIBRARY}
        ${JANSSON_LIBRARIES}
        OpenMP::OpenMP_CXX
        Threads::Threads
        CLI11::CLI11
        z
)

# ------------------------------------------------------------------------------
# 3bis. Standalone executable target
# ------------------------------------------------------------------------------
# Kept for independent development/testing of this branch alone. Just a thin
# main() calling cdx_builder::run() - see src/main.cpp.

add_executable(cdx_builder src/main.cpp)

set_target_properties(cdx_builder PROPERTIES
        BUILD_RPATH "${LIBBDSG_DIR}/lib"
)

target_link_libraries(cdx_builder PRIVATE cdx_builder_core)

# ------------------------------------------------------------------------------
# 4. Unit tests (Google Test)
# ------------------------------------------------------------------------------
#
# Each testable module gets its own executable, declared in tests/CMakeLists.txt,
# linked only against the dependencies it actually needs.

option(CDX_BUILDER_BUILD_TESTS "Build the cdx_builder unit test suites" ON)

if(CDX_BUILDER_BUILD_TESTS)
    enable_testing()
    include(FetchGoogleTest)
    add_subdirectory(tests)
endif()
