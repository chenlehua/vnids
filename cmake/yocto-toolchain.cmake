# Yocto/Buildroot ARM64 Toolchain for VNIDS
# Use with: cmake -DCMAKE_TOOLCHAIN_FILE=cmake/yocto-toolchain.cmake

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

# Cross compiler settings
set(CROSS_COMPILE_PREFIX "aarch64-linux-gnu-" CACHE STRING "Cross compile prefix")

set(CMAKE_C_COMPILER ${CROSS_COMPILE_PREFIX}gcc)
set(CMAKE_CXX_COMPILER ${CROSS_COMPILE_PREFIX}g++)
set(CMAKE_AR ${CROSS_COMPILE_PREFIX}ar)
set(CMAKE_RANLIB ${CROSS_COMPILE_PREFIX}ranlib)
set(CMAKE_STRIP ${CROSS_COMPILE_PREFIX}strip)

# Target environment
set(CMAKE_FIND_ROOT_PATH /usr/${CROSS_COMPILE_PREFIX})

# Search for programs in host environment
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)

# Search for libraries and headers in target environment
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# ARM64 optimizations
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=armv8-a")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fstack-protector-strong")

# Enable systemd integration for Yocto
set(VNIDS_USE_SYSTEMD ON CACHE BOOL "Enable systemd on Yocto")
