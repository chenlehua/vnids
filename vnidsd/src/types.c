/**
 * VNIDS Daemon - Type Conversion Functions
 *
 * Copyright (c) 2026 VNIDS Authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include "vnids_types.h"
#include "vnids_ipc.h"

const char* vnids_result_str(vnids_result_t result) {
    switch (result) {
        case VNIDS_OK:              return "OK";
        case VNIDS_ERROR:           return "Generic error";
        case VNIDS_ERROR_NOMEM:     return "Out of memory";
        case VNIDS_ERROR_INVALID:   return "Invalid argument";
        case VNIDS_ERROR_NOT_FOUND: return "Not found";
        case VNIDS_ERROR_TIMEOUT:   return "Timeout";
        case VNIDS_ERROR_IO:        return "I/O error";
        case VNIDS_ERROR_PARSE:     return "Parse error";
        case VNIDS_ERROR_CONFIG:    return "Configuration error";
        case VNIDS_ERROR_IPC:       return "IPC error";
        case VNIDS_ERROR_DB:        return "Database error";
        case VNIDS_ERROR_SURICATA:  return "Suricata error";
        default:                    return "Unknown error";
    }
}

const char* vnids_severity_str(vnids_severity_t severity) {
    switch (severity) {
        case VNIDS_SEVERITY_CRITICAL: return "critical";
        case VNIDS_SEVERITY_HIGH:     return "high";
        case VNIDS_SEVERITY_MEDIUM:   return "medium";
        case VNIDS_SEVERITY_LOW:      return "low";
        case VNIDS_SEVERITY_INFO:     return "info";
        default:                      return "unknown";
    }
}

const char* vnids_protocol_str(vnids_protocol_t proto) {
    switch (proto) {
        case VNIDS_PROTO_TCP:      return "tcp";
        case VNIDS_PROTO_UDP:      return "udp";
        case VNIDS_PROTO_ICMP:     return "icmp";
        case VNIDS_PROTO_IGMP:     return "igmp";
        case VNIDS_PROTO_SOMEIP:   return "someip";
        case VNIDS_PROTO_DOIP:     return "doip";
        case VNIDS_PROTO_GBT32960: return "gbt32960";
        case VNIDS_PROTO_HTTP:     return "http";
        case VNIDS_PROTO_TLS:      return "tls";
        case VNIDS_PROTO_DNS:      return "dns";
        case VNIDS_PROTO_MQTT:     return "mqtt";
        case VNIDS_PROTO_FTP:      return "ftp";
        case VNIDS_PROTO_TELNET:   return "telnet";
        default:                   return "unknown";
    }
}

const char* vnids_event_type_str(vnids_event_type_t type) {
    switch (type) {
        case VNIDS_EVENT_ALERT:   return "alert";
        case VNIDS_EVENT_ANOMALY: return "anomaly";
        case VNIDS_EVENT_FLOW:    return "flow";
        case VNIDS_EVENT_STATS:   return "stats";
        default:                  return "unknown";
    }
}

const char* vnids_ipc_error_str(vnids_ipc_error_t err) {
    switch (err) {
        case VNIDS_IPC_ERR_NONE:               return "No error";
        case VNIDS_IPC_ERR_INVALID_COMMAND:    return "Invalid command";
        case VNIDS_IPC_ERR_INVALID_PARAMS:     return "Invalid parameters";
        case VNIDS_IPC_ERR_INVALID_CONFIG_KEY: return "Invalid config key";
        case VNIDS_IPC_ERR_RULE_PARSE:         return "Rule parse error";
        case VNIDS_IPC_ERR_RESOURCE_EXHAUSTED: return "Resource exhausted";
        case VNIDS_IPC_ERR_INTERNAL:           return "Internal error";
        case VNIDS_IPC_ERR_SHUTDOWN_IN_PROGRESS: return "Shutdown in progress";
        default:                               return "Unknown error";
    }
}

const char* vnids_command_str(vnids_command_t cmd) {
    switch (cmd) {
        case VNIDS_CMD_RELOAD_RULES:   return "reload_rules";
        case VNIDS_CMD_GET_STATS:      return "get_stats";
        case VNIDS_CMD_SET_CONFIG:     return "set_config";
        case VNIDS_CMD_SHUTDOWN:       return "shutdown";
        case VNIDS_CMD_STATUS:         return "status";
        case VNIDS_CMD_LIST_RULES:     return "list_rules";
        case VNIDS_CMD_LIST_EVENTS:    return "list_events";
        case VNIDS_CMD_VALIDATE_RULES: return "validate_rules";
        default:                       return "unknown";
    }
}
