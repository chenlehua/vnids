# VNIDS - Vehicle Network Intrusion Detection System
# Root CMakeLists.txt

cmake_minimum_required(VERSION 3.20)
project(vnids VERSION 1.0.0 LANGUAGES C)

# C17 standard
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Build type defaults to Release
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wpedantic")
set(CMAKE_C_FLAGS_DEBUG "-g -O0 -DDEBUG")
set(CMAKE_C_FLAGS_RELEASE "-O2 -DNDEBUG")

# ARM64 optimizations
if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=armv8-a")
endif()

# Options
option(BUILD_TESTS "Build unit tests" ON)
option(BUILD_FUZZER "Build fuzz tests" OFF)
option(ENABLE_SANITIZERS "Enable address/undefined sanitizers" OFF)

if(ENABLE_SANITIZERS)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address,undefined")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address,undefined")
endif()

# Find required packages
find_package(Threads REQUIRED)

# SQLite3 - use system or bundled
find_package(SQLite3 QUIET)
if(NOT SQLite3_FOUND)
    message(STATUS "System SQLite3 not found, using bundled version")
    add_subdirectory(third_party/sqlite3)
    set(SQLite3_LIBRARIES sqlite3)
    set(SQLite3_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/third_party/sqlite3)
else()
    message(STATUS "Using system SQLite3")
    set(SQLite3_LIBRARIES SQLite::SQLite3)
endif()

# cJSON - use system or bundled
find_library(CJSON_LIBRARY cjson)
if(NOT CJSON_LIBRARY)
    message(STATUS "System cJSON not found, using bundled version")
    add_subdirectory(third_party/cjson)
    set(CJSON_LIBRARIES cjson)
    set(CJSON_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/third_party/cjson)
else()
    message(STATUS "Using system cJSON")
    set(CJSON_LIBRARIES ${CJSON_LIBRARY})
    set(CJSON_INCLUDE_DIRS "")
endif()

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/shared/include)

# Subdirectories
add_subdirectory(vnidsd)
add_subdirectory(vnids-cli)

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests/unit)
endif()

# Installation
install(DIRECTORY deploy/ DESTINATION share/vnids/deploy)
install(DIRECTORY suricata/rules/ DESTINATION share/vnids/rules)

# Package configuration
set(CPACK_PACKAGE_NAME "vnids")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Vehicle Network Intrusion Detection System")
include(CPack)
