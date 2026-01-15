/**
 * VNIDS CLI - Main Entry Point
 *
 * Command-line interface for controlling the VNIDS daemon.
 *
 * Copyright (c) 2026 VNIDS Authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>

#include "vnids_types.h"
#include "vnids_ipc.h"

/* Forward declarations */
int vnids_cli_client_connect(const char* socket_path);
void vnids_cli_client_disconnect(void);
char* vnids_cli_client_request(const char* request);

int cmd_status(int argc, char** argv);
int cmd_stats(int argc, char** argv);
int cmd_events(int argc, char** argv);
int cmd_rules(int argc, char** argv);
int cmd_reload(int argc, char** argv);
int cmd_config(int argc, char** argv);
int cmd_shutdown(int argc, char** argv);

void output_json(const char* json);
void output_table(const char* json, const char* type);

static void print_usage(const char* prog) {
    fprintf(stderr, "Usage: %s [OPTIONS] COMMAND [ARGS...]\n", prog);
    fprintf(stderr, "\n");
    fprintf(stderr, "VNIDS Command Line Interface v%s\n", VNIDS_VERSION_STRING);
    fprintf(stderr, "\n");
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -s, --socket PATH    Path to daemon socket (default: %s)\n", VNIDS_API_SOCKET);
    fprintf(stderr, "  -j, --json           Output in JSON format\n");
    fprintf(stderr, "  -q, --quiet          Quiet mode (errors only)\n");
    fprintf(stderr, "  -h, --help           Show this help message\n");
    fprintf(stderr, "  -v, --version        Show version information\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Commands:\n");
    fprintf(stderr, "  status               Show daemon status\n");
    fprintf(stderr, "  stats                Show statistics\n");
    fprintf(stderr, "  events [OPTIONS]     List security events\n");
    fprintf(stderr, "  rules [OPTIONS]      Manage detection rules\n");
    fprintf(stderr, "  reload               Reload detection rules\n");
    fprintf(stderr, "  config [KEY] [VALUE] Get or set configuration\n");
    fprintf(stderr, "  shutdown             Stop the daemon\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Examples:\n");
    fprintf(stderr, "  %s status\n", prog);
    fprintf(stderr, "  %s events --limit 10 --severity high\n", prog);
    fprintf(stderr, "  %s reload\n", prog);
    fprintf(stderr, "  %s config log_level debug\n", prog);
    fprintf(stderr, "\n");
}

static void print_version(void) {
    printf("vnids-cli version %s\n", VNIDS_VERSION_STRING);
    printf("VNIDS - Vehicle Network Intrusion Detection System\n");
    printf("Copyright (c) 2026 VNIDS Authors\n");
}

/* Global options */
static char g_socket_path[VNIDS_MAX_PATH_LEN] = "";
static int g_json_output = 0;
static int g_quiet = 0;

const char* vnids_cli_get_socket_path(void) {
    return g_socket_path;
}

int vnids_cli_is_json_output(void) {
    return g_json_output;
}

int vnids_cli_is_quiet(void) {
    return g_quiet;
}

int main(int argc, char* argv[]) {
    static struct option long_options[] = {
        {"socket",  required_argument, NULL, 's'},
        {"json",    no_argument,       NULL, 'j'},
        {"quiet",   no_argument,       NULL, 'q'},
        {"help",    no_argument,       NULL, 'h'},
        {"version", no_argument,       NULL, 'v'},
        {NULL,      0,                 NULL, 0}
    };

    /* Default socket path */
    strncpy(g_socket_path, VNIDS_API_SOCKET, sizeof(g_socket_path) - 1);

    int opt;
    while ((opt = getopt_long(argc, argv, "s:jqhv", long_options, NULL)) != -1) {
        switch (opt) {
            case 's':
                strncpy(g_socket_path, optarg, sizeof(g_socket_path) - 1);
                break;
            case 'j':
                g_json_output = 1;
                break;
            case 'q':
                g_quiet = 1;
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            case 'v':
                print_version();
                return 0;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }

    /* Check for command */
    if (optind >= argc) {
        fprintf(stderr, "Error: No command specified\n\n");
        print_usage(argv[0]);
        return 1;
    }

    const char* command = argv[optind];
    int cmd_argc = argc - optind;
    char** cmd_argv = argv + optind;

    /* Connect to daemon */
    if (vnids_cli_client_connect(g_socket_path) < 0) {
        fprintf(stderr, "Error: Failed to connect to daemon at %s\n", g_socket_path);
        fprintf(stderr, "Is vnidsd running?\n");
        return 1;
    }

    int result = 1;

    /* Dispatch command */
    if (strcmp(command, "status") == 0) {
        result = cmd_status(cmd_argc, cmd_argv);
    } else if (strcmp(command, "stats") == 0) {
        result = cmd_stats(cmd_argc, cmd_argv);
    } else if (strcmp(command, "events") == 0) {
        result = cmd_events(cmd_argc, cmd_argv);
    } else if (strcmp(command, "rules") == 0) {
        result = cmd_rules(cmd_argc, cmd_argv);
    } else if (strcmp(command, "reload") == 0) {
        result = cmd_reload(cmd_argc, cmd_argv);
    } else if (strcmp(command, "config") == 0) {
        result = cmd_config(cmd_argc, cmd_argv);
    } else if (strcmp(command, "shutdown") == 0) {
        result = cmd_shutdown(cmd_argc, cmd_argv);
    } else {
        fprintf(stderr, "Error: Unknown command '%s'\n\n", command);
        print_usage(argv[0]);
        result = 1;
    }

    vnids_cli_client_disconnect();

    return result;
}
