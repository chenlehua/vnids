/**
 * VNIDS Daemon - Configuration Parser
 *
 * INI format configuration file parser.
 *
 * Copyright (c) 2026 VNIDS Authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include "vnids_config.h"
#include "vnids_log.h"

vnids_config_t* vnids_config_create(void) {
    vnids_config_t* config = calloc(1, sizeof(vnids_config_t));
    if (config) {
        vnids_config_set_defaults(config);
    }
    return config;
}

void vnids_config_destroy(vnids_config_t* config) {
    free(config);
}

void vnids_config_set_defaults(vnids_config_t* config) {
    if (!config) return;

    /* General */
    config->general.log_level = VNIDS_DEFAULT_LOG_LEVEL;
    strncpy(config->general.pid_file, VNIDS_DEFAULT_PID_FILE,
            VNIDS_MAX_PATH_LEN - 1);
    config->general.daemonize = true;

    /* Suricata */
    strncpy(config->suricata.binary, VNIDS_DEFAULT_SURICATA_BINARY,
            VNIDS_MAX_PATH_LEN - 1);
    strncpy(config->suricata.config, VNIDS_DEFAULT_SURICATA_CONFIG,
            VNIDS_MAX_PATH_LEN - 1);
    strncpy(config->suricata.rules_dir, VNIDS_DEFAULT_RULES_DIR,
            VNIDS_MAX_PATH_LEN - 1);
    strncpy(config->suricata.interface, VNIDS_DEFAULT_INTERFACE, 63);

    /* IPC */
    strncpy(config->ipc.socket_dir, VNIDS_DEFAULT_SOCKET_DIR,
            VNIDS_MAX_PATH_LEN - 1);
    config->ipc.event_buffer_size = VNIDS_DEFAULT_EVENT_BUFFER_SIZE;

    /* Storage */
    strncpy(config->storage.database, VNIDS_DEFAULT_DATABASE,
            VNIDS_MAX_PATH_LEN - 1);
    config->storage.retention_days = VNIDS_DEFAULT_RETENTION_DAYS;
    config->storage.max_size_mb = VNIDS_DEFAULT_MAX_SIZE_MB;

    /* Watchdog */
    config->watchdog.check_interval_ms = VNIDS_DEFAULT_CHECK_INTERVAL_MS;
    config->watchdog.heartbeat_timeout_s = VNIDS_DEFAULT_HEARTBEAT_TIMEOUT_S;
    config->watchdog.max_restart_attempts = VNIDS_DEFAULT_MAX_RESTART_ATTEMPTS;
}

static char* trim(char* str) {
    char* end;

    /* Trim leading spaces */
    while (isspace((unsigned char)*str)) str++;

    if (*str == 0) return str;

    /* Trim trailing spaces */
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;

    end[1] = '\0';
    return str;
}

static int parse_bool(const char* value) {
    if (strcasecmp(value, "true") == 0 ||
        strcasecmp(value, "yes") == 0 ||
        strcasecmp(value, "on") == 0 ||
        strcmp(value, "1") == 0) {
        return 1;
    }
    return 0;
}

