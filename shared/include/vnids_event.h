/**
 * VNIDS Security Event Definitions
 *
 * Security event structures matching the event-schema.json contract.
 *
 * Copyright (c) 2026 VNIDS Authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef VNIDS_EVENT_H
#define VNIDS_EVENT_H

#include "vnids_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* SOME/IP metadata */
typedef struct {
    uint16_t service_id;
    uint16_t method_id;
    uint16_t client_id;
    uint16_t session_id;
    uint8_t message_type;
    uint8_t return_code;
} vnids_someip_metadata_t;

/* DoIP metadata */
typedef struct {
    uint16_t payload_type;
    uint16_t source_address;
    uint16_t target_address;
    uint8_t uds_service;
    uint8_t activation_type;
} vnids_doip_metadata_t;

/* GB/T 32960.3 metadata */
typedef struct {
    uint8_t command;
    char vin[VNIDS_MAX_VIN_LEN];
    uint8_t encryption;
} vnids_gbt32960_metadata_t;

/* HTTP metadata */
typedef struct {
    char method[16];
    char uri[256];
    char host[256];
    char user_agent[256];
    uint16_t status_code;
    char content_type[128];
} vnids_http_metadata_t;

/* DNS metadata */
typedef struct {
    char query_type[8];
    char query_name[256];
    char response_code[16];
} vnids_dns_metadata_t;

/* Flood attack metadata */
typedef struct {
    char attack_type[32];
    uint64_t packet_count;
    uint32_t duration_ms;
    uint32_t pps_rate;
    uint32_t threshold;
} vnids_flood_metadata_t;

/* Protocol-specific metadata union */
typedef union {
    vnids_someip_metadata_t someip;
    vnids_doip_metadata_t doip;
    vnids_gbt32960_metadata_t gbt32960;
    vnids_http_metadata_t http;
    vnids_dns_metadata_t dns;
    vnids_flood_metadata_t flood;
} vnids_metadata_t;

/* Security event structure */
typedef struct {
    char id[VNIDS_UUID_LEN];
    vnids_timestamp_t timestamp;
    vnids_event_type_t event_type;
    vnids_severity_t severity;
    char src_addr[VNIDS_MAX_IP_LEN];
    uint16_t src_port;
    char dst_addr[VNIDS_MAX_IP_LEN];
    uint16_t dst_port;
    vnids_protocol_t protocol;
    uint32_t rule_sid;
    uint32_t rule_gid;
    char message[VNIDS_MAX_MSG_LEN];
    vnids_metadata_t metadata;
    bool has_metadata;
    char session_id[VNIDS_UUID_LEN];
    char packet_hash[65]; /* SHA-256 hex + null */
} vnids_security_event_t;

/* Flow event structure */
typedef struct {
    char id[VNIDS_UUID_LEN];
    vnids_timestamp_t timestamp;
    uint64_t flow_id;
    vnids_flow_state_t state;
    char src_addr[VNIDS_MAX_IP_LEN];
    uint16_t src_port;
    char dst_addr[VNIDS_MAX_IP_LEN];
    uint16_t dst_port;
    vnids_protocol_t protocol;
    char app_proto[32];
    uint64_t pkts_toserver;
    uint64_t pkts_toclient;
    uint64_t bytes_toserver;
    uint64_t bytes_toclient;
    vnids_timestamp_t start;
    vnids_timestamp_t end;
} vnids_flow_event_t;

/* Event creation */
vnids_security_event_t* vnids_event_create(void);
void vnids_event_destroy(vnids_security_event_t* event);
void vnids_event_init(vnids_security_event_t* event);

/* Event serialization */
int vnids_event_to_json(const vnids_security_event_t* event, char* buf, size_t buf_len);
int vnids_event_from_json(vnids_security_event_t* event, const char* json);

/* Flow event serialization */
int vnids_flow_to_json(const vnids_flow_event_t* flow, char* buf, size_t buf_len);
int vnids_flow_from_json(vnids_flow_event_t* flow, const char* json);

/* UUID generation */
void vnids_uuid_generate(char* buf);

#ifdef __cplusplus
}
#endif

#endif /* VNIDS_EVENT_H */
