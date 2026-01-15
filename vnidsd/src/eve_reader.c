/**
 * VNIDS Daemon - EVE Reader Thread
 *
 * Reads EVE JSON events from Suricata socket and queues them for processing.
 *
 * Copyright (c) 2026 VNIDS Authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>

#include "vnids_event.h"
#include "vnids_types.h"
#include "vnids_ipc.h"
#include "vnids_config.h"
#include "vnids_log.h"

/* Forward declarations */
typedef struct vnids_eve_client vnids_eve_client_t;
typedef struct vnids_event_queue vnids_event_queue_t;

extern vnids_eve_client_t* vnids_eve_client_create(void);
extern void vnids_eve_client_destroy(vnids_eve_client_t* client);
extern int vnids_eve_client_connect(vnids_eve_client_t* client, const char* socket_path);
extern void vnids_eve_client_disconnect(vnids_eve_client_t* client);
extern bool vnids_eve_client_is_connected(vnids_eve_client_t* client);
extern char* vnids_eve_client_read_line(vnids_eve_client_t* client);
extern int vnids_eve_client_wait(vnids_eve_client_t* client, int timeout_ms);
extern int vnids_eve_client_reconnect(vnids_eve_client_t* client);

extern int vnids_eve_parse(const char* json_line, vnids_security_event_t* event);
extern int vnids_eve_parse_stats(const char* json_line, vnids_stats_t* stats);

extern int vnids_event_queue_push(vnids_event_queue_t* queue,
                                   const vnids_security_event_t* event);

/**
 * EVE reader context.
 */
typedef struct vnids_eve_reader {
    vnids_eve_client_t* client;
    vnids_event_queue_t* event_queue;
    char socket_path[VNIDS_MAX_PATH_LEN];

    pthread_t thread;
    pthread_mutex_t mutex;
    bool running;
    bool thread_started;

    /* Statistics */
    uint64_t events_read;
    uint64_t events_parsed;
    uint64_t events_queued;
    uint64_t parse_errors;
    uint64_t reconnect_count;

    /* Latest stats from Suricata */
    vnids_stats_t latest_stats;
    pthread_mutex_t stats_mutex;

    /* Configuration */
    int reconnect_delay_ms;
    int read_timeout_ms;
} vnids_eve_reader_t;

/* Forward declaration */
void vnids_eve_reader_stop(vnids_eve_reader_t* reader);

/**
 * Create EVE reader.
 */
vnids_eve_reader_t* vnids_eve_reader_create(void) {
    vnids_eve_reader_t* reader = calloc(1, sizeof(vnids_eve_reader_t));
    if (!reader) return NULL;

    reader->client = vnids_eve_client_create();
    if (!reader->client) {
        free(reader);
        return NULL;
    }

    pthread_mutex_init(&reader->mutex, NULL);
    pthread_mutex_init(&reader->stats_mutex, NULL);

    reader->reconnect_delay_ms = 1000;  /* 1 second */
    reader->read_timeout_ms = 100;      /* 100ms */

    return reader;
}

/**
 * Destroy EVE reader.
 */
void vnids_eve_reader_destroy(vnids_eve_reader_t* reader) {
    if (!reader) return;

    vnids_eve_reader_stop(reader);

    if (reader->client) {
        vnids_eve_client_destroy(reader->client);
    }

    pthread_mutex_destroy(&reader->mutex);
    pthread_mutex_destroy(&reader->stats_mutex);

    free(reader);
}

/**
 * Reader thread main function.
 */
