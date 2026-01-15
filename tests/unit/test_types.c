/**
 * VNIDS Unit Tests - Types Tests
 *
 * Copyright (c) 2026 VNIDS Authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vnids_types.h"

/* Test helper macros */
#define TEST_ASSERT(cond) do { \
    if (!(cond)) { \
        printf("  FAIL: %s (line %d)\n", #cond, __LINE__); \
        return 1; \
    } \
} while(0)

/* External functions from types.c */
extern const char* vnids_result_str(vnids_result_t result);
extern const char* vnids_severity_str(vnids_severity_t severity);
extern const char* vnids_protocol_str(vnids_protocol_t proto);
extern const char* vnids_event_type_str(vnids_event_type_t type);

/**
 * Test result string conversion.
 */
int test_types_result_str(void) {
    TEST_ASSERT(strcmp(vnids_result_str(VNIDS_OK), "OK") == 0);
    TEST_ASSERT(strcmp(vnids_result_str(VNIDS_ERROR), "Generic error") == 0);
    TEST_ASSERT(strcmp(vnids_result_str(VNIDS_ERROR_NOMEM), "Out of memory") == 0);
    TEST_ASSERT(strcmp(vnids_result_str(VNIDS_ERROR_INVALID), "Invalid argument") == 0);
    TEST_ASSERT(strcmp(vnids_result_str(VNIDS_ERROR_NOT_FOUND), "Not found") == 0);
    TEST_ASSERT(strcmp(vnids_result_str(VNIDS_ERROR_TIMEOUT), "Timeout") == 0);
    TEST_ASSERT(strcmp(vnids_result_str(VNIDS_ERROR_IO), "I/O error") == 0);
    TEST_ASSERT(strcmp(vnids_result_str(VNIDS_ERROR_PARSE), "Parse error") == 0);
    TEST_ASSERT(strcmp(vnids_result_str(VNIDS_ERROR_CONFIG), "Configuration error") == 0);

    /* Unknown error code */
    TEST_ASSERT(strcmp(vnids_result_str((vnids_result_t)999), "Unknown error") == 0);

    return 0;
}

/**
 * Test severity string conversion.
 */
int test_types_severity_str(void) {
    TEST_ASSERT(strcmp(vnids_severity_str(VNIDS_SEVERITY_CRITICAL), "critical") == 0);
    TEST_ASSERT(strcmp(vnids_severity_str(VNIDS_SEVERITY_HIGH), "high") == 0);
    TEST_ASSERT(strcmp(vnids_severity_str(VNIDS_SEVERITY_MEDIUM), "medium") == 0);
    TEST_ASSERT(strcmp(vnids_severity_str(VNIDS_SEVERITY_LOW), "low") == 0);
    TEST_ASSERT(strcmp(vnids_severity_str(VNIDS_SEVERITY_INFO), "info") == 0);

    /* Unknown severity */
    TEST_ASSERT(strcmp(vnids_severity_str((vnids_severity_t)999), "unknown") == 0);

    return 0;
}

/**
 * Test protocol string conversion.
 */
int test_types_protocol_str(void) {
    TEST_ASSERT(strcmp(vnids_protocol_str(VNIDS_PROTO_TCP), "tcp") == 0);
    TEST_ASSERT(strcmp(vnids_protocol_str(VNIDS_PROTO_UDP), "udp") == 0);
    TEST_ASSERT(strcmp(vnids_protocol_str(VNIDS_PROTO_ICMP), "icmp") == 0);
    TEST_ASSERT(strcmp(vnids_protocol_str(VNIDS_PROTO_SOMEIP), "someip") == 0);
    TEST_ASSERT(strcmp(vnids_protocol_str(VNIDS_PROTO_DOIP), "doip") == 0);
    TEST_ASSERT(strcmp(vnids_protocol_str(VNIDS_PROTO_GBT32960), "gbt32960") == 0);
    TEST_ASSERT(strcmp(vnids_protocol_str(VNIDS_PROTO_HTTP), "http") == 0);
    TEST_ASSERT(strcmp(vnids_protocol_str(VNIDS_PROTO_TLS), "tls") == 0);
    TEST_ASSERT(strcmp(vnids_protocol_str(VNIDS_PROTO_DNS), "dns") == 0);
    TEST_ASSERT(strcmp(vnids_protocol_str(VNIDS_PROTO_MQTT), "mqtt") == 0);

    /* Unknown protocol */
    TEST_ASSERT(strcmp(vnids_protocol_str((vnids_protocol_t)999), "unknown") == 0);

    return 0;
}
