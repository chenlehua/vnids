/**
 * VNIDS CLI - Output Formatting
 *
 * Functions for formatting and displaying output.
 *
 * Copyright (c) 2026 VNIDS Authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cjson/cJSON.h>

/**
 * Output raw JSON.
 */
void output_json(const char* json) {
    if (!json) {
        printf("{\"error\": \"No response\"}\n");
        return;
    }

    /* Pretty print the JSON */
    cJSON* root = cJSON_Parse(json);
    if (root) {
        char* formatted = cJSON_Print(root);
        if (formatted) {
            printf("%s\n", formatted);
            free(formatted);
        } else {
            printf("%s\n", json);
        }
        cJSON_Delete(root);
    } else {
        printf("%s\n", json);
    }
}

/**
 * Output as a formatted table.
 */
void output_table(const char* json, const char* type) {
    if (!json || !type) {
        fprintf(stderr, "Error: No data to display\n");
        return;
    }

    cJSON* root = cJSON_Parse(json);
    if (!root) {
        fprintf(stderr, "Error: Invalid JSON\n");
        return;
    }

    cJSON* data = cJSON_GetObjectItem(root, "data");
    if (!data) {
        data = root;
    }

    if (strcmp(type, "events") == 0) {
        cJSON* events = cJSON_GetObjectItem(data, "events");
        if (events && cJSON_IsArray(events)) {
            printf("%-20s %-10s %-18s %-18s %-6s %s\n",
                   "TIMESTAMP", "SEVERITY", "SOURCE", "DESTINATION", "PROTO", "MESSAGE");
            printf("%-20s %-10s %-18s %-18s %-6s %s\n",
                   "--------------------", "----------", "------------------",
                   "------------------", "------", "----------------------------------------");

            cJSON* event;
            cJSON_ArrayForEach(event, events) {
                cJSON* ts = cJSON_GetObjectItem(event, "timestamp");
                cJSON* sev = cJSON_GetObjectItem(event, "severity");
                cJSON* src = cJSON_GetObjectItem(event, "src_ip");
                cJSON* sp = cJSON_GetObjectItem(event, "src_port");
                cJSON* dst = cJSON_GetObjectItem(event, "dst_ip");
                cJSON* dp = cJSON_GetObjectItem(event, "dst_port");
                cJSON* proto = cJSON_GetObjectItem(event, "protocol");
                cJSON* msg = cJSON_GetObjectItem(event, "signature_msg");

                char src_addr[32], dst_addr[32];
                snprintf(src_addr, sizeof(src_addr), "%s:%d",
                         src ? cJSON_GetStringValue(src) : "?",
                         sp ? (int)cJSON_GetNumberValue(sp) : 0);
                snprintf(dst_addr, sizeof(dst_addr), "%s:%d",
                         dst ? cJSON_GetStringValue(dst) : "?",
                         dp ? (int)cJSON_GetNumberValue(dp) : 0);

                printf("%-20lld %-10s %-18s %-18s %-6s %.40s\n",
                       ts ? (long long)cJSON_GetNumberValue(ts) : 0,
                       sev ? cJSON_GetStringValue(sev) : "?",
                       src_addr,
                       dst_addr,
                       proto ? cJSON_GetStringValue(proto) : "?",
                       msg ? cJSON_GetStringValue(msg) : "-");
            }
        }
    } else if (strcmp(type, "stats") == 0) {
        printf("%-25s %s\n", "METRIC", "VALUE");
        printf("%-25s %s\n", "-------------------------", "--------------------");

        const char* metrics[] = {
            "packets_received", "packets_decoded", "packets_dropped",
            "bytes_received", "alerts_triggered", "flows_tracked",
            "memory_used", "uptime_seconds", NULL
        };

        for (int i = 0; metrics[i]; i++) {
            cJSON* item = cJSON_GetObjectItem(data, metrics[i]);
            if (item) {
                printf("%-25s %lld\n", metrics[i], (long long)cJSON_GetNumberValue(item));
            }
        }
    } else if (strcmp(type, "rules") == 0) {
        cJSON* rules = cJSON_GetObjectItem(data, "rules");
        if (rules && cJSON_IsArray(rules)) {
            printf("%-10s %-60s %s\n", "SID", "MESSAGE", "STATUS");
            printf("%-10s %-60s %s\n",
                   "----------", "------------------------------------------------------------", "--------");

            cJSON* rule;
            cJSON_ArrayForEach(rule, rules) {
                cJSON* sid = cJSON_GetObjectItem(rule, "sid");
                cJSON* msg = cJSON_GetObjectItem(rule, "msg");
                cJSON* enabled = cJSON_GetObjectItem(rule, "enabled");

                printf("%-10d %-60.60s %s\n",
                       sid ? (int)cJSON_GetNumberValue(sid) : 0,
                       msg ? cJSON_GetStringValue(msg) : "-",
                       (enabled && cJSON_IsTrue(enabled)) ? "enabled" : "disabled");
            }
        }
    }

    cJSON_Delete(root);
}

/**
 * Format bytes to human-readable string.
 */
void format_bytes(uint64_t bytes, char* buf, size_t buf_len) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unit_idx = 0;
    double value = (double)bytes;

    while (value >= 1024.0 && unit_idx < 4) {
        value /= 1024.0;
        unit_idx++;
    }

    snprintf(buf, buf_len, "%.2f %s", value, units[unit_idx]);
}

/**
 * Format seconds to human-readable duration.
 */
void format_duration(uint64_t seconds, char* buf, size_t buf_len) {
    if (seconds < 60) {
        snprintf(buf, buf_len, "%llu seconds", (unsigned long long)seconds);
    } else if (seconds < 3600) {
        snprintf(buf, buf_len, "%llu minutes %llu seconds",
                 (unsigned long long)(seconds / 60),
                 (unsigned long long)(seconds % 60));
    } else if (seconds < 86400) {
        snprintf(buf, buf_len, "%llu hours %llu minutes",
                 (unsigned long long)(seconds / 3600),
                 (unsigned long long)((seconds % 3600) / 60));
    } else {
        snprintf(buf, buf_len, "%llu days %llu hours",
                 (unsigned long long)(seconds / 86400),
                 (unsigned long long)((seconds % 86400) / 3600));
    }
}

/**
 * Print colored output if terminal supports it.
 */
void print_colored(const char* color, const char* text) {
    /* Simple color codes */
    const char* reset = "\033[0m";
    const char* color_code = "";

    if (strcmp(color, "red") == 0) {
        color_code = "\033[31m";
    } else if (strcmp(color, "green") == 0) {
        color_code = "\033[32m";
    } else if (strcmp(color, "yellow") == 0) {
        color_code = "\033[33m";
    } else if (strcmp(color, "blue") == 0) {
        color_code = "\033[34m";
    } else if (strcmp(color, "magenta") == 0) {
        color_code = "\033[35m";
    } else if (strcmp(color, "cyan") == 0) {
        color_code = "\033[36m";
    } else if (strcmp(color, "bold") == 0) {
        color_code = "\033[1m";
    }

    printf("%s%s%s", color_code, text, reset);
}

/**
 * Get color for severity level.
 */
const char* severity_color(const char* severity) {
    if (!severity) return "";

    if (strcmp(severity, "critical") == 0) return "red";
    if (strcmp(severity, "high") == 0) return "red";
    if (strcmp(severity, "medium") == 0) return "yellow";
    if (strcmp(severity, "low") == 0) return "cyan";
    if (strcmp(severity, "info") == 0) return "green";

    return "";
}
