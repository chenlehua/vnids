/**
 * VNIDS Common Types
 *
 * Shared type definitions for VNIDS daemon and CLI.
 *
 * Copyright (c) 2026 VNIDS Authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef VNIDS_TYPES_H
#define VNIDS_TYPES_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Version information */
#define VNIDS_VERSION_MAJOR 1
#define VNIDS_VERSION_MINOR 0
#define VNIDS_VERSION_PATCH 0
#define VNIDS_VERSION_STRING "1.0.0"

/* Protocol version for IPC */
#define VNIDS_PROTOCOL_VERSION "1.0.0"

/* Limits */
#define VNIDS_MAX_PATH_LEN 256
#define VNIDS_MAX_MSG_LEN 256
#define VNIDS_MAX_IP_LEN 46  /* IPv6 + null */
#define VNIDS_MAX_VIN_LEN 18 /* 17 chars + null */
#define VNIDS_UUID_LEN 37   /* 36 chars + null */

/* Result type for error handling */
typedef enum {
    VNIDS_OK = 0,
    VNIDS_ERROR = -1,
    VNIDS_ERROR_NOMEM = -2,
    VNIDS_ERROR_INVALID = -3,
    VNIDS_ERROR_NOT_FOUND = -4,
    VNIDS_ERROR_TIMEOUT = -5,
    VNIDS_ERROR_IO = -6,
    VNIDS_ERROR_PARSE = -7,
    VNIDS_ERROR_CONFIG = -8,
    VNIDS_ERROR_IPC = -9,
    VNIDS_ERROR_DB = -10,
    VNIDS_ERROR_SURICATA = -11,
} vnids_result_t;

/* Severity levels (matches Suricata) */
typedef enum {
    VNIDS_SEVERITY_CRITICAL = 1,
    VNIDS_SEVERITY_HIGH = 2,
    VNIDS_SEVERITY_MEDIUM = 3,
    VNIDS_SEVERITY_LOW = 4,
    VNIDS_SEVERITY_INFO = 5,
} vnids_severity_t;

/* Event types */
typedef enum {
    VNIDS_EVENT_ALERT = 0,
    VNIDS_EVENT_ANOMALY = 1,
    VNIDS_EVENT_FLOW = 2,
    VNIDS_EVENT_STATS = 3,
} vnids_event_type_t;

/* Protocol types */
typedef enum {
    VNIDS_PROTO_UNKNOWN = 0,
    VNIDS_PROTO_TCP = 1,
    VNIDS_PROTO_UDP = 2,
    VNIDS_PROTO_ICMP = 3,
    VNIDS_PROTO_IGMP = 4,
    VNIDS_PROTO_SOMEIP = 10,
    VNIDS_PROTO_DOIP = 11,
    VNIDS_PROTO_GBT32960 = 12,
    VNIDS_PROTO_HTTP = 20,
    VNIDS_PROTO_TLS = 21,
    VNIDS_PROTO_DNS = 22,
    VNIDS_PROTO_MQTT = 23,
    VNIDS_PROTO_FTP = 24,
    VNIDS_PROTO_TELNET = 25,
} vnids_protocol_t;

/* Flow state */
typedef enum {
    VNIDS_FLOW_NEW = 0,
    VNIDS_FLOW_ESTABLISHED = 1,
    VNIDS_FLOW_CLOSED = 2,
    VNIDS_FLOW_TIMEOUT = 3,
} vnids_flow_state_t;

/* Ruleset status */
typedef enum {
    VNIDS_RULESET_PENDING = 0,
    VNIDS_RULESET_ACTIVE = 1,
    VNIDS_RULESET_ARCHIVED = 2,
    VNIDS_RULESET_FAILED = 3,
} vnids_ruleset_status_t;

/* Timestamp with microsecond precision */
typedef struct {
    time_t sec;
    uint32_t usec;
} vnids_timestamp_t;

/* Get current timestamp */
static inline vnids_timestamp_t vnids_timestamp_now(void) {
    vnids_timestamp_t ts;
    struct timespec spec;
    clock_gettime(CLOCK_REALTIME, &spec);
    ts.sec = spec.tv_sec;
    ts.usec = (uint32_t)(spec.tv_nsec / 1000);
    return ts;
}

/* Convert result to string */
const char* vnids_result_str(vnids_result_t result);

/* Convert severity to string */
const char* vnids_severity_str(vnids_severity_t severity);

/* Convert protocol to string */
const char* vnids_protocol_str(vnids_protocol_t proto);

/* Convert event type to string */
const char* vnids_event_type_str(vnids_event_type_t type);

#ifdef __cplusplus
}
#endif

#endif /* VNIDS_TYPES_H */
