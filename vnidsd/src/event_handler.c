/**
 * VNIDS Daemon - Event Handler
 *
 * Processes security events from the queue and dispatches to storage/callbacks.
 *
 * Copyright (c) 2026 VNIDS Authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <stdatomic.h>

#include "vnids_event.h"
#include "vnids_types.h"
#include "vnids_log.h"

/* Forward declarations */
typedef struct vnids_event_queue vnids_event_queue_t;
typedef struct vnids_storage vnids_storage_t;

extern int vnids_event_queue_pop(vnids_event_queue_t* queue,
                                  vnids_security_event_t* event);
extern bool vnids_event_queue_is_empty(vnids_event_queue_t* queue);
extern int vnids_storage_insert_event(vnids_storage_t* storage,
                                       const vnids_security_event_t* event);

/**
 * Event callback function type.
 */
typedef void (*vnids_event_callback_t)(const vnids_security_event_t* event,
                                        void* user_data);

/**
 * Registered callback entry.
 */
typedef struct {
    vnids_event_callback_t callback;
    void* user_data;
    vnids_event_type_t event_type_filter;  /* 0 = all types */
    vnids_severity_t min_severity;          /* Minimum severity to trigger */
} vnids_callback_entry_t;

#define MAX_CALLBACKS 16

/**
 * Event handler context.
 */
typedef struct vnids_event_handler {
    vnids_event_queue_t* event_queue;
    vnids_storage_t* storage;

    pthread_t thread;
    pthread_mutex_t mutex;
    bool running;
    bool thread_started;

    /* Callbacks */
    vnids_callback_entry_t callbacks[MAX_CALLBACKS];
    int callback_count;

    /* Statistics */
    _Atomic uint64_t events_processed;
    _Atomic uint64_t events_stored;
    _Atomic uint64_t events_dropped;
    _Atomic uint64_t callbacks_invoked;

    /* Configuration */
    int poll_interval_ms;
    int batch_size;
} vnids_event_handler_t;

/* Forward declaration */
void vnids_event_handler_stop(vnids_event_handler_t* handler);

/**
 * Create event handler.
 */
vnids_event_handler_t* vnids_event_handler_create(void) {
    vnids_event_handler_t* handler = calloc(1, sizeof(vnids_event_handler_t));
    if (!handler) return NULL;

    pthread_mutex_init(&handler->mutex, NULL);

    handler->poll_interval_ms = 10;  /* 10ms polling interval */
    handler->batch_size = 100;        /* Process up to 100 events per iteration */

    return handler;
}

/**
 * Destroy event handler.
 */
void vnids_event_handler_destroy(vnids_event_handler_t* handler) {
    if (!handler) return;

    vnids_event_handler_stop(handler);
    pthread_mutex_destroy(&handler->mutex);
    free(handler);
}

/**
 * Register an event callback.
 */
int vnids_event_handler_add_callback(vnids_event_handler_t* handler,
                                      vnids_event_callback_t callback,
                                      void* user_data,
                                      vnids_event_type_t event_type_filter,
                                      vnids_severity_t min_severity) {
    if (!handler || !callback) return -1;

    pthread_mutex_lock(&handler->mutex);

    if (handler->callback_count >= MAX_CALLBACKS) {
        pthread_mutex_unlock(&handler->mutex);
        LOG_ERROR("Maximum number of event callbacks reached");
        return -1;
    }

    vnids_callback_entry_t* entry = &handler->callbacks[handler->callback_count];
    entry->callback = callback;
    entry->user_data = user_data;
    entry->event_type_filter = event_type_filter;
    entry->min_severity = min_severity;
    handler->callback_count++;

    pthread_mutex_unlock(&handler->mutex);
    return 0;
}

/**
 * Check if event matches callback filter.
 */
static bool event_matches_filter(const vnids_security_event_t* event,
                                  const vnids_callback_entry_t* entry) {
    /* Check event type filter */
    if (entry->event_type_filter != 0 &&
        entry->event_type_filter != event->event_type) {
        return false;
    }

    /* Check severity filter (higher values = lower severity) */
    if (event->severity > entry->min_severity) {
        return false;
    }

    return true;
}

/**
 * Dispatch event to callbacks.
 */
static void dispatch_to_callbacks(vnids_event_handler_t* handler,
                                   const vnids_security_event_t* event) {
    pthread_mutex_lock(&handler->mutex);

    for (int i = 0; i < handler->callback_count; i++) {
        vnids_callback_entry_t* entry = &handler->callbacks[i];

        if (event_matches_filter(event, entry)) {
            entry->callback(event, entry->user_data);
            atomic_fetch_add(&handler->callbacks_invoked, 1);
        }
    }

    pthread_mutex_unlock(&handler->mutex);
}

