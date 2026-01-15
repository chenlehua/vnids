/**
 * VNIDS Daemon - Main Entry Point
 *
 * Copyright (c) 2026 VNIDS Authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <signal.h>
#include <unistd.h>

#include "vnidsd.h"
#include "vnids_log.h"

/* Global daemon context for signal handlers */
static vnidsd_ctx_t* g_ctx = NULL;

/* Command line options */
static struct option long_options[] = {
    {"config",  required_argument, 0, 'c'},
    {"debug",   no_argument,       0, 'd'},
    {"foreground", no_argument,    0, 'f'},
    {"help",    no_argument,       0, 'h'},
    {"version", no_argument,       0, 'v'},
    {0, 0, 0, 0}
};

static void print_usage(const char* prog) {
    printf("Usage: %s [OPTIONS]\n", prog);
    printf("\n");
    printf("VNIDS Daemon - Vehicle Network Intrusion Detection System\n");
    printf("\n");
    printf("Options:\n");
    printf("  -c, --config FILE    Configuration file path\n");
    printf("  -d, --debug          Enable debug logging\n");
    printf("  -f, --foreground     Run in foreground (don't daemonize)\n");
    printf("  -h, --help           Show this help message\n");
    printf("  -v, --version        Show version information\n");
    printf("\n");
    printf("Default config: " VNIDS_DEFAULT_SURICATA_CONFIG "\n");
}

static void print_version(void) {
    printf("vnidsd version %s\n", VNIDS_VERSION_STRING);
    printf("Protocol version: %s\n", VNIDS_PROTOCOL_VERSION);
    printf("Copyright (c) 2026 VNIDS Authors\n");
}

static void signal_handler(int signum) {
    if (g_ctx) {
        switch (signum) {
            case SIGTERM:
            case SIGINT:
                LOG_INFO("Received signal %d, shutting down...", signum);
                vnidsd_shutdown(g_ctx);
                break;
            case SIGHUP:
                LOG_INFO("Received SIGHUP, reloading configuration...");
                /* TODO: Implement config reload */
                break;
            case SIGUSR1:
                LOG_INFO("Received SIGUSR1, dumping stats...");
                /* TODO: Implement stats dump */
                break;
            default:
                break;
        }
    }
}

static void setup_signals(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGHUP, &sa, NULL);
    sigaction(SIGUSR1, &sa, NULL);

    /* Ignore SIGPIPE */
    signal(SIGPIPE, SIG_IGN);
}

int main(int argc, char* argv[]) {
    int opt;
    int option_index = 0;
    const char* config_path = "/etc/vnids/vnidsd.conf";
    bool debug_mode = false;
    bool foreground = false;
    vnids_result_t result;

    /* Parse command line arguments */
    while ((opt = getopt_long(argc, argv, "c:dfhv", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'c':
                config_path = optarg;
                break;
            case 'd':
                debug_mode = true;
                break;
            case 'f':
                foreground = true;
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

    /* Initialize logging */
    vnids_log_init("vnidsd",
                   debug_mode ? VNIDS_LOG_DEBUG : VNIDS_LOG_INFO,
                   !foreground);

    LOG_INFO("Starting vnidsd version %s", VNIDS_VERSION_STRING);

    /* Load configuration */
    vnids_config_t* config = vnids_config_create();
    if (!config) {
        LOG_FATAL("Failed to allocate configuration");
        return 1;
    }

    vnids_config_set_defaults(config);
    result = vnids_config_load(config, config_path);
    if (result != VNIDS_OK) {
        LOG_FATAL("Failed to load configuration from %s: %s",
                  config_path, vnids_result_str(result));
        vnids_config_destroy(config);
        return 1;
    }

    /* Apply environment overrides */
    vnids_config_apply_env(config);

    /* Override daemonize from command line */
    if (foreground) {
        config->general.daemonize = false;
    }

    /* Override log level from command line */
    if (debug_mode) {
        config->general.log_level = VNIDS_LOG_DEBUG;
    }

    /* Validate configuration */
    char error_msg[256];
    result = vnids_config_validate(config, error_msg, sizeof(error_msg));
    if (result != VNIDS_OK) {
        LOG_FATAL("Configuration validation failed: %s", error_msg);
        vnids_config_destroy(config);
        return 1;
    }

    /* Create daemon context */
    g_ctx = vnidsd_create();
    if (!g_ctx) {
        LOG_FATAL("Failed to create daemon context");
        vnids_config_destroy(config);
        return 1;
    }

    /* Setup signal handlers */
    setup_signals();

    /* Initialize daemon */
    result = vnidsd_init(g_ctx, config);
    if (result != VNIDS_OK) {
        LOG_FATAL("Failed to initialize daemon: %s", vnids_result_str(result));
        vnidsd_destroy(g_ctx);
        vnids_config_destroy(config);
        return 1;
    }

    /* Run daemon main loop */
    LOG_INFO("Daemon initialized, entering main loop");
    result = vnidsd_run(g_ctx);

    /* Cleanup */
    LOG_INFO("Daemon shutting down");
    vnidsd_destroy(g_ctx);
    vnids_config_destroy(config);
    vnids_log_shutdown();

    return (result == VNIDS_OK) ? 0 : 1;
}
