/**
 * VNIDS Daemon - EVE JSON Parser
 *
 * Parses Suricata EVE JSON events into internal structures.
 *
 * Copyright (c) 2026 VNIDS Authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <cjson/cJSON.h>

#include "vnids_event.h"
#include "vnids_types.h"
#include "vnids_ipc.h"
#include "vnids_log.h"

/**
 * Parse ISO 8601 timestamp string to vnids_timestamp_t.
 * Format: "2024-01-15T10:30:45.123456+0000"
 */
static int parse_timestamp(const char* ts_str, vnids_timestamp_t* ts) {
    if (!ts_str || !ts) return -1;

    struct tm tm;
    memset(&tm, 0, sizeof(tm));
    memset(ts, 0, sizeof(vnids_timestamp_t));

    /* Parse date and time portion */
    char* remaining = strptime(ts_str, "%Y-%m-%dT%H:%M:%S", &tm);
    if (!remaining) {
        return -1;
    }

    ts->sec = mktime(&tm);

    /* Parse optional microseconds (.123456) */
    if (*remaining == '.') {
        remaining++;
        char usec_str[7] = "000000";
        int i = 0;
        while (*remaining >= '0' && *remaining <= '9' && i < 6) {
            usec_str[i++] = *remaining++;
        }
        ts->usec = (uint32_t)atoi(usec_str);
    }

    return 0;
}

/**
 * Parse severity string to enum.
 */
static vnids_severity_t parse_severity(int priority) {
    switch (priority) {
        case 1: return VNIDS_SEVERITY_CRITICAL;
        case 2: return VNIDS_SEVERITY_HIGH;
        case 3: return VNIDS_SEVERITY_MEDIUM;
        case 4: return VNIDS_SEVERITY_LOW;
        default: return VNIDS_SEVERITY_INFO;
    }
}

/**
 * Parse protocol string to enum.
 */
static vnids_protocol_t parse_protocol(const char* proto_str,
                                        const char* app_proto_str) {
    if (app_proto_str) {
        if (strcasecmp(app_proto_str, "http") == 0) return VNIDS_PROTO_HTTP;
        if (strcasecmp(app_proto_str, "tls") == 0) return VNIDS_PROTO_TLS;
        if (strcasecmp(app_proto_str, "dns") == 0) return VNIDS_PROTO_DNS;
        if (strcasecmp(app_proto_str, "mqtt") == 0) return VNIDS_PROTO_MQTT;
        if (strcasecmp(app_proto_str, "ftp") == 0) return VNIDS_PROTO_FTP;
        if (strcasecmp(app_proto_str, "someip") == 0) return VNIDS_PROTO_SOMEIP;
        if (strcasecmp(app_proto_str, "doip") == 0) return VNIDS_PROTO_DOIP;
    }

    if (proto_str) {
        if (strcasecmp(proto_str, "TCP") == 0) return VNIDS_PROTO_TCP;
        if (strcasecmp(proto_str, "UDP") == 0) return VNIDS_PROTO_UDP;
        if (strcasecmp(proto_str, "ICMP") == 0) return VNIDS_PROTO_ICMP;
        if (strcasecmp(proto_str, "IGMP") == 0) return VNIDS_PROTO_IGMP;
    }

    return VNIDS_PROTO_TCP;  /* Default */
}

/**
 * Copy string safely with null termination.
 */
static void safe_strcpy(char* dest, size_t dest_size, const char* src) {
    if (!dest || dest_size == 0) return;
    if (!src) {
        dest[0] = '\0';
        return;
    }
    strncpy(dest, src, dest_size - 1);
    dest[dest_size - 1] = '\0';
}

/**
 * Get string value from cJSON object.
 */
static const char* get_string(cJSON* obj, const char* key) {
    cJSON* item = cJSON_GetObjectItem(obj, key);
    if (item && cJSON_IsString(item)) {
        return cJSON_GetStringValue(item);
    }
    return NULL;
}

/**
 * Get integer value from cJSON object.
 */
static int64_t get_int(cJSON* obj, const char* key, int64_t def) {
    cJSON* item = cJSON_GetObjectItem(obj, key);
    if (item && cJSON_IsNumber(item)) {
        return (int64_t)cJSON_GetNumberValue(item);
    }
    return def;
}