/**
 * Process a single event.
 */
static void process_event(vnids_event_handler_t* handler,
                          const vnids_security_event_t* event) {
    atomic_fetch_add(&handler->events_processed, 1);

    /* Log the event */
    LOG_INFO("Event: %s [%s] %s:%u -> %s:%u sid=%u \"%s\"",
             vnids_event_type_str(event->event_type),
             vnids_severity_str(event->severity),
             event->src_addr, event->src_port,
             event->dst_addr, event->dst_port,
             event->rule_sid,
             event->message);

    /* Store in database */
    if (handler->storage) {
        if (vnids_storage_insert_event(handler->storage, event) == 0) {
            atomic_fetch_add(&handler->events_stored, 1);
        } else {
            atomic_fetch_add(&handler->events_dropped, 1);
        }
    }

    /* Dispatch to callbacks */
    dispatch_to_callbacks(handler, event);
}

/**
 * Event handler thread main function.
 */
static void* event_handler_thread(void* arg) {
    vnids_event_handler_t* handler = (vnids_event_handler_t*)arg;

    LOG_INFO("Event handler thread started");

    while (handler->running) {
        bool processed_any = false;

        /* Process batch of events */
        for (int i = 0; i < handler->batch_size && handler->running; i++) {
            vnids_security_event_t event;

            if (vnids_event_queue_pop(handler->event_queue, &event) == 0) {
                process_event(handler, &event);
                processed_any = true;
            } else {
                break;  /* Queue empty */
            }
        }

        /* Sleep if no events were processed */
        if (!processed_any) {
            struct timespec ts;
            ts.tv_sec = 0;
            ts.tv_nsec = handler->poll_interval_ms * 1000000L;
            nanosleep(&ts, NULL);
        }
    }

    /* Drain remaining events */
    LOG_INFO("Event handler draining queue...");
    vnids_security_event_t event;
    while (vnids_event_queue_pop(handler->event_queue, &event) == 0) {
        process_event(handler, &event);
    }

    LOG_INFO("Event handler thread stopped");
    return NULL;
}

/**
 * Start the event handler.
 */
int vnids_event_handler_start(vnids_event_handler_t* handler,
                               vnids_event_queue_t* event_queue,
                               vnids_storage_t* storage) {
    if (!handler || !event_queue) {
        return -1;
    }

    pthread_mutex_lock(&handler->mutex);

    if (handler->thread_started) {
        pthread_mutex_unlock(&handler->mutex);
        return -1;
    }

    handler->event_queue = event_queue;
    handler->storage = storage;
    handler->running = true;

    int ret = pthread_create(&handler->thread, NULL, event_handler_thread, handler);
    if (ret != 0) {
        LOG_ERROR("Failed to create event handler thread: %s", strerror(ret));
        handler->running = false;
        pthread_mutex_unlock(&handler->mutex);
        return -1;
    }

    handler->thread_started = true;

    pthread_mutex_unlock(&handler->mutex);
    return 0;
}

/**
 * Stop the event handler.
 */
void vnids_event_handler_stop(vnids_event_handler_t* handler) {
    if (!handler) return;

    pthread_mutex_lock(&handler->mutex);

    if (!handler->thread_started) {
        pthread_mutex_unlock(&handler->mutex);
        return;
    }

    handler->running = false;

    pthread_mutex_unlock(&handler->mutex);

    pthread_join(handler->thread, NULL);

    pthread_mutex_lock(&handler->mutex);
    handler->thread_started = false;
    pthread_mutex_unlock(&handler->mutex);

    LOG_INFO("Event handler stopped");
}

/**
 * Check if handler is running.
 */
bool vnids_event_handler_is_running(vnids_event_handler_t* handler) {
    if (!handler) return false;

    pthread_mutex_lock(&handler->mutex);
    bool running = handler->running && handler->thread_started;
    pthread_mutex_unlock(&handler->mutex);

    return running;
}

/**
 * Get handler statistics.
 */
void vnids_event_handler_get_stats(vnids_event_handler_t* handler,
                                    uint64_t* events_processed,
                                    uint64_t* events_stored,
                                    uint64_t* events_dropped,
                                    uint64_t* callbacks_invoked) {
    if (!handler) return;

    if (events_processed) *events_processed = atomic_load(&handler->events_processed);
    if (events_stored) *events_stored = atomic_load(&handler->events_stored);
    if (events_dropped) *events_dropped = atomic_load(&handler->events_dropped);
    if (callbacks_invoked) *callbacks_invoked = atomic_load(&handler->callbacks_invoked);
}
