cmake_minimum_required(VERSION 3.16)

project(cdx_lib LANGUAGES CXX)

add_library(
        cdx_lib STATIC
        src/cdx_format.cpp
        src/cdx_IO.cpp
)

target_compile_features(
        cdx_lib
        PUBLIC
        cxx_std_17
)

target_include_directories(
        cdx_lib
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_sources(
        cdx_lib
        PUBLIC
        include/cdx_format.h
        include/cdx_types.h
        include/cdx_IO.h
)

enable_testing()
add_subdirectory(tests)