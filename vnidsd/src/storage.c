/**
 * VNIDS Daemon - SQLite Event Storage
 *
 * Persistent storage for security events using SQLite.
 *
 * Copyright (c) 2026 VNIDS Authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sqlite3.h>

#include "vnids_event.h"
#include "vnids_types.h"
#include "vnids_log.h"

#define STORAGE_DEFAULT_MAX_EVENTS 100000
#define STORAGE_CLEANUP_THRESHOLD  1000

/**
 * Storage context.
 */
typedef struct vnids_storage {
    sqlite3* db;
    char db_path[VNIDS_MAX_PATH_LEN];
    pthread_mutex_t mutex;

    /* Prepared statements */
    sqlite3_stmt* stmt_insert;
    sqlite3_stmt* stmt_select_recent;
    sqlite3_stmt* stmt_select_by_id;
    sqlite3_stmt* stmt_count;
    sqlite3_stmt* stmt_delete_old;

    /* Configuration */
    size_t max_events;
    size_t cleanup_batch_size;

    /* Statistics */
    uint64_t events_inserted;
    uint64_t events_deleted;
} vnids_storage_t;

/* Forward declaration */
void vnids_storage_close(vnids_storage_t* storage);

/**
 * SQL schema for events table.
 */
static const char* SCHEMA_SQL =
    "CREATE TABLE IF NOT EXISTS events ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  event_id INTEGER,"
    "  timestamp INTEGER,"
    "  timestamp_usec INTEGER,"
    "  event_type INTEGER,"
    "  severity INTEGER,"
    "  protocol INTEGER,"
    "  src_ip TEXT,"
    "  src_port INTEGER,"
    "  dst_ip TEXT,"
    "  dst_port INTEGER,"
    "  signature_id INTEGER,"
    "  signature_rev INTEGER,"
    "  signature_msg TEXT,"
    "  classification TEXT,"
    "  interface TEXT,"
    "  created_at INTEGER DEFAULT (strftime('%s', 'now'))"
    ");"
    "CREATE INDEX IF NOT EXISTS idx_events_timestamp ON events(timestamp DESC);"
    "CREATE INDEX IF NOT EXISTS idx_events_severity ON events(severity);"
    "CREATE INDEX IF NOT EXISTS idx_events_signature ON events(signature_id);";

static const char* INSERT_SQL =
    "INSERT INTO events ("
    "  event_id, timestamp, timestamp_usec, event_type, severity, protocol,"
    "  src_ip, src_port, dst_ip, dst_port,"
    "  signature_id, signature_rev, signature_msg, classification, interface"
    ") VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";

static const char* SELECT_RECENT_SQL =
    "SELECT event_id, timestamp, timestamp_usec, event_type, severity, protocol,"
    "  src_ip, src_port, dst_ip, dst_port,"
    "  signature_id, signature_rev, signature_msg, classification, interface "
    "FROM events ORDER BY timestamp DESC, id DESC LIMIT ?;";

static const char* SELECT_BY_ID_SQL =
    "SELECT event_id, timestamp, timestamp_usec, event_type, severity, protocol,"
    "  src_ip, src_port, dst_ip, dst_port,"
    "  signature_id, signature_rev, signature_msg, classification, interface "
    "FROM events WHERE id = ?;";

static const char* COUNT_SQL =
    "SELECT COUNT(*) FROM events;";

static const char* DELETE_OLD_SQL =
    "DELETE FROM events WHERE id IN ("
    "  SELECT id FROM events ORDER BY timestamp ASC, id ASC LIMIT ?"
    ");";

/**
 * Create storage instance.
 */
vnids_storage_t* vnids_storage_create(void) {
    vnids_storage_t* storage = calloc(1, sizeof(vnids_storage_t));
    if (!storage) return NULL;

    pthread_mutex_init(&storage->mutex, NULL);
    storage->max_events = STORAGE_DEFAULT_MAX_EVENTS;
    storage->cleanup_batch_size = STORAGE_CLEANUP_THRESHOLD;

    return storage;
}