vnids_result_t vnids_config_load(vnids_config_t* config, const char* path) {
    if (!config || !path) {
        return VNIDS_ERROR_INVALID;
    }

    FILE* fp = fopen(path, "r");
    if (!fp) {
        LOG_ERROR("Failed to open config file %s: %s", path, strerror(errno));
        return VNIDS_ERROR_IO;
    }

    char line[1024];
    char section[64] = "";
    int line_num = 0;

    while (fgets(line, sizeof(line), fp)) {
        line_num++;
        char* trimmed = trim(line);

        /* Skip empty lines and comments */
        if (*trimmed == '\0' || *trimmed == '#' || *trimmed == ';') {
            continue;
        }

        /* Section header */
        if (*trimmed == '[') {
            char* end = strchr(trimmed, ']');
            if (!end) {
                LOG_WARN("Config line %d: Invalid section header", line_num);
                continue;
            }
            *end = '\0';
            strncpy(section, trimmed + 1, sizeof(section) - 1);
            continue;
        }

        /* Key-value pair */
        char* eq = strchr(trimmed, '=');
        if (!eq) {
            LOG_WARN("Config line %d: Invalid key-value pair", line_num);
            continue;
        }

        *eq = '\0';
        char* key = trim(trimmed);
        char* value = trim(eq + 1);

        /* Apply to configuration */
        if (strcmp(section, "general") == 0) {
            if (strcmp(key, "log_level") == 0) {
                config->general.log_level = vnids_log_level_parse(value);
            } else if (strcmp(key, "pid_file") == 0) {
                strncpy(config->general.pid_file, value, VNIDS_MAX_PATH_LEN - 1);
            } else if (strcmp(key, "daemonize") == 0) {
                config->general.daemonize = parse_bool(value);
            }
        } else if (strcmp(section, "suricata") == 0) {
            if (strcmp(key, "binary") == 0) {
                strncpy(config->suricata.binary, value, VNIDS_MAX_PATH_LEN - 1);
            } else if (strcmp(key, "config") == 0) {
                strncpy(config->suricata.config, value, VNIDS_MAX_PATH_LEN - 1);
            } else if (strcmp(key, "rules_dir") == 0) {
                strncpy(config->suricata.rules_dir, value, VNIDS_MAX_PATH_LEN - 1);
            } else if (strcmp(key, "interface") == 0) {
                strncpy(config->suricata.interface, value, 63);
            }
        } else if (strcmp(section, "ipc") == 0) {
            if (strcmp(key, "socket_dir") == 0) {
                strncpy(config->ipc.socket_dir, value, VNIDS_MAX_PATH_LEN - 1);
            } else if (strcmp(key, "event_buffer_size") == 0) {
                config->ipc.event_buffer_size = (uint32_t)atoi(value);
            }
        } else if (strcmp(section, "storage") == 0) {
            if (strcmp(key, "database") == 0) {
                strncpy(config->storage.database, value, VNIDS_MAX_PATH_LEN - 1);
            } else if (strcmp(key, "retention_days") == 0) {
                config->storage.retention_days = (uint32_t)atoi(value);
            } else if (strcmp(key, "max_size_mb") == 0) {
                config->storage.max_size_mb = (uint32_t)atoi(value);
            }
        } else if (strcmp(section, "watchdog") == 0) {
            if (strcmp(key, "check_interval_ms") == 0) {
                config->watchdog.check_interval_ms = (uint32_t)atoi(value);
            } else if (strcmp(key, "heartbeat_timeout_s") == 0) {
                config->watchdog.heartbeat_timeout_s = (uint32_t)atoi(value);
            } else if (strcmp(key, "max_restart_attempts") == 0) {
                config->watchdog.max_restart_attempts = (uint32_t)atoi(value);
            }
        }
    }

    fclose(fp);
    LOG_DEBUG("Configuration loaded from %s", path);
    return VNIDS_OK;
}

void vnids_config_apply_env(vnids_config_t* config) {
    if (!config) return;

    const char* env;

    if ((env = getenv("VNIDS_LOG_LEVEL")) != NULL) {
        config->general.log_level = vnids_log_level_parse(env);
    }

    if ((env = getenv("VNIDS_SURICATA_BINARY")) != NULL) {
        strncpy(config->suricata.binary, env, VNIDS_MAX_PATH_LEN - 1);
    }

    if ((env = getenv("VNIDS_SURICATA_CONFIG")) != NULL) {
        strncpy(config->suricata.config, env, VNIDS_MAX_PATH_LEN - 1);
    }

    if ((env = getenv("VNIDS_INTERFACE")) != NULL) {
        strncpy(config->suricata.interface, env, 63);
    }

    if ((env = getenv("VNIDS_SOCKET_DIR")) != NULL) {
        strncpy(config->ipc.socket_dir, env, VNIDS_MAX_PATH_LEN - 1);
    }

    if ((env = getenv("VNIDS_DATABASE")) != NULL) {
        strncpy(config->storage.database, env, VNIDS_MAX_PATH_LEN - 1);
    }
}

const char* vnids_log_level_str(vnids_log_level_t level) {
    switch (level) {
        case VNIDS_LOG_TRACE: return "trace";
        case VNIDS_LOG_DEBUG: return "debug";
        case VNIDS_LOG_INFO:  return "info";
        case VNIDS_LOG_WARN:  return "warn";
        case VNIDS_LOG_ERROR: return "error";
        case VNIDS_LOG_FATAL: return "fatal";
        default: return "unknown";
    }
}

vnids_log_level_t vnids_log_level_parse(const char* str) {
    if (!str) return VNIDS_LOG_INFO;

    if (strcasecmp(str, "trace") == 0) return VNIDS_LOG_TRACE;
    if (strcasecmp(str, "debug") == 0) return VNIDS_LOG_DEBUG;
    if (strcasecmp(str, "info") == 0)  return VNIDS_LOG_INFO;
    if (strcasecmp(str, "warn") == 0 || strcasecmp(str, "warning") == 0)
        return VNIDS_LOG_WARN;
    if (strcasecmp(str, "error") == 0) return VNIDS_LOG_ERROR;
    if (strcasecmp(str, "fatal") == 0) return VNIDS_LOG_FATAL;

    return VNIDS_LOG_INFO;
}
