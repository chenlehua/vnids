/**
 * VNIDS Unit Tests - Config Tests
 *
 * Copyright (c) 2026 VNIDS Authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vnids_config.h"

/* Test helper macros */
#define TEST_ASSERT(cond) do { \
    if (!(cond)) { \
        printf("  FAIL: %s (line %d)\n", #cond, __LINE__); \
        return 1; \
    } \
} while(0)

/**
 * Test config parsing (basic structure validation).
 */
int test_config_parse(void) {
    /* Test default values */
    vnids_config_t config;
    memset(&config, 0, sizeof(config));

    /* Check default constants exist and are valid */
    TEST_ASSERT(VNIDS_DEFAULT_EVENT_BUFFER_SIZE > 0);
    TEST_ASSERT(VNIDS_DEFAULT_CHECK_INTERVAL_MS > 0);
    TEST_ASSERT(VNIDS_DEFAULT_HEARTBEAT_TIMEOUT_S > 0);

    /* Verify structure can hold paths */
    TEST_ASSERT(sizeof(config.general.log_level) > 0);
    TEST_ASSERT(sizeof(config.suricata.binary) >= VNIDS_MAX_PATH_LEN);
    TEST_ASSERT(sizeof(config.ipc.socket_dir) >= VNIDS_MAX_PATH_LEN);
    TEST_ASSERT(sizeof(config.storage.database) >= VNIDS_MAX_PATH_LEN);

    return 0;
}
