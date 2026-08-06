# One standalone test executable per module, using the shared test_utils.h harness.

add_executable(test_cdx_types test_cdx_types.cpp)
target_link_libraries(test_cdx_types PRIVATE cdx_lib)
add_test(NAME test_cdx_types COMMAND test_cdx_types)

add_executable(test_cdx_format test_cdx_format.cpp)
target_link_libraries(test_cdx_format PRIVATE cdx_lib)
add_test(NAME test_cdx_format COMMAND test_cdx_format)

add_executable(test_cdx_IO test_cdx_IO.cpp)
target_link_libraries(test_cdx_IO PRIVATE cdx_lib)
add_test(NAME test_cdx_IO COMMAND test_cdx_IO)