static void* eve_reader_thread(void* arg) {
    vnids_eve_reader_t* reader = (vnids_eve_reader_t*)arg;

    LOG_INFO("EVE reader thread started");

    while (reader->running) {
        /* Ensure connected */
        if (!vnids_eve_client_is_connected(reader->client)) {
            LOG_INFO("Attempting to connect to EVE socket: %s", reader->socket_path);

            if (vnids_eve_client_connect(reader->client, reader->socket_path) < 0) {
                /* Wait before retry */
                for (int i = 0; i < reader->reconnect_delay_ms / 100 && reader->running; i++) {
                    usleep(100000);  /* 100ms */
                }
                reader->reconnect_count++;
                continue;
            }

            LOG_INFO("Connected to EVE socket");
        }

        /* Wait for data */
        int ready = vnids_eve_client_wait(reader->client, reader->read_timeout_ms);
        if (ready < 0) {
            /* Error, disconnect and retry */
            vnids_eve_client_disconnect(reader->client);
            continue;
        }

        if (ready == 0) {
            /* Timeout, check if still running */
            continue;
        }

        /* Read and process lines */
        char* line;
        while ((line = vnids_eve_client_read_line(reader->client)) != NULL) {
            reader->events_read++;

            /* Try to parse as stats first */
            vnids_stats_t stats;
            if (vnids_eve_parse_stats(line, &stats) == 0) {
                pthread_mutex_lock(&reader->stats_mutex);
                reader->latest_stats = stats;
                pthread_mutex_unlock(&reader->stats_mutex);
                continue;
            }

            /* Parse as security event */
            vnids_security_event_t event;
            if (vnids_eve_parse(line, &event) == 0) {
                reader->events_parsed++;

                /* Queue the event */
                if (reader->event_queue) {
                    if (vnids_event_queue_push(reader->event_queue, &event) == 0) {
                        reader->events_queued++;
                    }
                }
            } else {
                reader->parse_errors++;
            }

            if (!reader->running) break;
        }

        /* Check connection state */
        if (!vnids_eve_client_is_connected(reader->client)) {
            LOG_WARN("EVE socket disconnected, will reconnect");
        }
    }

    LOG_INFO("EVE reader thread stopping");
    vnids_eve_client_disconnect(reader->client);

    return NULL;
}

/**
 * Initialize and start the EVE reader.
 */
int vnids_eve_reader_start(vnids_eve_reader_t* reader,
                            const char* socket_path,
                            vnids_event_queue_t* event_queue) {
    if (!reader || !socket_path) {
        return -1;
    }

    pthread_mutex_lock(&reader->mutex);

    if (reader->thread_started) {
        pthread_mutex_unlock(&reader->mutex);
        return -1;
    }

    strncpy(reader->socket_path, socket_path, VNIDS_MAX_PATH_LEN - 1);
    reader->event_queue = event_queue;
    reader->running = true;

    int ret = pthread_create(&reader->thread, NULL, eve_reader_thread, reader);
    if (ret != 0) {
        LOG_ERROR("Failed to create EVE reader thread: %s", strerror(ret));
        reader->running = false;
        pthread_mutex_unlock(&reader->mutex);
        return -1;
    }

    reader->thread_started = true;

    pthread_mutex_unlock(&reader->mutex);
    return 0;
}

/**
 * Stop the EVE reader.
 */
void vnids_eve_reader_stop(vnids_eve_reader_t* reader) {
    if (!reader) return;

    pthread_mutex_lock(&reader->mutex);

    if (!reader->thread_started) {
        pthread_mutex_unlock(&reader->mutex);
        return;
    }

    reader->running = false;

    pthread_mutex_unlock(&reader->mutex);

    pthread_join(reader->thread, NULL);

    pthread_mutex_lock(&reader->mutex);
    reader->thread_started = false;
    pthread_mutex_unlock(&reader->mutex);

    LOG_INFO("EVE reader stopped");
}

/**
 * Check if reader is running.
 */
bool vnids_eve_reader_is_running(vnids_eve_reader_t* reader) {
    if (!reader) return false;

    pthread_mutex_lock(&reader->mutex);
    bool running = reader->running && reader->thread_started;
    pthread_mutex_unlock(&reader->mutex);

    return running;
}

/**
 * Get reader statistics.
 */
void vnids_eve_reader_get_stats(vnids_eve_reader_t* reader,
                                 uint64_t* events_read,
                                 uint64_t* events_parsed,
                                 uint64_t* events_queued,
                                 uint64_t* parse_errors) {
    if (!reader) return;

    if (events_read) *events_read = reader->events_read;
    if (events_parsed) *events_parsed = reader->events_parsed;
    if (events_queued) *events_queued = reader->events_queued;
    if (parse_errors) *parse_errors = reader->parse_errors;
}

/**
 * Get latest Suricata stats.
 */
int vnids_eve_reader_get_suricata_stats(vnids_eve_reader_t* reader,
                                         vnids_stats_t* stats) {
    if (!reader || !stats) return -1;

    pthread_mutex_lock(&reader->stats_mutex);
    *stats = reader->latest_stats;
    pthread_mutex_unlock(&reader->stats_mutex);

    return 0;
}
