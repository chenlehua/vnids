/**
 * VNIDS Daemon - Core Daemon Implementation
 *
 * Copyright (c) 2026 VNIDS Authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>

#include "vnidsd.h"
#include "vnids_log.h"

/* Forward declarations */
typedef struct vnids_event_queue vnids_event_queue_t;
typedef struct vnids_storage vnids_storage_t;
typedef struct vnids_watchdog vnids_watchdog_t;

/* Daemon context */
struct vnidsd_ctx {
    vnids_config_t config;
    bool running;
    bool initialized;

    /* IPC contexts */
    vnids_ipc_ctx_t* api_server;
    vnids_ipc_ctx_t* eve_client;

    /* Event queue */
    vnids_event_queue_t* event_queue;

    /* Storage */
    vnids_storage_t* storage;

    /* Watchdog */
    vnids_watchdog_t* watchdog;
    pid_t suricata_pid;

    /* Statistics */
    vnids_stats_t stats;
    pthread_mutex_t stats_lock;

    /* Thread handles */
    pthread_t eve_reader_thread;
    pthread_t api_server_thread;
    pthread_t watchdog_thread;
    pthread_t event_processor_thread;
};

/* External functions (implemented in other files) */
extern vnids_event_queue_t* vnids_event_queue_create(size_t capacity);
extern void vnids_event_queue_destroy(vnids_event_queue_t* queue);
extern vnids_storage_t* vnids_storage_create(const char* db_path);
extern void vnids_storage_destroy(vnids_storage_t* storage);
extern vnids_watchdog_t* vnids_watchdog_create(const vnids_watchdog_config_t* config);
extern void vnids_watchdog_destroy(vnids_watchdog_t* watchdog);
extern int vnids_watchdog_start(vnids_watchdog_t* watchdog, vnidsd_ctx_t* ctx);
extern void vnids_watchdog_stop(vnids_watchdog_t* watchdog);
extern void* vnids_eve_reader_thread(void* arg);
extern void* vnids_api_server_thread(void* arg);
extern void* vnids_event_processor_thread(void* arg);
extern int vnids_pidfile_create(const char* path);
extern void vnids_pidfile_remove(const char* path);

vnidsd_ctx_t* vnidsd_create(void) {
    vnidsd_ctx_t* ctx = calloc(1, sizeof(vnidsd_ctx_t));
    if (!ctx) {
        return NULL;
    }

    pthread_mutex_init(&ctx->stats_lock, NULL);
    ctx->suricata_pid = -1;

    return ctx;
}

void vnidsd_destroy(vnidsd_ctx_t* ctx) {
    if (!ctx) {
        return;
    }

    /* Stop all threads */
    ctx->running = false;

    /* Wait for threads to finish */
    if (ctx->eve_reader_thread) {
        pthread_join(ctx->eve_reader_thread, NULL);
    }
    if (ctx->api_server_thread) {
        pthread_join(ctx->api_server_thread, NULL);
    }
    if (ctx->watchdog_thread) {
        pthread_join(ctx->watchdog_thread, NULL);
    }
    if (ctx->event_processor_thread) {
        pthread_join(ctx->event_processor_thread, NULL);
    }

    /* Cleanup resources */
    if (ctx->watchdog) {
        vnids_watchdog_stop(ctx->watchdog);
        vnids_watchdog_destroy(ctx->watchdog);
    }

    if (ctx->storage) {
        vnids_storage_destroy(ctx->storage);
    }

    if (ctx->event_queue) {
        vnids_event_queue_destroy(ctx->event_queue);
    }

    if (ctx->api_server) {
        vnids_ipc_server_close(ctx->api_server);
        vnids_ipc_destroy(ctx->api_server);
    }

    if (ctx->eve_client) {
        vnids_ipc_client_disconnect(ctx->eve_client);
        vnids_ipc_destroy(ctx->eve_client);
    }

    /* Remove PID file */
    vnids_pidfile_remove(ctx->config.general.pid_file);

    pthread_mutex_destroy(&ctx->stats_lock);
    free(ctx);
}

static int ensure_directory(const char* path) {
    struct stat st;
    if (stat(path, &st) == 0) {
        if (S_ISDIR(st.st_mode)) {
            return 0;
        }
        LOG_ERROR("%s exists but is not a directory", path);
        return -1;
    }

    if (mkdir(path, 0755) != 0) {
        LOG_ERROR("Failed to create directory %s: %s", path, strerror(errno));
        return -1;
    }

    return 0;
}

