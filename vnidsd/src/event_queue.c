/**
 * VNIDS Daemon - Event Queue (Lock-Free MPSC)
 *
 * Lock-free multi-producer single-consumer queue for event processing.
 * Uses C11 atomics for thread safety.
 *
 * Copyright (c) 2026 VNIDS Authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>

#include "vnids_event.h"
#include "vnids_log.h"

#define EVENT_QUEUE_DEFAULT_SIZE 4096

/**
 * Queue node structure.
 */
typedef struct vnids_queue_node {
    vnids_security_event_t event;
    _Atomic(struct vnids_queue_node*) next;
} vnids_queue_node_t;

/**
 * Lock-free MPSC queue.
 */
typedef struct vnids_event_queue {
    _Atomic(vnids_queue_node_t*) head;
    _Atomic(vnids_queue_node_t*) tail;
    vnids_queue_node_t* stub;

    /* Statistics */
    _Atomic uint64_t enqueue_count;
    _Atomic uint64_t dequeue_count;
    _Atomic uint64_t drop_count;

    /* Configuration */
    size_t max_size;
    _Atomic size_t current_size;
} vnids_event_queue_t;

/**
 * Create a new event queue.
 */
vnids_event_queue_t* vnids_event_queue_create(size_t max_size) {
    vnids_event_queue_t* queue = calloc(1, sizeof(vnids_event_queue_t));
    if (!queue) {
        return NULL;
    }

    /* Create stub node */
    queue->stub = calloc(1, sizeof(vnids_queue_node_t));
    if (!queue->stub) {
        free(queue);
        return NULL;
    }

    atomic_store(&queue->stub->next, NULL);
    atomic_store(&queue->head, queue->stub);
    atomic_store(&queue->tail, queue->stub);

    queue->max_size = max_size > 0 ? max_size : EVENT_QUEUE_DEFAULT_SIZE;
    atomic_store(&queue->current_size, 0);
    atomic_store(&queue->enqueue_count, 0);
    atomic_store(&queue->dequeue_count, 0);
    atomic_store(&queue->drop_count, 0);

    return queue;
}

/**
 * Destroy the event queue and free all nodes.
 */
void vnids_event_queue_destroy(vnids_event_queue_t* queue) {
    if (!queue) return;

    /* Drain the queue */
    vnids_security_event_t event;
    while (vnids_event_queue_pop(queue, &event) == 0) {
        /* Discard */
    }

    free(queue->stub);
    free(queue);
}

/**
 * Push an event onto the queue (producer side).
 * Returns 0 on success, -1 if queue is full.
 */
int vnids_event_queue_push(vnids_event_queue_t* queue,
                            const vnids_security_event_t* event) {
    if (!queue || !event) {
        return -1;
    }

    /* Check capacity */
    size_t current = atomic_load(&queue->current_size);
    if (current >= queue->max_size) {
        atomic_fetch_add(&queue->drop_count, 1);
        return -1;
    }

    /* Allocate new node */
    vnids_queue_node_t* node = malloc(sizeof(vnids_queue_node_t));
    if (!node) {
        atomic_fetch_add(&queue->drop_count, 1);
        return -1;
    }

    memcpy(&node->event, event, sizeof(vnids_security_event_t));
    atomic_store(&node->next, NULL);

    /* MPSC enqueue: atomically swap tail and update previous tail's next */
    vnids_queue_node_t* prev = atomic_exchange(&queue->tail, node);
    atomic_store(&prev->next, node);

    atomic_fetch_add(&queue->current_size, 1);
    atomic_fetch_add(&queue->enqueue_count, 1);

    return 0;
}

/**
 * Pop an event from the queue (consumer side).
 * Returns 0 on success, -1 if queue is empty.
 */
int vnids_event_queue_pop(vnids_event_queue_t* queue,
                           vnids_security_event_t* event) {
    if (!queue || !event) {
        return -1;
    }

    vnids_queue_node_t* head = atomic_load(&queue->head);
    vnids_queue_node_t* next = atomic_load(&head->next);

    if (next == NULL) {
        /* Queue is empty */
        return -1;
    }

    /* Copy event data */
    memcpy(event, &next->event, sizeof(vnids_security_event_t));

    /* Advance head */
    atomic_store(&queue->head, next);

    /* Free old head (the stub becomes the new head) */
    free(head);

    atomic_fetch_sub(&queue->current_size, 1);
    atomic_fetch_add(&queue->dequeue_count, 1);

    return 0;
}

/**
 * Check if queue is empty.
 */
bool vnids_event_queue_is_empty(vnids_event_queue_t* queue) {
    if (!queue) return true;

    vnids_queue_node_t* head = atomic_load(&queue->head);
    vnids_queue_node_t* next = atomic_load(&head->next);

    return next == NULL;
}

/**
 * Get current queue size (approximate).
 */
size_t vnids_event_queue_size(vnids_event_queue_t* queue) {
    if (!queue) return 0;
    return atomic_load(&queue->current_size);
}

/**
 * Get queue statistics.
 */
void vnids_event_queue_get_stats(vnids_event_queue_t* queue,
                                  uint64_t* enqueue_count,
                                  uint64_t* dequeue_count,
                                  uint64_t* drop_count) {
    if (!queue) return;

    if (enqueue_count) *enqueue_count = atomic_load(&queue->enqueue_count);
    if (dequeue_count) *dequeue_count = atomic_load(&queue->dequeue_count);
    if (drop_count) *drop_count = atomic_load(&queue->drop_count);
}

/**
 * Clear the queue, discarding all events.
 */
void vnids_event_queue_clear(vnids_event_queue_t* queue) {
    if (!queue) return;

    vnids_security_event_t event;
    while (vnids_event_queue_pop(queue, &event) == 0) {
        /* Discard */
    }
}