/**
 * Parse an alert event from EVE JSON.
 */
static int parse_alert_event(cJSON* root, vnids_security_event_t* event) {
    event->event_type = VNIDS_EVENT_ALERT;

    /* Parse alert object */
    cJSON* alert = cJSON_GetObjectItem(root, "alert");
    if (!alert) {
        LOG_WARN("Alert event missing 'alert' object");
        return -1;
    }

    event->rule_sid = (uint32_t)get_int(alert, "signature_id", 0);
    event->rule_gid = (uint32_t)get_int(alert, "gid", 1);
    event->severity = parse_severity((int)get_int(alert, "severity", 4));

    const char* signature = get_string(alert, "signature");
    safe_strcpy(event->message, sizeof(event->message), signature);

    return 0;
}

/**
 * Parse an anomaly event from EVE JSON.
 */
static int parse_anomaly_event(cJSON* root, vnids_security_event_t* event) {
    event->event_type = VNIDS_EVENT_ANOMALY;
    event->severity = VNIDS_SEVERITY_MEDIUM;

    cJSON* anomaly = cJSON_GetObjectItem(root, "anomaly");
    if (anomaly) {
        const char* type = get_string(anomaly, "type");
        safe_strcpy(event->message, sizeof(event->message),
                    type ? type : "Network anomaly detected");
    }

    return 0;
}

/**
 * Parse common flow fields from EVE JSON.
 */
static void parse_flow_fields(cJSON* root, vnids_security_event_t* event) {
    /* Source/destination addresses */
    const char* src_ip = get_string(root, "src_ip");
    const char* dest_ip = get_string(root, "dest_ip");
    safe_strcpy(event->src_addr, sizeof(event->src_addr), src_ip);
    safe_strcpy(event->dst_addr, sizeof(event->dst_addr), dest_ip);

    /* Ports */
    event->src_port = (uint16_t)get_int(root, "src_port", 0);
    event->dst_port = (uint16_t)get_int(root, "dest_port", 0);

    /* Protocol */
    const char* proto = get_string(root, "proto");
    const char* app_proto = get_string(root, "app_proto");
    event->protocol = parse_protocol(proto, app_proto);
}

/**
 * Parse SOME/IP metadata if present.
 */
static void parse_someip_metadata(cJSON* root, vnids_someip_metadata_t* meta) {
    cJSON* someip = cJSON_GetObjectItem(root, "someip");
    if (!someip) return;

    meta->service_id = (uint16_t)get_int(someip, "service_id", 0);
    meta->method_id = (uint16_t)get_int(someip, "method_id", 0);
    meta->client_id = (uint16_t)get_int(someip, "client_id", 0);
    meta->session_id = (uint16_t)get_int(someip, "session_id", 0);
    meta->message_type = (uint8_t)get_int(someip, "message_type", 0);
    meta->return_code = (uint8_t)get_int(someip, "return_code", 0);
}

/**
 * Parse DoIP metadata if present.
 */
static void parse_doip_metadata(cJSON* root, vnids_doip_metadata_t* meta) {
    cJSON* doip = cJSON_GetObjectItem(root, "doip");
    if (!doip) return;

    meta->source_address = (uint16_t)get_int(doip, "source_address", 0);
    meta->target_address = (uint16_t)get_int(doip, "target_address", 0);
    meta->payload_type = (uint16_t)get_int(doip, "payload_type", 0);
}

/**
 * Parse a complete EVE JSON line into a security event.
 * Returns 0 on success, -1 on error.
 */