static int daemonize_process(void) {
    pid_t pid = fork();
    if (pid < 0) {
        LOG_ERROR("First fork failed: %s", strerror(errno));
        return -1;
    }
    if (pid > 0) {
        _exit(0);  /* Parent exits */
    }

    /* Create new session */
    if (setsid() < 0) {
        LOG_ERROR("setsid failed: %s", strerror(errno));
        return -1;
    }

    /* Second fork to prevent acquiring terminal */
    pid = fork();
    if (pid < 0) {
        LOG_ERROR("Second fork failed: %s", strerror(errno));
        return -1;
    }
    if (pid > 0) {
        _exit(0);  /* First child exits */
    }

    /* Set working directory */
    if (chdir("/") < 0) {
        LOG_WARN("chdir(/) failed: %s", strerror(errno));
    }

    /* Reset file mode mask */
    umask(0);

    /* Close standard file descriptors */
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    /* Redirect to /dev/null */
    int fd = open("/dev/null", O_RDWR);
    if (fd >= 0) {
        dup2(fd, STDIN_FILENO);
        dup2(fd, STDOUT_FILENO);
        dup2(fd, STDERR_FILENO);
        if (fd > 2) {
            close(fd);
        }
    }

    return 0;
}

vnids_result_t vnidsd_init(vnidsd_ctx_t* ctx, const vnids_config_t* config) {
    if (!ctx || !config) {
        return VNIDS_ERROR_INVALID;
    }

    /* Copy configuration */
    memcpy(&ctx->config, config, sizeof(vnids_config_t));

    /* Daemonize if requested */
    if (config->general.daemonize) {
        if (daemonize_process() != 0) {
            return VNIDS_ERROR;
        }
        /* Reinitialize logging for daemon */
        vnids_log_init("vnidsd", config->general.log_level, true);
    }

    /* Create socket directory */
    if (ensure_directory(config->ipc.socket_dir) != 0) {
        return VNIDS_ERROR_IO;
    }

    /* Create PID file */
    if (vnids_pidfile_create(config->general.pid_file) != 0) {
        LOG_ERROR("Failed to create PID file");
        return VNIDS_ERROR_IO;
    }

    /* Initialize event queue */
    ctx->event_queue = vnids_event_queue_create(config->ipc.event_buffer_size);
    if (!ctx->event_queue) {
        LOG_ERROR("Failed to create event queue");
        return VNIDS_ERROR_NOMEM;
    }

    /* Initialize storage */
    ctx->storage = vnids_storage_create(config->storage.database);
    if (!ctx->storage) {
        LOG_ERROR("Failed to initialize storage");
        return VNIDS_ERROR_DB;
    }

    /* Initialize API server */
    ctx->api_server = vnids_ipc_create();
    if (!ctx->api_server) {
        LOG_ERROR("Failed to create API server context");
        return VNIDS_ERROR_NOMEM;
    }

    char api_socket[VNIDS_MAX_PATH_LEN];
    snprintf(api_socket, sizeof(api_socket), "%s/api.sock", config->ipc.socket_dir);
    if (vnids_ipc_server_init(ctx->api_server, api_socket) != 0) {
        LOG_ERROR("Failed to initialize API server");
        return VNIDS_ERROR_IPC;
    }

    /* Initialize EVE client (will connect when Suricata starts) */
    ctx->eve_client = vnids_ipc_create();
    if (!ctx->eve_client) {
        LOG_ERROR("Failed to create EVE client context");
        return VNIDS_ERROR_NOMEM;
    }

    /* Initialize watchdog */
    ctx->watchdog = vnids_watchdog_create(&config->watchdog);
    if (!ctx->watchdog) {
        LOG_ERROR("Failed to create watchdog");
        return VNIDS_ERROR_NOMEM;
    }

    ctx->initialized = true;
    LOG_INFO("Daemon initialized successfully");

    return VNIDS_OK;
}

vnids_result_t vnidsd_run(vnidsd_ctx_t* ctx) {
    if (!ctx || !ctx->initialized) {
        return VNIDS_ERROR_INVALID;
    }

    ctx->running = true;

    /* Start watchdog thread (will spawn Suricata) */
    if (pthread_create(&ctx->watchdog_thread, NULL,
                       (void*(*)(void*))vnids_watchdog_start,
                       ctx->watchdog) != 0) {
        LOG_ERROR("Failed to start watchdog thread");
        return VNIDS_ERROR;
    }

    /* Start EVE reader thread */
    if (pthread_create(&ctx->eve_reader_thread, NULL,
                       vnids_eve_reader_thread, ctx) != 0) {
        LOG_ERROR("Failed to start EVE reader thread");
        return VNIDS_ERROR;
    }

    /* Start event processor thread */
    if (pthread_create(&ctx->event_processor_thread, NULL,
                       vnids_event_processor_thread, ctx) != 0) {
        LOG_ERROR("Failed to start event processor thread");
        return VNIDS_ERROR;
    }

    /* Start API server thread */
    if (pthread_create(&ctx->api_server_thread, NULL,
                       vnids_api_server_thread, ctx) != 0) {
        LOG_ERROR("Failed to start API server thread");
        return VNIDS_ERROR;
    }

    LOG_INFO("All threads started, daemon running");

    /* Wait for shutdown signal */
    while (ctx->running) {
        sleep(1);
    }

    LOG_INFO("Daemon main loop exiting");
    return VNIDS_OK;
}

