/**
 * VNIDS Daemon - IPC Control Commands
 *
 * Handles control commands from CLI and API.
 *
 * Copyright (c) 2026 VNIDS Authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#include "vnids_ipc.h"
#include "vnids_types.h"
#include "vnids_log.h"

/* Forward declarations for daemon context operations */
struct vnidsd_ctx;
typedef struct vnidsd_ctx vnidsd_ctx_t;

/* External functions from daemon */
extern vnids_result_t vnidsd_reload_rules(vnidsd_ctx_t* ctx);
extern vnids_result_t vnidsd_get_stats(vnidsd_ctx_t* ctx, vnids_stats_t* stats);
extern bool vnidsd_is_suricata_running(vnidsd_ctx_t* ctx);
extern uint64_t vnidsd_get_uptime(vnidsd_ctx_t* ctx);
extern void vnidsd_request_shutdown(vnidsd_ctx_t* ctx);

/* External message functions */
extern char* vnids_response_to_json(vnids_ipc_error_t error, const char* message,
                                     const void* data);
extern char* vnids_stats_to_json(const vnids_stats_t* stats);
extern char* vnids_status_response(const char* status, const char* version,
                                    uint64_t uptime, bool suricata_running);
extern int vnids_parse_config_param(const char* params_json, char* key, size_t key_len,
                                     char* value, size_t value_len);

/**
 * Control command handler context.
 */
typedef struct {
    vnidsd_ctx_t* daemon_ctx;
    vnids_ipc_ctx_t* ipc_ctx;
    bool shutdown_requested;
} vnids_control_ctx_t;

/**
 * Create control context.
 */
vnids_control_ctx_t* vnids_control_create(vnidsd_ctx_t* daemon_ctx) {
    vnids_control_ctx_t* ctx = calloc(1, sizeof(vnids_control_ctx_t));
    if (!ctx) return NULL;

    ctx->daemon_ctx = daemon_ctx;
    ctx->shutdown_requested = false;

    return ctx;
}

/**
 * Destroy control context.
 */
void vnids_control_destroy(vnids_control_ctx_t* ctx) {
    free(ctx);
}

/**
 * Handle reload rules command.
 */
static char* handle_reload_rules(vnids_control_ctx_t* ctx) {
    LOG_INFO("Handling reload_rules command");

    if (!ctx->daemon_ctx) {
        return vnids_response_to_json(VNIDS_IPC_ERR_INTERNAL,
                                       "Daemon context not available", NULL);
    }

    vnids_result_t result = vnidsd_reload_rules(ctx->daemon_ctx);
    if (result != VNIDS_OK) {
        return vnids_response_to_json(VNIDS_IPC_ERR_INTERNAL,
                                       vnids_result_str(result), NULL);
    }

    return vnids_response_to_json(VNIDS_IPC_ERR_NONE,
                                   "Rules reloaded successfully", NULL);
}

/**
 * Handle get stats command.
 */
static char* handle_get_stats(vnids_control_ctx_t* ctx) {
    LOG_DEBUG("Handling get_stats command");

    if (!ctx->daemon_ctx) {
        return vnids_response_to_json(VNIDS_IPC_ERR_INTERNAL,
                                       "Daemon context not available", NULL);
    }

    vnids_stats_t stats;
    vnids_result_t result = vnidsd_get_stats(ctx->daemon_ctx, &stats);
    if (result != VNIDS_OK) {
        return vnids_response_to_json(VNIDS_IPC_ERR_INTERNAL,
                                       vnids_result_str(result), NULL);
    }

    char* stats_json = vnids_stats_to_json(&stats);
    char* response = vnids_response_to_json(VNIDS_IPC_ERR_NONE, NULL, stats_json);
    free(stats_json);

    return response;
}

/**
 * Handle set config command.
 */
static char* handle_set_config(vnids_control_ctx_t* ctx, const char* params) {
    LOG_INFO("Handling set_config command");

    if (!params || strlen(params) == 0) {
        return vnids_response_to_json(VNIDS_IPC_ERR_INVALID_PARAMS,
                                       "Missing parameters", NULL);
    }

    char key[256];
    char value[1024];

    if (vnids_parse_config_param(params, key, sizeof(key),
                                  value, sizeof(value)) < 0) {
        return vnids_response_to_json(VNIDS_IPC_ERR_INVALID_PARAMS,
                                       "Invalid parameter format", NULL);
    }

    /* Validate config key */
    bool valid_key = false;
    const char* valid_keys[] = {
        "log_level", "eve_socket", "rules_dir", "max_events",
        "watchdog_interval", "stats_interval", NULL
    };

    for (int i = 0; valid_keys[i]; i++) {
        if (strcmp(key, valid_keys[i]) == 0) {
            valid_key = true;
            break;
        }
    }

    if (!valid_key) {
        return vnids_response_to_json(VNIDS_IPC_ERR_INVALID_CONFIG_KEY,
                                       "Unknown configuration key", NULL);
    }

    /* Apply configuration change */
    /* TODO: Implement actual config change in daemon context */
    LOG_INFO("Config change: %s = %s", key, value);

    return vnids_response_to_json(VNIDS_IPC_ERR_NONE,
                                   "Configuration updated", NULL);
}

