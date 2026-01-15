# VNIDS Daemon (vnidsd)
# CMakeLists.txt

# Core library sources (everything except main.c)
set(VNIDSD_LIB_SOURCES
    src/daemon.c
    src/config.c
    src/config_validate.c
    src/log.c
    src/signal.c
    src/pidfile.c
    src/eventloop.c
    src/types.c
    src/ipc_server.c
    src/ipc_client.c
    src/ipc_message.c
    src/ipc_control.c
    src/eve_parser.c
    src/eve_reader.c
    src/event_queue.c
    src/event_handler.c
    src/storage.c
    src/watchdog.c
    src/api_server.c
)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/shared/include
    ${SQLite3_INCLUDE_DIRS}
    ${CJSON_INCLUDE_DIRS}
)

# Create static library for core functionality (used by tests)
add_library(vnidsd_core STATIC ${VNIDSD_LIB_SOURCES})
target_compile_definitions(vnidsd_core PUBLIC
    _GNU_SOURCE
    _POSIX_C_SOURCE=200809L
)
target_include_directories(vnidsd_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/shared/include
    ${SQLite3_INCLUDE_DIRS}
    ${CJSON_INCLUDE_DIRS}
)
target_link_libraries(vnidsd_core
    Threads::Threads
    ${SQLite3_LIBRARIES}
    ${CJSON_LIBRARIES}
    m
)

# Create executable
add_executable(vnidsd src/main.c)

# Link libraries
target_link_libraries(vnidsd
    vnidsd_core
)

# Installation
install(TARGETS vnidsd RUNTIME DESTINATION bin)
install(FILES vnidsd.conf.example DESTINATION etc/vnids RENAME vnidsd.conf.example)
