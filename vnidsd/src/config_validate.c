/**
 * VNIDS Daemon - Configuration Validation
 *
 * Copyright (c) 2026 VNIDS Authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "vnids_config.h"

static int file_exists(const char* path) {
    struct stat st;
    return stat(path, &st) == 0;
}

static int is_executable(const char* path) {
    return access(path, X_OK) == 0;
}

static int is_directory(const char* path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

vnids_result_t vnids_config_validate(const vnids_config_t* config,
                                      char* error_msg, size_t error_len) {
    if (!config) {
        snprintf(error_msg, error_len, "Configuration is NULL");
        return VNIDS_ERROR_INVALID;
    }

    /* Validate Suricata binary */
    if (strlen(config->suricata.binary) == 0) {
        snprintf(error_msg, error_len, "Suricata binary path not configured");
        return VNIDS_ERROR_CONFIG;
    }

    if (!file_exists(config->suricata.binary)) {
        snprintf(error_msg, error_len, "Suricata binary not found: %s",
                 config->suricata.binary);
        return VNIDS_ERROR_CONFIG;
    }

    if (!is_executable(config->suricata.binary)) {
        snprintf(error_msg, error_len, "Suricata binary is not executable: %s",
                 config->suricata.binary);
        return VNIDS_ERROR_CONFIG;
    }

    /* Validate Suricata config */
    if (strlen(config->suricata.config) == 0) {
        snprintf(error_msg, error_len, "Suricata config path not configured");
        return VNIDS_ERROR_CONFIG;
    }

    if (!file_exists(config->suricata.config)) {
        snprintf(error_msg, error_len, "Suricata config not found: %s",
                 config->suricata.config);
        return VNIDS_ERROR_CONFIG;
    }

    /* Validate rules directory */
    if (strlen(config->suricata.rules_dir) == 0) {
        snprintf(error_msg, error_len, "Rules directory not configured");
        return VNIDS_ERROR_CONFIG;
    }

    if (!is_directory(config->suricata.rules_dir)) {
        snprintf(error_msg, error_len, "Rules directory not found: %s",
                 config->suricata.rules_dir);
        return VNIDS_ERROR_CONFIG;
    }

    /* Validate interface */
    if (strlen(config->suricata.interface) == 0) {
        snprintf(error_msg, error_len, "Network interface not configured");
        return VNIDS_ERROR_CONFIG;
    }

    /* Validate event buffer size */
    if (config->ipc.event_buffer_size < 1024 ||
        config->ipc.event_buffer_size > 1048576) {
        snprintf(error_msg, error_len,
                 "Event buffer size must be between 1024 and 1048576");
        return VNIDS_ERROR_CONFIG;
    }

    /* Validate retention days */
    if (config->storage.retention_days < 1 ||
        config->storage.retention_days > 365) {
        snprintf(error_msg, error_len,
                 "Retention days must be between 1 and 365");
        return VNIDS_ERROR_CONFIG;
    }

    /* Validate watchdog intervals */
    if (config->watchdog.check_interval_ms < 100 ||
        config->watchdog.check_interval_ms > 10000) {
        snprintf(error_msg, error_len,
                 "Watchdog check interval must be between 100ms and 10000ms");
        return VNIDS_ERROR_CONFIG;
    }

    if (config->watchdog.heartbeat_timeout_s < 1 ||
        config->watchdog.heartbeat_timeout_s > 60) {
        snprintf(error_msg, error_len,
                 "Heartbeat timeout must be between 1 and 60 seconds");
        return VNIDS_ERROR_CONFIG;
    }

    return VNIDS_OK;
}