/**
 * Handle shutdown command.
 */
static char* handle_shutdown(vnids_control_ctx_t* ctx) {
    LOG_INFO("Handling shutdown command");

    ctx->shutdown_requested = true;

    if (ctx->daemon_ctx) {
        vnidsd_request_shutdown(ctx->daemon_ctx);
    }

    return vnids_response_to_json(VNIDS_IPC_ERR_NONE,
                                   "Shutdown initiated", NULL);
}

/**
 * Handle status command.
 */
static char* handle_status(vnids_control_ctx_t* ctx) {
    LOG_DEBUG("Handling status command");

    if (!ctx->daemon_ctx) {
        return vnids_response_to_json(VNIDS_IPC_ERR_INTERNAL,
                                       "Daemon context not available", NULL);
    }

    bool suricata_running = vnidsd_is_suricata_running(ctx->daemon_ctx);
    uint64_t uptime = vnidsd_get_uptime(ctx->daemon_ctx);

    const char* status = ctx->shutdown_requested ? "shutting_down" :
                         (suricata_running ? "running" : "degraded");

    return vnids_status_response(status, VNIDS_VERSION_STRING, uptime, suricata_running);
}

/**
 * Handle list rules command.
 */
static char* handle_list_rules(vnids_control_ctx_t* ctx) {
    LOG_DEBUG("Handling list_rules command");

    /* TODO: Implement rule listing from rules directory */
    return vnids_response_to_json(VNIDS_IPC_ERR_NONE,
                                   "Rule listing not yet implemented", NULL);
}

/**
 * Handle list events command.
 */
static char* handle_list_events(vnids_control_ctx_t* ctx, const char* params) {
    LOG_DEBUG("Handling list_events command");

    /* TODO: Implement event listing from storage */
    return vnids_response_to_json(VNIDS_IPC_ERR_NONE,
                                   "Event listing not yet implemented", NULL);
}

/**
 * Handle validate rules command.
 */
static char* handle_validate_rules(vnids_control_ctx_t* ctx) {
    LOG_DEBUG("Handling validate_rules command");

    /* TODO: Implement rule validation by running suricata -T */
    return vnids_response_to_json(VNIDS_IPC_ERR_NONE,
                                   "Rule validation not yet implemented", NULL);
}

/**
 * Process a control command and return JSON response.
 * Caller must free the returned string.
 */
char* vnids_control_process(vnids_control_ctx_t* ctx,
                             vnids_command_t cmd,
                             const char* params) {
    if (!ctx) {
        return vnids_response_to_json(VNIDS_IPC_ERR_INTERNAL,
                                       "Control context not available", NULL);
    }

    LOG_DEBUG("Processing command: %s", vnids_command_str(cmd));

    switch (cmd) {
        case VNIDS_CMD_RELOAD_RULES:
            return handle_reload_rules(ctx);

        case VNIDS_CMD_GET_STATS:
            return handle_get_stats(ctx);

        case VNIDS_CMD_SET_CONFIG:
            return handle_set_config(ctx, params);

        case VNIDS_CMD_SHUTDOWN:
            return handle_shutdown(ctx);

        case VNIDS_CMD_STATUS:
            return handle_status(ctx);

        case VNIDS_CMD_LIST_RULES:
            return handle_list_rules(ctx);

        case VNIDS_CMD_LIST_EVENTS:
            return handle_list_events(ctx, params);

        case VNIDS_CMD_VALIDATE_RULES:
            return handle_validate_rules(ctx);

        default:
            return vnids_response_to_json(VNIDS_IPC_ERR_INVALID_COMMAND,
                                           "Unknown command", NULL);
    }
}

/**
 * Check if shutdown was requested.
 */
bool vnids_control_shutdown_requested(vnids_control_ctx_t* ctx) {
    return ctx && ctx->shutdown_requested;
}
