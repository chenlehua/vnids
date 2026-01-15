# Android NDK Toolchain for VNIDS
# Use with: cmake -DCMAKE_TOOLCHAIN_FILE=cmake/android-toolchain.cmake

# This file is a wrapper - actual toolchain comes from NDK
# Just set some project-specific options

set(ANDROID_PLATFORM android-31 CACHE STRING "Android platform level")
set(ANDROID_ABI arm64-v8a CACHE STRING "Android ABI")
set(ANDROID_STL c++_static CACHE STRING "Android STL")

# Find NDK path
if(NOT DEFINED ENV{ANDROID_NDK_HOME})
    if(NOT DEFINED ENV{ANDROID_NDK})
        message(FATAL_ERROR "ANDROID_NDK_HOME or ANDROID_NDK environment variable must be set")
    else()
        set(ANDROID_NDK $ENV{ANDROID_NDK})
    endif()
else()
    set(ANDROID_NDK $ENV{ANDROID_NDK_HOME})
endif()

# Include the actual NDK toolchain
include(${ANDROID_NDK}/build/cmake/android.toolchain.cmake)

# Project-specific settings for Android
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fstack-protector-strong")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_FORTIFY_SOURCE=2")

# Disable features not available on Android
set(VNIDS_USE_SYSTEMD OFF CACHE BOOL "Disable systemd on Android")
