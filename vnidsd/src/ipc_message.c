/**
 * VNIDS Daemon - IPC Message Serialization
 *
 * JSON message encoding/decoding for IPC communication.
 *
 * Copyright (c) 2026 VNIDS Authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cjson/cJSON.h>

#include "vnids_ipc.h"
#include "vnids_event.h"
#include "vnids_log.h"

/**
 * Serialize statistics to JSON.
 */
char* vnids_stats_to_json(const vnids_stats_t* stats) {
    if (!stats) return NULL;

    cJSON* root = cJSON_CreateObject();
    if (!root) return NULL;

    cJSON_AddNumberToObject(root, "uptime_seconds", (double)stats->uptime_seconds);
    cJSON_AddNumberToObject(root, "packets_captured", (double)stats->packets_captured);
    cJSON_AddNumberToObject(root, "bytes_captured", (double)stats->bytes_captured);
    cJSON_AddNumberToObject(root, "packets_dropped", (double)stats->packets_dropped);
    cJSON_AddNumberToObject(root, "capture_errors", (double)stats->capture_errors);
    cJSON_AddNumberToObject(root, "alerts_total", (double)stats->alerts_total);
    cJSON_AddNumberToObject(root, "rules_loaded", stats->rules_loaded);
    cJSON_AddNumberToObject(root, "rules_failed", stats->rules_failed);
    cJSON_AddNumberToObject(root, "flows_active", stats->flows_active);
    cJSON_AddNumberToObject(root, "flows_total", (double)stats->flows_total);
    cJSON_AddNumberToObject(root, "memory_used_mb", stats->memory_used_mb);
    cJSON_AddNumberToObject(root, "memory_limit_mb", stats->memory_limit_mb);
    cJSON_AddNumberToObject(root, "avg_latency_us", stats->avg_latency_us);
    cJSON_AddNumberToObject(root, "p99_latency_us", stats->p99_latency_us);
    cJSON_AddNumberToObject(root, "pps", stats->pps);

    char* json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    return json;
}

/**
 * Parse statistics from JSON.
 */
int vnids_stats_from_json(const char* json, vnids_stats_t* stats) {
    if (!json || !stats) return -1;

    cJSON* root = cJSON_Parse(json);
    if (!root) {
        LOG_ERROR("Failed to parse stats JSON");
        return -1;
    }

    memset(stats, 0, sizeof(vnids_stats_t));

    cJSON* item;
    if ((item = cJSON_GetObjectItem(root, "uptime_seconds"))) {
        stats->uptime_seconds = (uint64_t)cJSON_GetNumberValue(item);
    }
    if ((item = cJSON_GetObjectItem(root, "packets_captured"))) {
        stats->packets_captured = (uint64_t)cJSON_GetNumberValue(item);
    }
    if ((item = cJSON_GetObjectItem(root, "bytes_captured"))) {
        stats->bytes_captured = (uint64_t)cJSON_GetNumberValue(item);
    }
    if ((item = cJSON_GetObjectItem(root, "packets_dropped"))) {
        stats->packets_dropped = (uint64_t)cJSON_GetNumberValue(item);
    }
    if ((item = cJSON_GetObjectItem(root, "capture_errors"))) {
        stats->capture_errors = (uint64_t)cJSON_GetNumberValue(item);
    }
    if ((item = cJSON_GetObjectItem(root, "alerts_total"))) {
        stats->alerts_total = (uint64_t)cJSON_GetNumberValue(item);
    }
    if ((item = cJSON_GetObjectItem(root, "rules_loaded"))) {
        stats->rules_loaded = (uint32_t)cJSON_GetNumberValue(item);
    }
    if ((item = cJSON_GetObjectItem(root, "flows_active"))) {
        stats->flows_active = (uint32_t)cJSON_GetNumberValue(item);
    }
    if ((item = cJSON_GetObjectItem(root, "flows_total"))) {
        stats->flows_total = (uint64_t)cJSON_GetNumberValue(item);
    }
    if ((item = cJSON_GetObjectItem(root, "memory_used_mb"))) {
        stats->memory_used_mb = (uint32_t)cJSON_GetNumberValue(item);
    }

    cJSON_Delete(root);
    return 0;
}

