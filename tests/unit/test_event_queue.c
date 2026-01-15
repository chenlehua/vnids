/**
 * VNIDS Unit Tests - Event Queue Tests
 *
 * Copyright (c) 2026 VNIDS Authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vnids_event.h"

/* Test helper macros */
#define TEST_ASSERT(cond) do { \
    if (!(cond)) { \
        printf("  FAIL: %s (line %d)\n", #cond, __LINE__); \
        return 1; \
    } \
} while(0)

/* External functions from event_queue.c */
typedef struct vnids_event_queue vnids_event_queue_t;
extern vnids_event_queue_t* vnids_event_queue_create(size_t max_size);
extern void vnids_event_queue_destroy(vnids_event_queue_t* queue);
extern int vnids_event_queue_push(vnids_event_queue_t* queue,
                                   const vnids_security_event_t* event);
extern int vnids_event_queue_pop(vnids_event_queue_t* queue,
                                  vnids_security_event_t* event);
extern bool vnids_event_queue_is_empty(vnids_event_queue_t* queue);
extern size_t vnids_event_queue_size(vnids_event_queue_t* queue);

/**
 * Test queue creation.
 */
int test_event_queue_create(void) {
    vnids_event_queue_t* queue = vnids_event_queue_create(100);
    TEST_ASSERT(queue != NULL);
    TEST_ASSERT(vnids_event_queue_is_empty(queue) == true);
    TEST_ASSERT(vnids_event_queue_size(queue) == 0);

    vnids_event_queue_destroy(queue);
    return 0;
}

/**
 * Test push and pop operations.
 */
int test_event_queue_push_pop(void) {
    vnids_event_queue_t* queue = vnids_event_queue_create(100);
    TEST_ASSERT(queue != NULL);

    /* Create test event */
    vnids_security_event_t event_in;
    memset(&event_in, 0, sizeof(event_in));
    strncpy(event_in.id, "test-event-12345", sizeof(event_in.id) - 1);
    event_in.rule_sid = 1000001;
    event_in.severity = VNIDS_SEVERITY_HIGH;
    strncpy(event_in.src_addr, "192.168.1.100", sizeof(event_in.src_addr) - 1);
    strncpy(event_in.dst_addr, "192.168.1.1", sizeof(event_in.dst_addr) - 1);
    event_in.src_port = 12345;
    event_in.dst_port = 80;
    strncpy(event_in.message, "Test Alert", sizeof(event_in.message) - 1);

    /* Push event */
    int result = vnids_event_queue_push(queue, &event_in);
    TEST_ASSERT(result == 0);
    TEST_ASSERT(vnids_event_queue_is_empty(queue) == false);
    TEST_ASSERT(vnids_event_queue_size(queue) == 1);

    /* Pop event */
    vnids_security_event_t event_out;
    result = vnids_event_queue_pop(queue, &event_out);
    TEST_ASSERT(result == 0);
    TEST_ASSERT(vnids_event_queue_is_empty(queue) == true);

    /* Verify data */
    TEST_ASSERT(strcmp(event_out.id, "test-event-12345") == 0);
    TEST_ASSERT(event_out.rule_sid == 1000001);
    TEST_ASSERT(event_out.severity == VNIDS_SEVERITY_HIGH);
    TEST_ASSERT(strcmp(event_out.src_addr, "192.168.1.100") == 0);
    TEST_ASSERT(strcmp(event_out.dst_addr, "192.168.1.1") == 0);
    TEST_ASSERT(event_out.src_port == 12345);
    TEST_ASSERT(event_out.dst_port == 80);
    TEST_ASSERT(strcmp(event_out.message, "Test Alert") == 0);

    /* Pop from empty queue should fail */
    result = vnids_event_queue_pop(queue, &event_out);
    TEST_ASSERT(result == -1);

    vnids_event_queue_destroy(queue);
    return 0;
}

/**
 * Test queue capacity limit.
 */
int test_event_queue_full(void) {
    const size_t max_size = 10;
    vnids_event_queue_t* queue = vnids_event_queue_create(max_size);
    TEST_ASSERT(queue != NULL);

    vnids_security_event_t event;
    memset(&event, 0, sizeof(event));

    /* Fill the queue */
    for (size_t i = 0; i < max_size; i++) {
        snprintf(event.id, sizeof(event.id), "event-%zu", i);
        int result = vnids_event_queue_push(queue, &event);
        TEST_ASSERT(result == 0);
    }

    TEST_ASSERT(vnids_event_queue_size(queue) == max_size);

    /* Try to push to full queue */
    strncpy(event.id, "event-999", sizeof(event.id) - 1);
    int result = vnids_event_queue_push(queue, &event);
    TEST_ASSERT(result == -1);  /* Should fail */

    /* Pop one and try again */
    vnids_security_event_t popped;
    result = vnids_event_queue_pop(queue, &popped);
    TEST_ASSERT(result == 0);
    TEST_ASSERT(strcmp(popped.id, "event-0") == 0);

    result = vnids_event_queue_push(queue, &event);
    TEST_ASSERT(result == 0);

    vnids_event_queue_destroy(queue);
    return 0;
}
