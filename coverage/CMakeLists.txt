cmake_minimum_required(VERSION 3.16)

# Fix policy warnings BEFORE project()
if(POLICY CMP0074)
    cmake_policy(SET CMP0074 NEW)
endif()

project(
        cdx_coverage
        VERSION 0.1.0
        DESCRIPTION "Compute coverage from a pangenome using a CDX index and GAM alignments"
        LANGUAGES CXX
)

# ---------------------------------------------------------------------------
# Project configuration
# ---------------------------------------------------------------------------
# Default to a Release build (optimized) whenever the build type isn't
# explicitly requested. Without this, a plain `cmake -S . -B build` (no
# -DCMAKE_BUILD_TYPE) leaves CMAKE_BUILD_TYPE empty, i.e. no optimization
# flags at all. This only affects single-config generators (Makefiles,
# Ninja); multi-config generators (Xcode, Visual Studio) ignore it and pick
# the config at build time instead - and any explicit -DCMAKE_BUILD_TYPE=...
# (e.g. CLion's own per-profile setting, or CI's Release flag) still wins.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# ---------------------------------------------------------------------------
# Verify libvgio system dependencies
# ---------------------------------------------------------------------------
find_package(PkgConfig REQUIRED)
find_package(Protobuf REQUIRED)
find_package(Threads REQUIRED)

# ---------------------------------------------------------------------------
# Abseil (needed by libvgio's Protobuf-based logging - see the
# target_link_libraries(... absl::log_internal_message ...) calls further
# below)
# ---------------------------------------------------------------------------
# Not every platform ships a usable Abseil: Homebrew's modern Protobuf on
# macOS pulls it in as a sibling package, but e.g. Ubuntu 20.04's own apt
# Protobuf (3.6.1) predates Abseil entirely, and Ubuntu doesn't package
# libabsl-dev before 22.04 (jammy). So: prefer whatever the system already
# provides (fast, no network needed), and fall back to auto-fetching a
# pinned Abseil release otherwise - the same approach already used for
# CLI11/libbdsg/GBWT/GBWTGraph/sdsl-lite (builder/) and libvgio (below).
find_package(absl CONFIG QUIET)

