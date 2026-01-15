/**
 * VNIDS CLI - Command Handlers
 *
 * Implementation of CLI commands.
 *
 * Copyright (c) 2026 VNIDS Authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <cjson/cJSON.h>

#include "vnids_types.h"
#include "vnids_ipc.h"

/* External functions */
extern char* vnids_cli_client_request(const char* request);
extern int vnids_cli_is_json_output(void);
extern int vnids_cli_is_quiet(void);
extern void output_json(const char* json);
extern void output_table(const char* json, const char* type);

/**
 * Helper to create a request JSON.
 */
static char* create_request(const char* command, cJSON* params) {
    cJSON* root = cJSON_CreateObject();
    if (!root) return NULL;

    cJSON_AddStringToObject(root, "command", command);

    if (params) {
        cJSON_AddItemToObject(root, "params", params);
    }

    char* json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    return json;
}

/**
 * Parse response and check for success.
 */
static int check_response(const char* response, cJSON** data_out) {
    if (!response) {
        fprintf(stderr, "Error: No response from daemon\n");
        return -1;
    }

    cJSON* root = cJSON_Parse(response);
    if (!root) {
        fprintf(stderr, "Error: Invalid response format\n");
        return -1;
    }

    cJSON* success = cJSON_GetObjectItem(root, "success");
    if (!success || !cJSON_IsTrue(success)) {
        cJSON* error = cJSON_GetObjectItem(root, "error");
        cJSON* message = cJSON_GetObjectItem(root, "message");

        const char* err_str = error ? cJSON_GetStringValue(error) : "Unknown error";
        const char* msg_str = message ? cJSON_GetStringValue(message) : "";

        fprintf(stderr, "Error: %s", err_str);
        if (strlen(msg_str) > 0) {
            fprintf(stderr, " - %s", msg_str);
        }
        fprintf(stderr, "\n");

        cJSON_Delete(root);
        return -1;
    }

    if (data_out) {
        *data_out = cJSON_DetachItemFromObject(root, "data");
    }

    cJSON_Delete(root);
    return 0;
}

/**
 * Status command.
 */
int cmd_status(int argc, char** argv) {
    (void)argc;
    (void)argv;

    char* request = create_request("status", NULL);
    if (!request) return 1;

    char* response = vnids_cli_client_request(request);
    free(request);

    if (vnids_cli_is_json_output()) {
        output_json(response);
        free(response);
        return 0;
    }

    cJSON* data = NULL;
    if (check_response(response, &data) < 0) {
        free(response);
        return 1;
    }

    if (data) {
        cJSON* status = cJSON_GetObjectItem(data, "status");
        cJSON* version = cJSON_GetObjectItem(data, "version");
        cJSON* uptime = cJSON_GetObjectItem(data, "uptime");
        cJSON* suricata = cJSON_GetObjectItem(data, "suricata_running");

        printf("VNIDS Daemon Status\n");
        printf("-------------------\n");
        printf("Status:           %s\n", status ? cJSON_GetStringValue(status) : "unknown");
        printf("Version:          %s\n", version ? cJSON_GetStringValue(version) : "unknown");
        printf("Uptime:           %lld seconds\n", uptime ? (long long)cJSON_GetNumberValue(uptime) : 0);
        printf("Suricata:         %s\n", (suricata && cJSON_IsTrue(suricata)) ? "running" : "stopped");

        cJSON_Delete(data);
    }

    free(response);
    return 0;
}

/**
 * Stats command.
 */
int cmd_stats(int argc, char** argv) {
    (void)argc;
    (void)argv;

    char* request = create_request("get_stats", NULL);
    if (!request) return 1;

    char* response = vnids_cli_client_request(request);
    free(request);

    if (vnids_cli_is_json_output()) {
        output_json(response);
        free(response);
        return 0;
    }

    cJSON* data = NULL;
    if (check_response(response, &data) < 0) {
        free(response);
        return 1;
    }

    if (data) {
        printf("VNIDS Statistics\n");
        printf("----------------\n");

        cJSON* item;
        if ((item = cJSON_GetObjectItem(data, "packets_received"))) {
            printf("Packets received:  %lld\n", (long long)cJSON_GetNumberValue(item));
        }
        if ((item = cJSON_GetObjectItem(data, "packets_decoded"))) {
            printf("Packets decoded:   %lld\n", (long long)cJSON_GetNumberValue(item));
        }
        if ((item = cJSON_GetObjectItem(data, "packets_dropped"))) {
            printf("Packets dropped:   %lld\n", (long long)cJSON_GetNumberValue(item));
        }
        if ((item = cJSON_GetObjectItem(data, "bytes_received"))) {
            printf("Bytes received:    %lld\n", (long long)cJSON_GetNumberValue(item));
        }
        if ((item = cJSON_GetObjectItem(data, "alerts_triggered"))) {
            printf("Alerts triggered:  %lld\n", (long long)cJSON_GetNumberValue(item));
        }
        if ((item = cJSON_GetObjectItem(data, "flows_tracked"))) {
            printf("Flows tracked:     %lld\n", (long long)cJSON_GetNumberValue(item));
        }
        if ((item = cJSON_GetObjectItem(data, "memory_used"))) {
            printf("Memory used:       %lld bytes\n", (long long)cJSON_GetNumberValue(item));
        }
        if ((item = cJSON_GetObjectItem(data, "uptime_seconds"))) {
            printf("Uptime:            %lld seconds\n", (long long)cJSON_GetNumberValue(item));
        }

        cJSON_Delete(data);
    }

    free(response);
    return 0;
}

