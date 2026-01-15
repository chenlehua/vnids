# VNIDS Unit Tests
# CMakeLists.txt

# Find or fetch a test framework
find_package(CMocka QUIET)
if(NOT CMocka_FOUND)
    message(STATUS "CMocka not found, unit tests will use minimal test harness")
endif()

# Test sources
set(TEST_SOURCES
    test_main.c
    test_event_queue.c
    test_config.c
    test_types.c
)

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/shared/include
    ${CMAKE_SOURCE_DIR}/vnidsd/include
)

# Create test executable
add_executable(vnids_tests ${TEST_SOURCES})

# Link against vnidsd_core library (contains all the implementation)
target_link_libraries(vnids_tests
    vnidsd_core
)

if(CMocka_FOUND)
    target_link_libraries(vnids_tests CMocka::cmocka)
    target_compile_definitions(vnids_tests PRIVATE USE_CMOCKA)
endif()

# Add tests
add_test(NAME vnids_unit_tests COMMAND vnids_tests)

# Coverage support
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(vnids_tests PRIVATE --coverage)
    target_link_options(vnids_tests PRIVATE --coverage)
endif()
