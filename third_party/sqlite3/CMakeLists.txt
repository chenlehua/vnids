# Bundled SQLite3 for cross-compilation
# SQLite3 is public domain

add_library(sqlite3 STATIC sqlite3.c)

target_include_directories(sqlite3 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Recommended compile options for embedded use
target_compile_definitions(sqlite3 PRIVATE
    SQLITE_THREADSAFE=1
    SQLITE_ENABLE_FTS5
    SQLITE_OMIT_LOAD_EXTENSION
    SQLITE_DQS=0
)

# Suppress warnings in third-party code
target_compile_options(sqlite3 PRIVATE -w)