int vnids_eve_parse(const char* json_line, vnids_security_event_t* event) {
    if (!json_line || !event) {
        return -1;
    }

    memset(event, 0, sizeof(vnids_security_event_t));

    cJSON* root = cJSON_Parse(json_line);
    if (!root) {
        LOG_ERROR("Failed to parse EVE JSON: %s", cJSON_GetErrorPtr());
        return -1;
    }

    /* Parse timestamp */
    const char* timestamp = get_string(root, "timestamp");
    if (timestamp) {
        parse_timestamp(timestamp, &event->timestamp);
    }

    /* Parse event type */
    const char* event_type = get_string(root, "event_type");
    if (!event_type) {
        cJSON_Delete(root);
        return -1;
    }

    /* Parse common flow fields */
    parse_flow_fields(root, event);

    /* Parse type-specific fields */
    int result = 0;
    if (strcmp(event_type, "alert") == 0) {
        result = parse_alert_event(root, event);
    } else if (strcmp(event_type, "anomaly") == 0) {
        result = parse_anomaly_event(root, event);
    } else if (strcmp(event_type, "flow") == 0) {
        event->event_type = VNIDS_EVENT_FLOW;
        /* Flow events are typically not security events */
        result = -1;  /* Skip flow events for now */
    } else if (strcmp(event_type, "stats") == 0) {
        event->event_type = VNIDS_EVENT_STATS;
        result = -1;  /* Stats handled separately */
    } else {
        /* Unknown event type, try to treat as alert if it has alert info */
        cJSON* alert = cJSON_GetObjectItem(root, "alert");
        if (alert) {
            result = parse_alert_event(root, event);
        } else {
            result = -1;
        }
    }

    /* Parse protocol-specific metadata */
    if (result == 0) {
        vnids_someip_metadata_t someip_meta;
        memset(&someip_meta, 0, sizeof(someip_meta));
        parse_someip_metadata(root, &someip_meta);
        if (someip_meta.service_id != 0) {
            event->protocol = VNIDS_PROTO_SOMEIP;
        }

        vnids_doip_metadata_t doip_meta;
        memset(&doip_meta, 0, sizeof(doip_meta));
        parse_doip_metadata(root, &doip_meta);
        if (doip_meta.payload_type != 0) {
            event->protocol = VNIDS_PROTO_DOIP;
        }
    }

    cJSON_Delete(root);
    return result;
}

/**
 * Parse EVE stats event into vnids_stats_t.
 */
int vnids_eve_parse_stats(const char* json_line, vnids_stats_t* stats) {
    if (!json_line || !stats) {
        return -1;
    }

    memset(stats, 0, sizeof(vnids_stats_t));

    cJSON* root = cJSON_Parse(json_line);
    if (!root) {
        return -1;
    }

    /* Check event type */
    const char* event_type = get_string(root, "event_type");
    if (!event_type || strcmp(event_type, "stats") != 0) {
        cJSON_Delete(root);
        return -1;
    }

    cJSON* stats_obj = cJSON_GetObjectItem(root, "stats");
    if (!stats_obj) {
        cJSON_Delete(root);
        return -1;
    }

    /* Parse capture stats */
    cJSON* capture = cJSON_GetObjectItem(stats_obj, "capture");
    if (capture) {
        stats->packets_captured = (uint64_t)get_int(capture, "kernel_packets", 0);
        stats->packets_dropped = (uint64_t)get_int(capture, "kernel_drops", 0);
    }

    /* Parse decoder stats */
    cJSON* decoder = cJSON_GetObjectItem(stats_obj, "decoder");
    if (decoder) {
        stats->bytes_captured = (uint64_t)get_int(decoder, "bytes", 0);
    }

    /* Parse detect stats */
    cJSON* detect = cJSON_GetObjectItem(stats_obj, "detect");
    if (detect) {
        stats->alerts_total = (uint64_t)get_int(detect, "alert", 0);
    }

    /* Parse flow manager stats */
    cJSON* flow_mgr = cJSON_GetObjectItem(stats_obj, "flow_mgr");
    if (flow_mgr) {
        stats->flows_active = (uint32_t)get_int(flow_mgr, "flows_active", 0);
    }

    /* Parse memory stats */
    cJSON* memcap = cJSON_GetObjectItem(stats_obj, "flow");
    if (memcap) {
        uint64_t memuse = (uint64_t)get_int(memcap, "memuse", 0);
        stats->memory_used_mb = (uint32_t)(memuse / (1024 * 1024));
    }

    /* Parse uptime */
    stats->uptime_seconds = (uint64_t)get_int(stats_obj, "uptime", 0);

    cJSON_Delete(root);
    return 0;
}