/**
 * Events command.
 */
int cmd_events(int argc, char** argv) {
    static struct option long_options[] = {
        {"limit",    required_argument, NULL, 'n'},
        {"severity", required_argument, NULL, 's'},
        {"since",    required_argument, NULL, 't'},
        {"help",     no_argument,       NULL, 'h'},
        {NULL,       0,                 NULL, 0}
    };

    int limit = 10;
    const char* severity = NULL;
    const char* since = NULL;

    optind = 1;  /* Reset getopt */
    int opt;
    while ((opt = getopt_long(argc, argv, "n:s:t:h", long_options, NULL)) != -1) {
        switch (opt) {
            case 'n':
                limit = atoi(optarg);
                if (limit <= 0) limit = 10;
                break;
            case 's':
                severity = optarg;
                break;
            case 't':
                since = optarg;
                break;
            case 'h':
                printf("Usage: vnids-cli events [OPTIONS]\n");
                printf("\nOptions:\n");
                printf("  -n, --limit N     Limit to N events (default: 10)\n");
                printf("  -s, --severity S  Filter by severity (critical, high, medium, low)\n");
                printf("  -t, --since TIME  Show events since TIME\n");
                printf("  -h, --help        Show this help\n");
                return 0;
            default:
                return 1;
        }
    }

    cJSON* params = cJSON_CreateObject();
    cJSON_AddNumberToObject(params, "limit", limit);
    if (severity) cJSON_AddStringToObject(params, "severity", severity);
    if (since) cJSON_AddStringToObject(params, "since", since);

    char* request = create_request("list_events", params);
    if (!request) return 1;

    char* response = vnids_cli_client_request(request);
    free(request);

    if (vnids_cli_is_json_output()) {
        output_json(response);
        free(response);
        return 0;
    }

    cJSON* data = NULL;
    if (check_response(response, &data) < 0) {
        free(response);
        return 1;
    }

    if (data) {
        cJSON* events = cJSON_GetObjectItem(data, "events");
        cJSON* count = cJSON_GetObjectItem(data, "count");

        int event_count = count ? (int)cJSON_GetNumberValue(count) : 0;
        printf("Security Events (%d)\n", event_count);
        printf("%-20s %-10s %-15s %-15s %s\n",
               "TIMESTAMP", "SEVERITY", "SRC", "DST", "MESSAGE");
        printf("%-20s %-10s %-15s %-15s %s\n",
               "--------------------", "----------", "---------------",
               "---------------", "----------------------------------------");

        if (events && cJSON_IsArray(events)) {
            cJSON* event;
            cJSON_ArrayForEach(event, events) {
                cJSON* ts = cJSON_GetObjectItem(event, "timestamp");
                cJSON* sev = cJSON_GetObjectItem(event, "severity");
                cJSON* src = cJSON_GetObjectItem(event, "src_ip");
                cJSON* dst = cJSON_GetObjectItem(event, "dst_ip");
                cJSON* msg = cJSON_GetObjectItem(event, "signature_msg");

                printf("%-20lld %-10s %-15s %-15s %s\n",
                       ts ? (long long)cJSON_GetNumberValue(ts) : 0,
                       sev ? cJSON_GetStringValue(sev) : "unknown",
                       src ? cJSON_GetStringValue(src) : "-",
                       dst ? cJSON_GetStringValue(dst) : "-",
                       msg ? cJSON_GetStringValue(msg) : "-");
            }
        }

        cJSON_Delete(data);
    }

    free(response);
    return 0;
}

/**
 * Rules command.
 */