/**
 * Destroy storage instance.
 */
void vnids_storage_destroy(vnids_storage_t* storage) {
    if (!storage) return;

    vnids_storage_close(storage);
    pthread_mutex_destroy(&storage->mutex);
    free(storage);
}

/**
 * Open the database and initialize schema.
 */
int vnids_storage_open(vnids_storage_t* storage, const char* db_path) {
    if (!storage || !db_path) return -1;

    pthread_mutex_lock(&storage->mutex);

    if (storage->db) {
        LOG_WARN("Storage already open");
        pthread_mutex_unlock(&storage->mutex);
        return -1;
    }

    int rc = sqlite3_open(db_path, &storage->db);
    if (rc != SQLITE_OK) {
        LOG_ERROR("Failed to open database %s: %s", db_path, sqlite3_errmsg(storage->db));
        sqlite3_close(storage->db);
        storage->db = NULL;
        pthread_mutex_unlock(&storage->mutex);
        return -1;
    }

    strncpy(storage->db_path, db_path, VNIDS_MAX_PATH_LEN - 1);

    /* Enable WAL mode for better concurrency */
    char* err_msg = NULL;
    rc = sqlite3_exec(storage->db, "PRAGMA journal_mode=WAL;", NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
        LOG_WARN("Failed to enable WAL mode: %s", err_msg);
        sqlite3_free(err_msg);
    }

    /* Set synchronous mode to NORMAL for balance of safety and speed */
    rc = sqlite3_exec(storage->db, "PRAGMA synchronous=NORMAL;", NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
        LOG_WARN("Failed to set synchronous mode: %s", err_msg);
        sqlite3_free(err_msg);
    }

    /* Create schema */
    rc = sqlite3_exec(storage->db, SCHEMA_SQL, NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
        LOG_ERROR("Failed to create schema: %s", err_msg);
        sqlite3_free(err_msg);
        sqlite3_close(storage->db);
        storage->db = NULL;
        pthread_mutex_unlock(&storage->mutex);
        return -1;
    }

    /* Prepare statements */
    rc = sqlite3_prepare_v2(storage->db, INSERT_SQL, -1, &storage->stmt_insert, NULL);
    if (rc != SQLITE_OK) {
        LOG_ERROR("Failed to prepare insert statement: %s", sqlite3_errmsg(storage->db));
        sqlite3_close(storage->db);
        storage->db = NULL;
        pthread_mutex_unlock(&storage->mutex);
        return -1;
    }

    sqlite3_prepare_v2(storage->db, SELECT_RECENT_SQL, -1, &storage->stmt_select_recent, NULL);
    sqlite3_prepare_v2(storage->db, SELECT_BY_ID_SQL, -1, &storage->stmt_select_by_id, NULL);
    sqlite3_prepare_v2(storage->db, COUNT_SQL, -1, &storage->stmt_count, NULL);
    sqlite3_prepare_v2(storage->db, DELETE_OLD_SQL, -1, &storage->stmt_delete_old, NULL);

    LOG_INFO("Storage opened: %s", db_path);

    pthread_mutex_unlock(&storage->mutex);
    return 0;
}

/**
 * Close the database.
 */
void vnids_storage_close(vnids_storage_t* storage) {
    if (!storage) return;

    pthread_mutex_lock(&storage->mutex);

    if (storage->stmt_insert) {
        sqlite3_finalize(storage->stmt_insert);
        storage->stmt_insert = NULL;
    }
    if (storage->stmt_select_recent) {
        sqlite3_finalize(storage->stmt_select_recent);
        storage->stmt_select_recent = NULL;
    }
    if (storage->stmt_select_by_id) {
        sqlite3_finalize(storage->stmt_select_by_id);
        storage->stmt_select_by_id = NULL;
    }
    if (storage->stmt_count) {
        sqlite3_finalize(storage->stmt_count);
        storage->stmt_count = NULL;
    }
    if (storage->stmt_delete_old) {
        sqlite3_finalize(storage->stmt_delete_old);
        storage->stmt_delete_old = NULL;
    }

    if (storage->db) {
        sqlite3_close(storage->db);
        storage->db = NULL;
        LOG_INFO("Storage closed");
    }

    pthread_mutex_unlock(&storage->mutex);
}