/**
 * Serialize security event to JSON (internal helper returning allocated string).
 */
static char* vnids_event_to_json_alloc(const vnids_security_event_t* event) {
    if (!event) return NULL;

    cJSON* root = cJSON_CreateObject();
    if (!root) return NULL;

    /* Basic fields */
    cJSON_AddStringToObject(root, "id", event->id);
    cJSON_AddNumberToObject(root, "timestamp", (double)event->timestamp.sec);
    cJSON_AddNumberToObject(root, "timestamp_usec", (double)event->timestamp.usec);
    cJSON_AddStringToObject(root, "event_type", vnids_event_type_str(event->event_type));
    cJSON_AddStringToObject(root, "severity", vnids_severity_str(event->severity));
    cJSON_AddStringToObject(root, "protocol", vnids_protocol_str(event->protocol));

    /* Network tuple */
    cJSON_AddStringToObject(root, "src_addr", event->src_addr);
    cJSON_AddNumberToObject(root, "src_port", event->src_port);
    cJSON_AddStringToObject(root, "dst_addr", event->dst_addr);
    cJSON_AddNumberToObject(root, "dst_port", event->dst_port);

    /* Rule info */
    cJSON_AddNumberToObject(root, "rule_sid", (double)event->rule_sid);
    cJSON_AddNumberToObject(root, "rule_gid", event->rule_gid);
    cJSON_AddStringToObject(root, "message", event->message);

    char* json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    return json;
}

/**
 * Serialize IPC response to JSON.
 */
char* vnids_response_to_json(vnids_ipc_error_t error, const char* message,
                              const void* data) {
    cJSON* root = cJSON_CreateObject();
    if (!root) return NULL;

    cJSON_AddBoolToObject(root, "success", error == VNIDS_IPC_ERR_NONE);
    cJSON_AddNumberToObject(root, "error_code", error);
    cJSON_AddStringToObject(root, "error", vnids_ipc_error_str(error));

    if (message) {
        cJSON_AddStringToObject(root, "message", message);
    }

    /* If data is provided as JSON string, parse and add it */
    if (data) {
        cJSON* data_json = cJSON_Parse((const char*)data);
        if (data_json) {
            cJSON_AddItemToObject(root, "data", data_json);
        } else {
            cJSON_AddStringToObject(root, "data", (const char*)data);
        }
    }

    char* json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    return json;
}

/**
 * Parse IPC request from JSON.
 */
int vnids_request_from_json(const char* json, vnids_command_t* cmd,
                             char* params, size_t params_len) {
    if (!json || !cmd) return -1;

    cJSON* root = cJSON_Parse(json);
    if (!root) {
        LOG_ERROR("Failed to parse request JSON");
        return -1;
    }

    *cmd = VNIDS_CMD_STATUS;  /* Default */

    cJSON* cmd_item = cJSON_GetObjectItem(root, "command");
    if (cmd_item && cJSON_IsString(cmd_item)) {
        const char* cmd_str = cJSON_GetStringValue(cmd_item);
        if (strcmp(cmd_str, "reload_rules") == 0) {
            *cmd = VNIDS_CMD_RELOAD_RULES;
        } else if (strcmp(cmd_str, "get_stats") == 0) {
            *cmd = VNIDS_CMD_GET_STATS;
        } else if (strcmp(cmd_str, "set_config") == 0) {
            *cmd = VNIDS_CMD_SET_CONFIG;
        } else if (strcmp(cmd_str, "shutdown") == 0) {
            *cmd = VNIDS_CMD_SHUTDOWN;
        } else if (strcmp(cmd_str, "status") == 0) {
            *cmd = VNIDS_CMD_STATUS;
        } else if (strcmp(cmd_str, "list_rules") == 0) {
            *cmd = VNIDS_CMD_LIST_RULES;
        } else if (strcmp(cmd_str, "list_events") == 0) {
            *cmd = VNIDS_CMD_LIST_EVENTS;
        } else if (strcmp(cmd_str, "validate_rules") == 0) {
            *cmd = VNIDS_CMD_VALIDATE_RULES;
        }
    }

    if (params && params_len > 0) {
        params[0] = '\0';
        cJSON* params_item = cJSON_GetObjectItem(root, "params");
        if (params_item) {
            char* params_str = cJSON_PrintUnformatted(params_item);
            if (params_str) {
                strncpy(params, params_str, params_len - 1);
                params[params_len - 1] = '\0';
                free(params_str);
            }
        }
    }

    cJSON_Delete(root);
    return 0;
}