int cmd_rules(int argc, char** argv) {
    static struct option long_options[] = {
        {"list",     no_argument,       NULL, 'l'},
        {"validate", no_argument,       NULL, 'v'},
        {"help",     no_argument,       NULL, 'h'},
        {NULL,       0,                 NULL, 0}
    };

    int do_list = 1;
    int do_validate = 0;

    optind = 1;
    int opt;
    while ((opt = getopt_long(argc, argv, "lvh", long_options, NULL)) != -1) {
        switch (opt) {
            case 'l':
                do_list = 1;
                do_validate = 0;
                break;
            case 'v':
                do_validate = 1;
                do_list = 0;
                break;
            case 'h':
                printf("Usage: vnids-cli rules [OPTIONS]\n");
                printf("\nOptions:\n");
                printf("  -l, --list       List loaded rules\n");
                printf("  -v, --validate   Validate rules\n");
                printf("  -h, --help       Show this help\n");
                return 0;
            default:
                return 1;
        }
    }

    const char* command = do_validate ? "validate_rules" : "list_rules";

    char* request = create_request(command, NULL);
    if (!request) return 1;

    char* response = vnids_cli_client_request(request);
    free(request);

    if (vnids_cli_is_json_output()) {
        output_json(response);
        free(response);
        return 0;
    }

    cJSON* data = NULL;
    if (check_response(response, &data) < 0) {
        free(response);
        return 1;
    }

    cJSON* message = cJSON_GetObjectItem(data ? data : cJSON_Parse(response), "message");
    if (message) {
        printf("%s\n", cJSON_GetStringValue(message));
    }

    if (data) cJSON_Delete(data);
    free(response);
    return 0;
}

/**
 * Reload command.
 */
int cmd_reload(int argc, char** argv) {
    (void)argc;
    (void)argv;

    char* request = create_request("reload_rules", NULL);
    if (!request) return 1;

    char* response = vnids_cli_client_request(request);
    free(request);

    if (vnids_cli_is_json_output()) {
        output_json(response);
        free(response);
        return 0;
    }

    cJSON* data = NULL;
    if (check_response(response, &data) < 0) {
        free(response);
        return 1;
    }

    if (!vnids_cli_is_quiet()) {
        printf("Rules reloaded successfully\n");
    }

    if (data) cJSON_Delete(data);
    free(response);
    return 0;
}

/**
 * Config command.
 */
int cmd_config(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage: vnids-cli config [KEY] [VALUE]\n");
        printf("\nIf VALUE is omitted, shows current value of KEY.\n");
        printf("If KEY is omitted, lists all configuration.\n");
        printf("\nConfigurable keys:\n");
        printf("  log_level         Logging level (trace, debug, info, warn, error)\n");
        printf("  eve_socket        Path to EVE socket\n");
        printf("  rules_dir         Path to rules directory\n");
        printf("  max_events        Maximum events to store\n");
        printf("  watchdog_interval Watchdog check interval (seconds)\n");
        printf("  stats_interval    Statistics interval (seconds)\n");
        return 0;
    }

    const char* key = argv[1];
    const char* value = (argc > 2) ? argv[2] : NULL;

    cJSON* params = cJSON_CreateObject();
    cJSON_AddStringToObject(params, "key", key);
    if (value) {
        cJSON_AddStringToObject(params, "value", value);
    }

    char* request = create_request("set_config", params);
    if (!request) return 1;

    char* response = vnids_cli_client_request(request);
    free(request);

    if (vnids_cli_is_json_output()) {
        output_json(response);
        free(response);
        return 0;
    }

    cJSON* data = NULL;
    if (check_response(response, &data) < 0) {
        free(response);
        return 1;
    }

    if (!vnids_cli_is_quiet()) {
        if (value) {
            printf("Configuration updated: %s = %s\n", key, value);
        }
    }

    if (data) cJSON_Delete(data);
    free(response);
    return 0;
}

/**
 * Shutdown command.
 */
int cmd_shutdown(int argc, char** argv) {
    (void)argc;
    (void)argv;

    if (!vnids_cli_is_quiet()) {
        printf("Sending shutdown command to daemon...\n");
    }

    char* request = create_request("shutdown", NULL);
    if (!request) return 1;

    char* response = vnids_cli_client_request(request);
    free(request);

    if (vnids_cli_is_json_output()) {
        output_json(response);
        free(response);
        return 0;
    }

    cJSON* data = NULL;
    if (check_response(response, &data) < 0) {
        free(response);
        return 1;
    }

    if (!vnids_cli_is_quiet()) {
        printf("Shutdown initiated\n");
    }

    if (data) cJSON_Delete(data);
    free(response);
    return 0;
}
