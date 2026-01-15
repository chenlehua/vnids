# VNIDS CLI Tool
# CMakeLists.txt

set(VNIDS_CLI_SOURCES
    src/main.c
    src/client.c
    src/commands.c
    src/output.c
)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/shared/include
    ${CJSON_INCLUDE_DIRS}
)

# Create executable
add_executable(vnids-cli ${VNIDS_CLI_SOURCES})

# Link libraries
target_link_libraries(vnids-cli
    Threads::Threads
    ${CJSON_LIBRARIES}
)

# Compiler definitions
target_compile_definitions(vnids-cli PRIVATE
    _GNU_SOURCE
    _POSIX_C_SOURCE=200809L
)

# Installation
install(TARGETS vnids-cli RUNTIME DESTINATION bin)