/**
 * Create a simple status response.
 */
char* vnids_status_response(const char* status, const char* version,
                             uint64_t uptime, bool suricata_running) {
    cJSON* root = cJSON_CreateObject();
    if (!root) return NULL;

    cJSON_AddBoolToObject(root, "success", true);
    cJSON_AddNumberToObject(root, "error_code", 0);

    cJSON* data = cJSON_CreateObject();
    cJSON_AddStringToObject(data, "status", status);
    cJSON_AddStringToObject(data, "version", version);
    cJSON_AddNumberToObject(data, "uptime", (double)uptime);
    cJSON_AddBoolToObject(data, "suricata_running", suricata_running);
    cJSON_AddItemToObject(root, "data", data);

    char* json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    return json;
}

/**
 * Create an event list response.
 */
char* vnids_events_response(const vnids_security_event_t* events, size_t count) {
    cJSON* root = cJSON_CreateObject();
    if (!root) return NULL;

    cJSON_AddBoolToObject(root, "success", true);
    cJSON_AddNumberToObject(root, "error_code", 0);

    cJSON* events_array = cJSON_CreateArray();
    for (size_t i = 0; i < count; i++) {
        char* event_json = vnids_event_to_json_alloc(&events[i]);
        if (event_json) {
            cJSON* event_obj = cJSON_Parse(event_json);
            if (event_obj) {
                cJSON_AddItemToArray(events_array, event_obj);
            }
            free(event_json);
        }
    }

    cJSON* data = cJSON_CreateObject();
    cJSON_AddNumberToObject(data, "count", (double)count);
    cJSON_AddItemToObject(data, "events", events_array);
    cJSON_AddItemToObject(root, "data", data);

    char* json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    return json;
}

/**
 * Parse a key-value pair from JSON params.
 */
int vnids_parse_config_param(const char* params_json, char* key, size_t key_len,
                              char* value, size_t value_len) {
    if (!params_json || !key || !value) return -1;

    cJSON* root = cJSON_Parse(params_json);
    if (!root) return -1;

    cJSON* key_item = cJSON_GetObjectItem(root, "key");
    cJSON* value_item = cJSON_GetObjectItem(root, "value");

    if (!key_item || !cJSON_IsString(key_item)) {
        cJSON_Delete(root);
        return -1;
    }

    strncpy(key, cJSON_GetStringValue(key_item), key_len - 1);
    key[key_len - 1] = '\0';

    if (value_item) {
        if (cJSON_IsString(value_item)) {
            strncpy(value, cJSON_GetStringValue(value_item), value_len - 1);
        } else {
            char* val_str = cJSON_PrintUnformatted(value_item);
            if (val_str) {
                strncpy(value, val_str, value_len - 1);
                free(val_str);
            }
        }
        value[value_len - 1] = '\0';
    } else {
        value[0] = '\0';
    }

    cJSON_Delete(root);
    return 0;
}