if(NOT absl_FOUND)
    message(STATUS "System Abseil not found - fetching a pinned release instead.")
    include(FetchContent)
    FetchContent_Declare(
            absl
            GIT_REPOSITORY https://github.com/abseil/abseil-cpp.git
            GIT_TAG 20230802.1 # LTS branch, Aug 2023, Patch 1
            SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/deps/abseil-cpp"
    )
    set(ABSL_PROPAGATE_CXX_STD ON CACHE BOOL "" FORCE)
    set(ABSL_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(absl)
endif()

# Homebrew installs libomp as a keg-only package on Apple Silicon / Intel macOS
if(APPLE)
    if(EXISTS "/opt/homebrew/opt/libomp")
        set(LIBOMP_PREFIX "/opt/homebrew/opt/libomp")
    elseif(EXISTS "/usr/local/opt/libomp")
        set(LIBOMP_PREFIX "/usr/local/opt/libomp")
    endif()

    if(LIBOMP_PREFIX)
        set(OpenMP_ROOT "${LIBOMP_PREFIX}")
        set(OpenMP_CXX_FLAGS "-Xpreprocessor -fopenmp -I${LIBOMP_PREFIX}/include")
        set(OpenMP_CXX_LIB_NAMES "omp")
        set(OpenMP_omp_LIBRARY "${LIBOMP_PREFIX}/lib/libomp.dylib")
    endif()
endif()

find_package(OpenMP REQUIRED COMPONENTS CXX)

# ---------------------------------------------------------------------------
# CLI11 (single-header command-line parser)
# ---------------------------------------------------------------------------
# Not vendored in this repo. Resolved portably so both the main executable
# and the unit tests (test/CMakeLists.txt) find the same header regardless
# of how it was installed on the host: a CLI11Config.cmake package (Homebrew,
# apt's libcli11-dev, vcpkg, conan, ...), or a bare single-header drop
# reachable via one of the hint paths below or CMAKE_PREFIX_PATH/CPATH.
find_package(CLI11 CONFIG QUIET)

if(NOT TARGET CLI11::CLI11)
    find_path(CLI11_INCLUDE_DIR
            NAMES CLI/CLI.hpp
            HINTS
            "/opt/homebrew/opt/cli11/include"
            "/usr/local/opt/cli11/include"
            "${CMAKE_CURRENT_SOURCE_DIR}/include"
    )

    if(NOT CLI11_INCLUDE_DIR)
        message(FATAL_ERROR
                "CLI11 (CLI/CLI.hpp) was not found. Install it, e.g.:\n"
                "  macOS:  brew install cli11\n"
                "  Ubuntu: apt-get install libcli11-dev\n"
                "  (or drop the single-header release as ./include/CLI/CLI.hpp)\n"
                "Then reconfigure."
        )
    endif()

    message(STATUS "Using CLI11 headers from: ${CLI11_INCLUDE_DIR}")

    add_library(CLI11::CLI11 INTERFACE IMPORTED)
    target_include_directories(CLI11::CLI11 INTERFACE "${CLI11_INCLUDE_DIR}")
endif()

# libvgio requires HTSlib 1.10 or newer.
pkg_check_modules(
        HTSLIB
        REQUIRED
        IMPORTED_TARGET
        htslib>=1.10
)

# Cairo is used for in-process linear coverage plot rendering (no subprocess).
pkg_check_modules(
        CAIRO
        REQUIRED
        IMPORTED_TARGET
        cairo
)

# Circular coverage plots are rendered by a Python (pycirclize) subprocess
# (src/python_circular_plot.cpp + python_script/circular_plot.py) - see
# CIRCULAR_PLOT_SETUP.md. Python is only needed at RUNTIME for that one
# feature (--component-type circular); this project must still compile with
# no Python present. This lookup is purely informational (a configure-time
# warning), never REQUIRED. If found, a POST_BUILD step further below
# auto-provisions a private venv with numpy/matplotlib/pycirclize next to
# the built executable, so end users don't have to set one up by hand.
find_package(Python3 COMPONENTS Interpreter)
if(NOT Python3_FOUND)
    message(STATUS
            "Python 3 interpreter not found at configure time. This is fine: "
            "cdx_coverage compiles without it. It is only needed at RUNTIME to "
            "render circular graphs (--component-type circular). A Python 3 "
            "install would also let this build auto-provision numpy/matplotlib/"
            "pycirclize for you - see CIRCULAR_PLOT_SETUP.md."
    )
endif()

# ---------------------------------------------------------------------------
# Build libvgio as part of this project
# ---------------------------------------------------------------------------
# Fetched automatically at configure time via FetchContent (no manual `git
# submodule` step required), pinned to the exact commit this project was
# last verified against. SOURCE_DIR/BINARY_DIR are pinned explicitly so the
# rest of this file (LIBVGIO_BINARY_DIR, the proto symlink fixup below) can
# keep referring to fixed, predictable paths.
set(LIBVGIO_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/deps/libvgio")

# Silence policy warning for CMP0074 in subdirectories
if(POLICY CMP0074)
    cmake_policy(SET CMP0074 NEW)
endif()

# Prevent libvgio from invoking find_package(Protobuf) a second time
set(CMAKE_DISABLE_FIND_PACKAGE_Protobuf TRUE)

include(FetchContent)

FetchContent_Declare(
        libvgio
        GIT_REPOSITORY https://github.com/vgteam/libvgio.git
        GIT_TAG fbc67c8289cd7fbbf03bc3b30ff15b72d0bd5bda # heads/master
        SOURCE_DIR "${LIBVGIO_SOURCE_DIR}"
        BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/deps/libvgio"
)

# libvgio's own CMakeLists.txt unconditionally calls both
# install(TARGETS vgio EXPORT vgio-targets ...) and, separately,
# export(EXPORT vgio-targets ...) to write a build-tree package file. Neither
# is needed here (we only build + link libvgio inside this project's own
# tree, never install/export it as a redistributable CMake package), and
# both fail at configure time specifically when the Abseil linked into it
# came from our own FetchContent fallback above rather than a
# system/Homebrew package: FetchContent-built Abseil targets aren't
# IMPORTED, so they aren't "export-set safe". This only happens in the
# absl_FOUND-false branch - when a real system Abseil was found (e.g.
# Homebrew on macOS, whose IMPORTED targets are export-set safe), leave
# install()/export() completely untouched, exactly as this already worked
# before the FetchContent fallback existed. CMake auto-provides each
# shadowed built-in as `_install`/`_export` so they can be restored
# immediately after.
if(NOT absl_FOUND)
    function(install)
    endfunction()
    function(export)
    endfunction()
endif()

FetchContent_MakeAvailable(libvgio)

if(NOT absl_FOUND)
    function(install)
        _install(${ARGN})
    endfunction()
    function(export)
        _export(${ARGN})
    endfunction()
endif()

# Re-enable find_package(Protobuf) for the rest of your project
set(CMAKE_DISABLE_FIND_PACKAGE_Protobuf FALSE)

# ---------------------------------------------------------------------------
# Normalize the libvgio target name & link Abseil
# ---------------------------------------------------------------------------
if(TARGET VGio::VGio)
    set(CDX_VGIO_TARGET VGio::VGio)
elseif(TARGET vgio)
    set(CDX_VGIO_TARGET vgio)
elseif(TARGET libvgio)
    set(CDX_VGIO_TARGET libvgio)
else()
    message(FATAL_ERROR "libvgio was configured, but no recognized CMake target was found.")
endif()

message(STATUS "Using libvgio target: ${CDX_VGIO_TARGET}")

# ---------------------------------------------------------------------------
# Portability fix: make sure "vg/vg.pb.h" resolves no matter which Protobuf
# CMake integration the host machine's libvgio build picked (see
# cmake/fix_libvgio_proto_symlink.cmake for the full explanation). This is
# self-healing on every machine/OS - no manual submodule patching needed.
# ---------------------------------------------------------------------------
set(LIBVGIO_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/deps/libvgio")

add_custom_command(
        OUTPUT "${LIBVGIO_BINARY_DIR}/.vg_proto_fixup_stamp"
        COMMAND "${CMAKE_COMMAND}"
                -DLIBVGIO_BINARY_DIR=${LIBVGIO_BINARY_DIR}
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/fix_libvgio_proto_symlink.cmake"
        COMMAND "${CMAKE_COMMAND}" -E touch "${LIBVGIO_BINARY_DIR}/.vg_proto_fixup_stamp"
        DEPENDS run_protoc
        COMMENT "Ensuring vg/vg.pb.h is reachable regardless of the host's Protobuf CMake integration"
        VERBATIM
)
add_custom_target(cdx_fix_libvgio_proto_include ALL
        DEPENDS "${LIBVGIO_BINARY_DIR}/.vg_proto_fixup_stamp")
add_dependencies(cdx_fix_libvgio_proto_include run_protoc)

# Make "vg/vg.pb.h" resolve for every target that compiles libvgio sources
# (both the shared and static variants build their own copies of
# alignment_emitter.cpp etc.) as well as anything linking against libvgio
# downstream (e.g. cdx_coverage itself, via the PUBLIC/BUILD_INTERFACE
# propagation below).
foreach(_vgio_target vgio vgio_static)
    if(TARGET ${_vgio_target})
        add_dependencies(${_vgio_target} cdx_fix_libvgio_proto_include)
        target_include_directories(${_vgio_target} PUBLIC
                $<BUILD_INTERFACE:${LIBVGIO_BINARY_DIR}/cdx_vg_include>)
    endif()
endforeach()

# Link Abseil components required by Protobuf into vgio
target_link_libraries(${CDX_VGIO_TARGET} PRIVATE
        absl::log_internal_message
        absl::log_internal_check_op
        absl::hash
        absl::status
        absl::raw_logging_internal
)

# ---------------------------------------------------------------------------
# cdx_coverage_core (everything except main.cpp)
# ---------------------------------------------------------------------------
# Split out as its own static library, exposing cdx_coverage::run(argc, argv)
# (see src/coverage_app.h), so the inspect/query/coverage pipelines can be
# called directly as a function - both by this project's own standalone
# executable below, and by the merged "cdx" toolkit executable (../cdx/),
# which links against cdx_coverage_core instead of spawning this binary as a
# subprocess.
add_library(cdx_coverage_core STATIC
        src/coverage_app.cpp
        src/coverage_app.h
        src/gam_io.cpp
        src/gam_io.h
        src/config.h
        src/coverage_precision.h
        src/coverage_gaps.cpp
        src/coverage_gaps.h
        src/cdx_loader.cpp
        src/cdx_loader.h
        src/cov_projection.cpp
        src/cov_projection.h
        src/output_stats.cpp
        src/output_stats.h
        src/output_coverage.cpp
        src/output_coverage.h
        src/cli.cpp
        src/cli.hpp
        src/query_resolver.cpp
        src/query_resolver.h
        src/linear_plot.cpp
        src/output_plot.h
        src/python_circular_plot.cpp
        src/cairo_plot.cpp
        src/query_plot_slice.h
        src/query_plot_slice.cpp
)

target_compile_features(cdx_coverage_core PUBLIC cxx_std_17)

# PUBLIC: these headers must also reach any target linking cdx_coverage_core
# (e.g. the "cdx" toolkit executable), not just this file.
target_include_directories(cdx_coverage_core PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

# Dev-time fallback so python_circular_plot.cpp can find circular_plot.py
# straight from the source tree when running the binary directly out of a
# build directory (before any install step ran). At runtime, the script is
# looked up next to the executable first (see the copy/install rules below);
# this compile definition is only consulted if that lookup fails.
target_compile_definitions(cdx_coverage_core PUBLIC
        CDX_CIRCULAR_PLOT_SCRIPT_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/python_script"
)

# ---------------------------------------------------------------------------
# cdx_coverage standalone executable
# ---------------------------------------------------------------------------
# Kept for independent development/testing of this branch alone. Just a thin
# main() calling cdx_coverage::run() - see src/main.cpp.
add_executable(cdx_coverage src/main.cpp)

target_link_libraries(cdx_coverage PRIVATE cdx_coverage_core)

# ---------------------------------------------------------------------------
# cdx_lib (CDX format types and I/O)
# ---------------------------------------------------------------------------
# In the cdx monorepo layout, cdx_lib lives as a plain sibling directory at
# ../lib (cdx/lib) - not fetched from GitHub here, since it is developed
# together with both cdx_builder and cdx_coverage.
#
# CDX_LIB_DIR can be overridden (e.g. -DCDX_LIB_DIR=/path/to/cdx_lib) to
# build cdx_coverage standalone (outside the cdx monorepo) against a
# separate checkout of https://github.com/JolanBoucher/cdx_lib.
#
# Guarded by `if(NOT TARGET cdx_lib)`: cdx_builder (add_subdirectory'd
# alongside this project by the top-level cdx/CMakeLists.txt) also needs
# cdx_lib and points at the same ../lib directory. Whichever of the two is
# add_subdirectory'd first defines the real `cdx_lib` target; the other
# reuses it instead of trying to define a second target with the same name
# (CMake/CMP0002 error).
set(CDX_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../lib" CACHE PATH
        "Path to the cdx_lib source tree")

if(NOT TARGET cdx_lib)
    if(NOT EXISTS "${CDX_LIB_DIR}/CMakeLists.txt")
        message(FATAL_ERROR
                "cdx_lib not found at ${CDX_LIB_DIR}.\n"
                "Point CDX_LIB_DIR at a local checkout, e.g.:\n"
                "    cmake -S . -B build -DCDX_LIB_DIR=/path/to/cdx_lib\n"
                "(inside the cdx monorepo this resolves automatically to ../lib).")
    endif()

    add_subdirectory("${CDX_LIB_DIR}" cdx_lib_build)
endif()

target_link_libraries(cdx_coverage_core PUBLIC
        cdx_lib
        ${CDX_VGIO_TARGET}
        protobuf::libprotobuf
        absl::log_internal_message
        absl::log_internal_check_op
        absl::hash
        absl::status
        absl::raw_logging_internal
        PkgConfig::CAIRO
        CLI11::CLI11
)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(cdx_coverage_core PUBLIC
            -Wall
            -Wextra
            -Wpedantic
    )
endif()

# Copy circular_plot.py next to the built executable so it is found there at
# runtime (see resolveCircularPlotScript in src/python_circular_plot.cpp)
# without requiring an install step during development.
#
# NOT a `TARGET cdx_coverage POST_BUILD` command: those only fire when the
# cdx_coverage TARGET itself gets rebuilt/relinked. Editing only
# circular_plot.py (no C++ change) would then leave a stale copy next to the
# executable - `cmake --build` sees cdx_coverage is already up to date and
# skips it entirely, silently running old Python logic. Tying this to its
# own OUTPUT/DEPENDS instead makes `cmake --build` re-copy it on its own
# whenever the source .py file's content changes, independent of the C++
# target's build state.
set(CDX_CIRCULAR_PLOT_SCRIPT_SRC "${CMAKE_CURRENT_SOURCE_DIR}/python_script/circular_plot.py")
set(CDX_CIRCULAR_PLOT_SCRIPT_DEST "${CMAKE_CURRENT_BINARY_DIR}/circular_plot.py")

add_custom_command(
        OUTPUT "${CDX_CIRCULAR_PLOT_SCRIPT_DEST}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CDX_CIRCULAR_PLOT_SCRIPT_SRC}"
        "${CDX_CIRCULAR_PLOT_SCRIPT_DEST}"
        DEPENDS "${CDX_CIRCULAR_PLOT_SCRIPT_SRC}"
        COMMENT "Copying circular_plot.py next to cdx_coverage"
)
add_custom_target(cdx_coverage_circular_plot_script ALL
        DEPENDS "${CDX_CIRCULAR_PLOT_SCRIPT_DEST}"
)
add_dependencies(cdx_coverage cdx_coverage_circular_plot_script)

# Auto-provision a private venv (numpy/matplotlib/pycirclize) next to the
# built executable, so building cdx_coverage on a fresh machine is enough to
# get working circular graphs too - no one has to create/activate a venv by
# hand. Only attempted if a Python 3 interpreter was found at configure time.
# NEVER fails the build (see cmake/setup_circular_env.sh: any problem, e.g.
# no network access, is a warning, not a build error). At runtime,
# resolvePythonExecutable() (src/python_circular_plot.cpp) prefers this
# bundled venv over the system "python3" when present, and
# CDX_PYTHON_EXECUTABLE always overrides both.
if(Python3_FOUND)
    add_custom_command(TARGET cdx_coverage POST_BUILD
            COMMAND bash
            "${CMAKE_CURRENT_SOURCE_DIR}/cmake/setup_circular_env.sh"
            "$<TARGET_FILE_DIR:cdx_coverage>/pyenv"
            "${CMAKE_CURRENT_SOURCE_DIR}/python_script/requirements.txt"
            "${Python3_EXECUTABLE}"
            COMMENT "Provisioning bundled Python environment for circular graphs (best-effort)"
    )
endif()

# ---------------------------------------------------------------------------
# Installation
# ---------------------------------------------------------------------------
install(TARGETS cdx_coverage DESTINATION bin)
install(FILES python_script/circular_plot.py DESTINATION bin)

# Bundle the provisioned venv with `make install` too, if it was built
# successfully (OPTIONAL: silently skipped if provisioning failed/was
# skipped, e.g. no network access during the build).
install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/pyenv" DESTINATION bin OPTIONAL)

# ---------------------------------------------------------------------------
# Unit tests
# ---------------------------------------------------------------------------
# CDX_BUILD_TESTS lets the tests (and their GoogleTest download/build) be
# skipped entirely, e.g. for a minimal/offline build of cdx_coverage itself.
option(CDX_BUILD_TESTS "Build unit tests (requires network access to fetch GoogleTest on first configure)" ON)

if(CDX_BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()