/**
 * Cleanup old events if we exceed max_events.
 */
static void storage_cleanup_if_needed(vnids_storage_t* storage) {
    /* Get current count */
    sqlite3_reset(storage->stmt_count);
    if (sqlite3_step(storage->stmt_count) != SQLITE_ROW) {
        return;
    }

    int64_t count = sqlite3_column_int64(storage->stmt_count, 0);

    if (count <= (int64_t)storage->max_events) {
        return;
    }

    /* Delete oldest events */
    int64_t to_delete = count - (int64_t)storage->max_events + (int64_t)storage->cleanup_batch_size;

    sqlite3_reset(storage->stmt_delete_old);
    sqlite3_bind_int64(storage->stmt_delete_old, 1, to_delete);

    if (sqlite3_step(storage->stmt_delete_old) == SQLITE_DONE) {
        int deleted = sqlite3_changes(storage->db);
        storage->events_deleted += deleted;
        LOG_DEBUG("Cleaned up %d old events", deleted);
    }
}

/**
 * Insert a security event into storage.
 */
int vnids_storage_insert_event(vnids_storage_t* storage,
                                const vnids_security_event_t* event) {
    if (!storage || !event) return -1;

    pthread_mutex_lock(&storage->mutex);

    if (!storage->db || !storage->stmt_insert) {
        pthread_mutex_unlock(&storage->mutex);
        return -1;
    }

    sqlite3_reset(storage->stmt_insert);

    sqlite3_bind_text(storage->stmt_insert, 1, event->id, -1, SQLITE_STATIC);
    sqlite3_bind_int64(storage->stmt_insert, 2, event->timestamp.sec);
    sqlite3_bind_int(storage->stmt_insert, 3, event->timestamp.usec);
    sqlite3_bind_int(storage->stmt_insert, 4, event->event_type);
    sqlite3_bind_int(storage->stmt_insert, 5, event->severity);
    sqlite3_bind_int(storage->stmt_insert, 6, event->protocol);
    sqlite3_bind_text(storage->stmt_insert, 7, event->src_addr, -1, SQLITE_STATIC);
    sqlite3_bind_int(storage->stmt_insert, 8, event->src_port);
    sqlite3_bind_text(storage->stmt_insert, 9, event->dst_addr, -1, SQLITE_STATIC);
    sqlite3_bind_int(storage->stmt_insert, 10, event->dst_port);
    sqlite3_bind_int(storage->stmt_insert, 11, event->rule_sid);
    sqlite3_bind_int(storage->stmt_insert, 12, event->rule_gid);
    sqlite3_bind_text(storage->stmt_insert, 13, event->message, -1, SQLITE_STATIC);

    int rc = sqlite3_step(storage->stmt_insert);
    if (rc != SQLITE_DONE) {
        LOG_ERROR("Failed to insert event: %s", sqlite3_errmsg(storage->db));
        pthread_mutex_unlock(&storage->mutex);
        return -1;
    }

    storage->events_inserted++;

    /* Periodic cleanup */
    if (storage->events_inserted % 1000 == 0) {
        storage_cleanup_if_needed(storage);
    }

    pthread_mutex_unlock(&storage->mutex);
    return 0;
}

/**
 * Query recent events.
 */