void vnidsd_shutdown(vnidsd_ctx_t* ctx) {
    if (ctx) {
        ctx->running = false;
    }
}

bool vnidsd_is_running(const vnidsd_ctx_t* ctx) {
    return ctx && ctx->running;
}

vnids_result_t vnidsd_get_stats(const vnidsd_ctx_t* ctx, vnids_stats_t* stats) {
    if (!ctx || !stats) {
        return VNIDS_ERROR_INVALID;
    }

    pthread_mutex_lock((pthread_mutex_t*)&ctx->stats_lock);
    memcpy(stats, &ctx->stats, sizeof(vnids_stats_t));
    pthread_mutex_unlock((pthread_mutex_t*)&ctx->stats_lock);

    return VNIDS_OK;
}

/**
 * EVE reader thread entry point.
 * Connects to Suricata's EVE socket and reads events.
 */
void* vnids_eve_reader_thread(void* arg) {
    vnidsd_ctx_t* ctx = (vnidsd_ctx_t*)arg;
    if (!ctx || !ctx->eve_client) return NULL;

    LOG_INFO("EVE reader thread starting");

    /* Connect to EVE socket */
    if (vnids_ipc_client_connect(ctx->eve_client, VNIDS_EVENT_SOCKET) != 0) {
        LOG_ERROR("Failed to connect to EVE socket");
        return NULL;
    }

    /* Read events until shutdown */
    while (ctx->running) {
        vnids_ipc_header_t header;
        char payload[VNIDS_SOCKET_BUFFER_SIZE];

        int rc = vnids_ipc_recv(ctx->eve_client, &header, payload, sizeof(payload));
        if (rc <= 0) {
            if (!ctx->running) break;
            /* Reconnect on error */
            usleep(100000);  /* 100ms */
            continue;
        }

        /* Parse and queue event */
        vnids_security_event_t event;
        if (vnids_eve_parse(payload, &event) == 0) {
            vnids_event_queue_push(ctx->event_queue, &event);
        }
    }

    vnids_ipc_client_disconnect(ctx->eve_client);
    LOG_INFO("EVE reader thread exiting");
    return NULL;
}

/**
 * Event processor thread entry point.
 * Processes events from queue and stores them.
 */
void* vnids_event_processor_thread(void* arg) {
    vnidsd_ctx_t* ctx = (vnidsd_ctx_t*)arg;
    if (!ctx) return NULL;

    LOG_INFO("Event processor thread starting");

    while (ctx->running) {
        vnids_security_event_t event;

        if (vnids_event_queue_pop(ctx->event_queue, &event) == 0) {
            /* Store event */
            if (ctx->storage) {
                vnids_storage_insert_event(ctx->storage, &event);
            }

            /* Update statistics */
            pthread_mutex_lock(&ctx->stats_lock);
            ctx->stats.alerts_total++;
            pthread_mutex_unlock(&ctx->stats_lock);
        } else {
            /* Queue empty, wait a bit */
            usleep(10000);  /* 10ms */
        }
    }

    LOG_INFO("Event processor thread exiting");
    return NULL;
}

/**
 * API server thread entry point.
 * Handles CLI/control connections.
 */
void* vnids_api_server_thread(void* arg) {
    vnidsd_ctx_t* ctx = (vnidsd_ctx_t*)arg;
    if (!ctx || !ctx->api_server) return NULL;

    LOG_INFO("API server thread starting");

    /* Initialize API server socket */
    if (vnids_ipc_server_init(ctx->api_server, VNIDS_API_SOCKET) != 0) {
        LOG_ERROR("Failed to initialize API server");
        return NULL;
    }

    /* Accept and handle connections */
    while (ctx->running) {
        int client_fd = vnids_ipc_server_accept(ctx->api_server);
        if (client_fd < 0) {
            if (!ctx->running) break;
            continue;
        }

        /* Handle client request */
        vnids_ipc_header_t header;
        char request[VNIDS_SOCKET_BUFFER_SIZE];

        if (vnids_ipc_recv(ctx->api_server, &header, request, sizeof(request)) > 0) {
            /* Simple status response for now */
            const char* response = "{\"success\":true,\"status\":\"running\"}";
            vnids_ipc_header_t resp_header = {
                .timestamp = vnids_timestamp_now(),
                .type = VNIDS_MSG_ACK,
                .payload_len = strlen(response)
            };
            vnids_ipc_send(ctx->api_server, &resp_header, response);
        }

        close(client_fd);
    }

    vnids_ipc_server_close(ctx->api_server);
    LOG_INFO("API server thread exiting");
    return NULL;
}
