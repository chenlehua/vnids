/**
 * VNIDS IPC Protocol Definitions
 *
 * Message structures for communication between vnidsd and Suricata.
 *
 * Copyright (c) 2026 VNIDS Authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef VNIDS_IPC_H
#define VNIDS_IPC_H

#include "vnids_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Socket paths */
#define VNIDS_SOCKET_DIR "/var/run/vnids"
#define VNIDS_EVENT_SOCKET VNIDS_SOCKET_DIR "/events.sock"
#define VNIDS_CONTROL_SOCKET VNIDS_SOCKET_DIR "/control.sock"
#define VNIDS_STATS_SOCKET VNIDS_SOCKET_DIR "/stats.sock"
#define VNIDS_API_SOCKET VNIDS_SOCKET_DIR "/api.sock"

/* Socket configuration */
#define VNIDS_SOCKET_BUFFER_SIZE 65536
#define VNIDS_SOCKET_BACKLOG 5
#define VNIDS_SOCKET_PERMISSIONS 0660

/* Message types */
typedef enum {
    VNIDS_MSG_EVENT = 0,
    VNIDS_MSG_CONTROL = 1,
    VNIDS_MSG_STATS = 2,
    VNIDS_MSG_ACK = 3,
    VNIDS_MSG_ERROR = 4,
} vnids_msg_type_t;

/* Control commands */
typedef enum {
    VNIDS_CMD_RELOAD_RULES = 0,
    VNIDS_CMD_GET_STATS = 1,
    VNIDS_CMD_SET_CONFIG = 2,
    VNIDS_CMD_SHUTDOWN = 3,
    VNIDS_CMD_STATUS = 4,
    VNIDS_CMD_LIST_RULES = 5,
    VNIDS_CMD_LIST_EVENTS = 6,
    VNIDS_CMD_VALIDATE_RULES = 7,
} vnids_command_t;

/* Error codes for IPC */
typedef enum {
    VNIDS_IPC_ERR_NONE = 0,
    VNIDS_IPC_ERR_INVALID_COMMAND = 1,
    VNIDS_IPC_ERR_INVALID_PARAMS = 2,
    VNIDS_IPC_ERR_INVALID_CONFIG_KEY = 3,
    VNIDS_IPC_ERR_RULE_PARSE = 4,
    VNIDS_IPC_ERR_RESOURCE_EXHAUSTED = 5,
    VNIDS_IPC_ERR_INTERNAL = 6,
    VNIDS_IPC_ERR_SHUTDOWN_IN_PROGRESS = 7,
} vnids_ipc_error_t;

/* IPC message header */
typedef struct {
    vnids_timestamp_t timestamp;
    vnids_msg_type_t type;
    uint32_t payload_len;
} vnids_ipc_header_t;

/* Control message */
typedef struct {
    vnids_command_t command;
    char request_id[VNIDS_UUID_LEN];
    /* params follow as JSON string */
} vnids_control_msg_t;

/* Acknowledgment message */
typedef struct {
    char request_id[VNIDS_UUID_LEN];
    vnids_command_t command;
    bool success;
    /* details follow as JSON string */
} vnids_ack_msg_t;

/* Error message */
typedef struct {
    char request_id[VNIDS_UUID_LEN];
    vnids_command_t command;
    vnids_ipc_error_t error_code;
    bool recoverable;
    char error_message[VNIDS_MAX_MSG_LEN];
} vnids_error_msg_t;

/* Statistics summary */
typedef struct {
    uint64_t uptime_seconds;
    /* Capture stats */
    uint64_t packets_captured;
    uint64_t bytes_captured;
    uint64_t packets_dropped;
    uint64_t capture_errors;
    /* Detection stats */
    uint64_t alerts_total;
    uint32_t rules_loaded;
    uint32_t rules_failed;
    /* Flow stats */
    uint32_t flows_active;
    uint64_t flows_total;
    uint64_t flows_tcp;
    uint64_t flows_udp;
    /* Memory stats */
    uint32_t memory_used_mb;
    uint32_t memory_limit_mb;
    /* Performance */
    uint32_t avg_latency_us;
    uint32_t p99_latency_us;
    uint32_t pps;
} vnids_stats_t;

/* Heartbeat message */
typedef struct {
    char protocol_version[16];
    char suricata_version[16];
    uint64_t uptime_seconds;
    uint32_t rules_loaded;
    uint32_t memory_used_mb;
} vnids_heartbeat_t;

/* IPC context */
typedef struct vnids_ipc_ctx vnids_ipc_ctx_t;

/* Create IPC context */
vnids_ipc_ctx_t* vnids_ipc_create(void);

/* Destroy IPC context */
void vnids_ipc_destroy(vnids_ipc_ctx_t* ctx);

/* Server operations */
int vnids_ipc_server_init(vnids_ipc_ctx_t* ctx, const char* socket_path);
int vnids_ipc_server_accept(vnids_ipc_ctx_t* ctx);
void vnids_ipc_server_close(vnids_ipc_ctx_t* ctx);

/* Client operations */
int vnids_ipc_client_connect(vnids_ipc_ctx_t* ctx, const char* socket_path);
void vnids_ipc_client_disconnect(vnids_ipc_ctx_t* ctx);

/* Message operations */
int vnids_ipc_send(vnids_ipc_ctx_t* ctx, const vnids_ipc_header_t* header, const void* payload);
int vnids_ipc_recv(vnids_ipc_ctx_t* ctx, vnids_ipc_header_t* header, void* payload, size_t max_len);

/* Convenience functions */
int vnids_ipc_send_command(vnids_ipc_ctx_t* ctx, vnids_command_t cmd, const char* request_id, const char* params_json);
int vnids_ipc_send_ack(vnids_ipc_ctx_t* ctx, const char* request_id, vnids_command_t cmd, const char* details_json);
int vnids_ipc_send_error(vnids_ipc_ctx_t* ctx, const char* request_id, vnids_command_t cmd, vnids_ipc_error_t err, const char* msg);

/* Convert IPC error to string */
const char* vnids_ipc_error_str(vnids_ipc_error_t err);

/* Convert command to string */
const char* vnids_command_str(vnids_command_t cmd);

#ifdef __cplusplus
}
#endif

#endif /* VNIDS_IPC_H */
