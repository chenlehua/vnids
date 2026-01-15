/**
 * VNIDS Configuration Definitions
 *
 * Configuration structures for vnidsd daemon.
 *
 * Copyright (c) 2026 VNIDS Authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef VNIDS_CONFIG_H
#define VNIDS_CONFIG_H

#include "vnids_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Log levels */
typedef enum {
    VNIDS_LOG_TRACE = 0,
    VNIDS_LOG_DEBUG = 1,
    VNIDS_LOG_INFO = 2,
    VNIDS_LOG_WARN = 3,
    VNIDS_LOG_ERROR = 4,
    VNIDS_LOG_FATAL = 5,
} vnids_log_level_t;

/* General configuration */
typedef struct {
    vnids_log_level_t log_level;
    char pid_file[VNIDS_MAX_PATH_LEN];
    bool daemonize;
} vnids_general_config_t;

/* Suricata configuration */
typedef struct {
    char binary[VNIDS_MAX_PATH_LEN];
    char config[VNIDS_MAX_PATH_LEN];
    char rules_dir[VNIDS_MAX_PATH_LEN];
    char interface[64];
} vnids_suricata_config_t;

/* IPC configuration */
typedef struct {
    char socket_dir[VNIDS_MAX_PATH_LEN];
    uint32_t event_buffer_size;
} vnids_ipc_config_t;

/* Storage configuration */
typedef struct {
    char database[VNIDS_MAX_PATH_LEN];
    uint32_t retention_days;
    uint32_t max_size_mb;
} vnids_storage_config_t;

/* Watchdog configuration */
typedef struct {
    uint32_t check_interval_ms;
    uint32_t heartbeat_timeout_s;
    uint32_t max_restart_attempts;
} vnids_watchdog_config_t;

/* Complete configuration */
typedef struct {
    vnids_general_config_t general;
    vnids_suricata_config_t suricata;
    vnids_ipc_config_t ipc;
    vnids_storage_config_t storage;
    vnids_watchdog_config_t watchdog;
} vnids_config_t;

/* Default values */
#define VNIDS_DEFAULT_LOG_LEVEL VNIDS_LOG_INFO
#define VNIDS_DEFAULT_PID_FILE "/var/run/vnidsd.pid"
#define VNIDS_DEFAULT_SURICATA_BINARY "/usr/bin/suricata"
#define VNIDS_DEFAULT_SURICATA_CONFIG "/etc/vnids/suricata.yaml"
#define VNIDS_DEFAULT_RULES_DIR "/etc/vnids/rules"
#define VNIDS_DEFAULT_INTERFACE "eth0"
#define VNIDS_DEFAULT_SOCKET_DIR "/var/run/vnids"
#define VNIDS_DEFAULT_EVENT_BUFFER_SIZE 32768
#define VNIDS_DEFAULT_DATABASE "/var/lib/vnids/events.db"
#define VNIDS_DEFAULT_RETENTION_DAYS 7
#define VNIDS_DEFAULT_MAX_SIZE_MB 500
#define VNIDS_DEFAULT_CHECK_INTERVAL_MS 500
#define VNIDS_DEFAULT_HEARTBEAT_TIMEOUT_S 2
#define VNIDS_DEFAULT_MAX_RESTART_ATTEMPTS 10

/* Configuration management */
vnids_config_t* vnids_config_create(void);
void vnids_config_destroy(vnids_config_t* config);
void vnids_config_set_defaults(vnids_config_t* config);

/* Load configuration from INI file */
vnids_result_t vnids_config_load(vnids_config_t* config, const char* path);

/* Validate configuration */
vnids_result_t vnids_config_validate(const vnids_config_t* config, char* error_msg, size_t error_len);

/* Apply environment variable overrides */
void vnids_config_apply_env(vnids_config_t* config);

/* Convert log level to string */
const char* vnids_log_level_str(vnids_log_level_t level);

/* Parse log level from string */
vnids_log_level_t vnids_log_level_parse(const char* str);

#ifdef __cplusplus
}
#endif

#endif /* VNIDS_CONFIG_H */