int vnids_storage_query_recent(vnids_storage_t* storage,
                                vnids_security_event_t* events,
                                size_t max_count,
                                size_t* count) {
    if (!storage || !events || !count) return -1;

    pthread_mutex_lock(&storage->mutex);

    if (!storage->db || !storage->stmt_select_recent) {
        pthread_mutex_unlock(&storage->mutex);
        return -1;
    }

    sqlite3_reset(storage->stmt_select_recent);
    sqlite3_bind_int64(storage->stmt_select_recent, 1, max_count);

    *count = 0;

    while (*count < max_count) {
        int rc = sqlite3_step(storage->stmt_select_recent);
        if (rc == SQLITE_DONE) {
            break;
        }
        if (rc != SQLITE_ROW) {
            LOG_ERROR("Query error: %s", sqlite3_errmsg(storage->db));
            break;
        }

        vnids_security_event_t* event = &events[*count];
        memset(event, 0, sizeof(vnids_security_event_t));

        const char* id = (const char*)sqlite3_column_text(storage->stmt_select_recent, 0);
        if (id) strncpy(event->id, id, sizeof(event->id) - 1);
        event->timestamp.sec = sqlite3_column_int64(storage->stmt_select_recent, 1);
        event->timestamp.usec = sqlite3_column_int(storage->stmt_select_recent, 2);
        event->event_type = sqlite3_column_int(storage->stmt_select_recent, 3);
        event->severity = sqlite3_column_int(storage->stmt_select_recent, 4);
        event->protocol = sqlite3_column_int(storage->stmt_select_recent, 5);

        const char* src_addr = (const char*)sqlite3_column_text(storage->stmt_select_recent, 6);
        if (src_addr) strncpy(event->src_addr, src_addr, sizeof(event->src_addr) - 1);

        event->src_port = sqlite3_column_int(storage->stmt_select_recent, 7);

        const char* dst_addr = (const char*)sqlite3_column_text(storage->stmt_select_recent, 8);
        if (dst_addr) strncpy(event->dst_addr, dst_addr, sizeof(event->dst_addr) - 1);

        event->dst_port = sqlite3_column_int(storage->stmt_select_recent, 9);
        event->rule_sid = sqlite3_column_int(storage->stmt_select_recent, 10);
        event->rule_gid = sqlite3_column_int(storage->stmt_select_recent, 11);

        const char* msg = (const char*)sqlite3_column_text(storage->stmt_select_recent, 12);
        if (msg) strncpy(event->message, msg, sizeof(event->message) - 1);

        /* Note: classification and interface fields from DB are ignored as they're not in current struct */

        (*count)++;
    }

    pthread_mutex_unlock(&storage->mutex);
    return 0;
}

/**
 * Get event count.
 */
int vnids_storage_get_count(vnids_storage_t* storage, size_t* count) {
    if (!storage || !count) return -1;

    pthread_mutex_lock(&storage->mutex);

    if (!storage->db || !storage->stmt_count) {
        pthread_mutex_unlock(&storage->mutex);
        return -1;
    }

    sqlite3_reset(storage->stmt_count);
    if (sqlite3_step(storage->stmt_count) == SQLITE_ROW) {
        *count = (size_t)sqlite3_column_int64(storage->stmt_count, 0);
    } else {
        *count = 0;
    }

    pthread_mutex_unlock(&storage->mutex);
    return 0;
}

/**
 * Set maximum events before cleanup.
 */
void vnids_storage_set_max_events(vnids_storage_t* storage, size_t max_events) {
    if (!storage) return;

    pthread_mutex_lock(&storage->mutex);
    storage->max_events = max_events;
    pthread_mutex_unlock(&storage->mutex);
}

/**
 * Get storage statistics.
 */
void vnids_storage_get_stats(vnids_storage_t* storage,
                              uint64_t* events_inserted,
                              uint64_t* events_deleted) {
    if (!storage) return;

    pthread_mutex_lock(&storage->mutex);
    if (events_inserted) *events_inserted = storage->events_inserted;
    if (events_deleted) *events_deleted = storage->events_deleted;
    pthread_mutex_unlock(&storage->mutex);
}
