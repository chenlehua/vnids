/**
 * VNIDS Daemon - API Server
 *
 * Handles incoming API connections and dispatches commands.
 *
 * Copyright (c) 2026 VNIDS Authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/epoll.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <fcntl.h>

#include "vnids_ipc.h"
#include "vnids_types.h"
#include "vnids_log.h"

#define API_MAX_CLIENTS 32
#define API_BUFFER_SIZE 65536

/* Forward declarations */
struct vnidsd_ctx;
typedef struct vnidsd_ctx vnidsd_ctx_t;
typedef struct vnids_control_ctx vnids_control_ctx_t;

extern vnids_control_ctx_t* vnids_control_create(vnidsd_ctx_t* daemon_ctx);
extern void vnids_control_destroy(vnids_control_ctx_t* ctx);
extern char* vnids_control_process(vnids_control_ctx_t* ctx,
                                    vnids_command_t cmd,
                                    const char* params);
extern int vnids_request_from_json(const char* json, vnids_command_t* cmd,
                                    char* params, size_t params_len);

/**
 * Client connection.
 */
typedef struct {
    int fd;
    char recv_buffer[API_BUFFER_SIZE];
    size_t recv_used;
    bool active;
} api_client_t;

/**
 * API server context.
 */
typedef struct vnids_api_server {
    int server_fd;
    int epoll_fd;
    char socket_path[VNIDS_MAX_PATH_LEN];

    api_client_t clients[API_MAX_CLIENTS];
    int client_count;

    vnids_control_ctx_t* control_ctx;

    pthread_t thread;
    pthread_mutex_t mutex;
    bool running;
    bool thread_started;

    /* Statistics */
    uint64_t connections_accepted;
    uint64_t requests_processed;
    uint64_t errors;
} vnids_api_server_t;

/* Forward declaration */
void vnids_api_server_stop(vnids_api_server_t* server);

/**
 * Create API server.
 */
vnids_api_server_t* vnids_api_server_create(void) {
    vnids_api_server_t* server = calloc(1, sizeof(vnids_api_server_t));
    if (!server) return NULL;

    server->server_fd = -1;
    server->epoll_fd = -1;

    pthread_mutex_init(&server->mutex, NULL);

    /* Initialize client slots */
    for (int i = 0; i < API_MAX_CLIENTS; i++) {
        server->clients[i].fd = -1;
        server->clients[i].active = false;
    }

    return server;
}

/**
 * Destroy API server.
 */
void vnids_api_server_destroy(vnids_api_server_t* server) {
    if (!server) return;

    vnids_api_server_stop(server);

    if (server->control_ctx) {
        vnids_control_destroy(server->control_ctx);
    }

    pthread_mutex_destroy(&server->mutex);
    free(server);
}

/**
 * Find an empty client slot.
 */
static api_client_t* find_free_slot(vnids_api_server_t* server) {
    for (int i = 0; i < API_MAX_CLIENTS; i++) {
        if (!server->clients[i].active) {
            return &server->clients[i];
        }
    }
    return NULL;
}

/**
 * Find client by fd.
 */
static api_client_t* find_client(vnids_api_server_t* server, int fd) {
    for (int i = 0; i < API_MAX_CLIENTS; i++) {
        if (server->clients[i].active && server->clients[i].fd == fd) {
            return &server->clients[i];
        }
    }
    return NULL;
}

/**
 * Accept a new client connection.
 */
static void accept_client(vnids_api_server_t* server) {
    struct sockaddr_un addr;
    socklen_t addr_len = sizeof(addr);

    int client_fd = accept(server->server_fd, (struct sockaddr*)&addr, &addr_len);
    if (client_fd < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            LOG_ERROR("API accept failed: %s", strerror(errno));
        }
        return;
    }

    /* Set non-blocking */
    int flags = fcntl(client_fd, F_GETFL, 0);
    fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);

    /* Find slot */
    api_client_t* client = find_free_slot(server);
    if (!client) {
        LOG_WARN("API max clients reached, rejecting connection");
        close(client_fd);
        return;
    }

    /* Add to epoll */
    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = client_fd;

    if (epoll_ctl(server->epoll_fd, EPOLL_CTL_ADD, client_fd, &ev) < 0) {
        LOG_ERROR("API epoll_ctl ADD failed: %s", strerror(errno));
        close(client_fd);
        return;
    }

    /* Initialize client */
    client->fd = client_fd;
    client->recv_used = 0;
    client->active = true;
    server->client_count++;
    server->connections_accepted++;

    LOG_DEBUG("API client connected (fd=%d)", client_fd);
}

/**
 * Close a client connection.
 */
static void close_client(vnids_api_server_t* server, api_client_t* client) {
    if (!client || !client->active) return;

    epoll_ctl(server->epoll_fd, EPOLL_CTL_DEL, client->fd, NULL);
    close(client->fd);

    client->fd = -1;
    client->active = false;
    client->recv_used = 0;
    server->client_count--;

    LOG_DEBUG("API client disconnected");
}

/**
 * Send response to client.
 */
static int send_response(api_client_t* client, const char* json) {
    if (!client || !json) return -1;

    size_t len = strlen(json);

    /* Send length prefix (4 bytes) + JSON + newline */
    uint32_t net_len = htonl((uint32_t)len);

    if (send(client->fd, &net_len, sizeof(net_len), MSG_NOSIGNAL) != sizeof(net_len)) {
        return -1;
    }

    if (send(client->fd, json, len, MSG_NOSIGNAL) != (ssize_t)len) {
        return -1;
    }

    return 0;
}

/**
 * Process a complete request.
 */
static void process_request(vnids_api_server_t* server,
                             api_client_t* client,
                             const char* request) {
    LOG_DEBUG("API request: %s", request);

    vnids_command_t cmd;
    char params[4096];

    if (vnids_request_from_json(request, &cmd, params, sizeof(params)) < 0) {
        const char* error = "{\"success\":false,\"error\":\"Invalid request\"}";
        send_response(client, error);
        server->errors++;
        return;
    }

    char* response = vnids_control_process(server->control_ctx, cmd, params);
    if (response) {
        send_response(client, response);
        free(response);
        server->requests_processed++;
    } else {
        const char* error = "{\"success\":false,\"error\":\"Internal error\"}";
        send_response(client, error);
        server->errors++;
    }
}

/**
 * Handle client data.
 */
static void handle_client_data(vnids_api_server_t* server, api_client_t* client) {
    while (true) {
        /* Read available data */
        ssize_t bytes = recv(client->fd,
                             client->recv_buffer + client->recv_used,
                             sizeof(client->recv_buffer) - client->recv_used - 1,
                             0);

        if (bytes < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;  /* No more data */
            }
            LOG_ERROR("API recv error: %s", strerror(errno));
            close_client(server, client);
            return;
        }

        if (bytes == 0) {
            /* Connection closed */
            close_client(server, client);
            return;
        }

        client->recv_used += bytes;
        client->recv_buffer[client->recv_used] = '\0';

        /* Process complete messages (length-prefixed) */
        while (client->recv_used >= sizeof(uint32_t)) {
            uint32_t msg_len;
            memcpy(&msg_len, client->recv_buffer, sizeof(msg_len));
            msg_len = ntohl(msg_len);

            if (msg_len > API_BUFFER_SIZE - sizeof(uint32_t)) {
                LOG_ERROR("API message too large: %u", msg_len);
                close_client(server, client);
                return;
            }

            size_t total_len = sizeof(uint32_t) + msg_len;
            if (client->recv_used < total_len) {
                break;  /* Incomplete message */
            }

            /* Extract and process message */
            char* msg = client->recv_buffer + sizeof(uint32_t);
            char saved = msg[msg_len];
            msg[msg_len] = '\0';

            process_request(server, client, msg);

            msg[msg_len] = saved;

            /* Shift remaining data */
            size_t remaining = client->recv_used - total_len;
            if (remaining > 0) {
                memmove(client->recv_buffer, client->recv_buffer + total_len, remaining);
            }
            client->recv_used = remaining;
        }
    }
}

/**
 * API server thread function.
 */
static void* api_server_thread(void* arg) {
    vnids_api_server_t* server = (vnids_api_server_t*)arg;

    LOG_INFO("API server thread started on %s", server->socket_path);

    struct epoll_event events[API_MAX_CLIENTS + 1];

    while (server->running) {
        int nfds = epoll_wait(server->epoll_fd, events, API_MAX_CLIENTS + 1, 100);

        if (nfds < 0) {
            if (errno == EINTR) continue;
            LOG_ERROR("API epoll_wait error: %s", strerror(errno));
            break;
        }

        for (int i = 0; i < nfds && server->running; i++) {
            if (events[i].data.fd == server->server_fd) {
                /* New connection */
                accept_client(server);
            } else {
                /* Client data */
                api_client_t* client = find_client(server, events[i].data.fd);
                if (client) {
                    if (events[i].events & (EPOLLERR | EPOLLHUP)) {
                        close_client(server, client);
                    } else if (events[i].events & EPOLLIN) {
                        handle_client_data(server, client);
                    }
                }
            }
        }
    }

    /* Close all clients */
    for (int i = 0; i < API_MAX_CLIENTS; i++) {
        if (server->clients[i].active) {
            close_client(server, &server->clients[i]);
        }
    }

    LOG_INFO("API server thread stopped");
    return NULL;
}

/**
 * Initialize and start the API server.
 */
int vnids_api_server_start(vnids_api_server_t* server,
                            const char* socket_path,
                            vnidsd_ctx_t* daemon_ctx) {
    if (!server || !socket_path) return -1;

    pthread_mutex_lock(&server->mutex);

    if (server->thread_started) {
        pthread_mutex_unlock(&server->mutex);
        return -1;
    }

    strncpy(server->socket_path, socket_path, VNIDS_MAX_PATH_LEN - 1);

    /* Create control context */
    server->control_ctx = vnids_control_create(daemon_ctx);
    if (!server->control_ctx) {
        LOG_ERROR("Failed to create control context");
        pthread_mutex_unlock(&server->mutex);
        return -1;
    }

    /* Remove existing socket */
    unlink(socket_path);

    /* Create server socket */
    server->server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server->server_fd < 0) {
        LOG_ERROR("API socket creation failed: %s", strerror(errno));
        pthread_mutex_unlock(&server->mutex);
        return -1;
    }

    /* Set non-blocking */
    int flags = fcntl(server->server_fd, F_GETFL, 0);
    fcntl(server->server_fd, F_SETFL, flags | O_NONBLOCK);

    /* Bind */
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);

    if (bind(server->server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        LOG_ERROR("API bind failed: %s", strerror(errno));
        close(server->server_fd);
        server->server_fd = -1;
        pthread_mutex_unlock(&server->mutex);
        return -1;
    }

    /* Set permissions */
    chmod(socket_path, VNIDS_SOCKET_PERMISSIONS);

    /* Listen */
    if (listen(server->server_fd, VNIDS_SOCKET_BACKLOG) < 0) {
        LOG_ERROR("API listen failed: %s", strerror(errno));
        close(server->server_fd);
        unlink(socket_path);
        server->server_fd = -1;
        pthread_mutex_unlock(&server->mutex);
        return -1;
    }

    /* Create epoll */
    server->epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    if (server->epoll_fd < 0) {
        LOG_ERROR("API epoll_create failed: %s", strerror(errno));
        close(server->server_fd);
        unlink(socket_path);
        server->server_fd = -1;
        pthread_mutex_unlock(&server->mutex);
        return -1;
    }

    /* Add server socket to epoll */
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = server->server_fd;
    epoll_ctl(server->epoll_fd, EPOLL_CTL_ADD, server->server_fd, &ev);

    /* Start thread */
    server->running = true;

    int ret = pthread_create(&server->thread, NULL, api_server_thread, server);
    if (ret != 0) {
        LOG_ERROR("API thread creation failed: %s", strerror(ret));
        close(server->epoll_fd);
        close(server->server_fd);
        unlink(socket_path);
        server->epoll_fd = -1;
        server->server_fd = -1;
        server->running = false;
        pthread_mutex_unlock(&server->mutex);
        return -1;
    }

    server->thread_started = true;

    pthread_mutex_unlock(&server->mutex);
    return 0;
}

/**
 * Stop the API server.
 */
void vnids_api_server_stop(vnids_api_server_t* server) {
    if (!server) return;

    pthread_mutex_lock(&server->mutex);

    if (!server->thread_started) {
        pthread_mutex_unlock(&server->mutex);
        return;
    }

    server->running = false;

    pthread_mutex_unlock(&server->mutex);

    pthread_join(server->thread, NULL);

    pthread_mutex_lock(&server->mutex);

    if (server->epoll_fd >= 0) {
        close(server->epoll_fd);
        server->epoll_fd = -1;
    }

    if (server->server_fd >= 0) {
        close(server->server_fd);
        server->server_fd = -1;
    }

    if (strlen(server->socket_path) > 0) {
        unlink(server->socket_path);
    }

    server->thread_started = false;

    pthread_mutex_unlock(&server->mutex);

    LOG_INFO("API server stopped");
}

/**
 * Get API server statistics.
 */
void vnids_api_server_get_stats(vnids_api_server_t* server,
                                 uint64_t* connections,
                                 uint64_t* requests,
                                 uint64_t* errors) {
    if (!server) return;

    pthread_mutex_lock(&server->mutex);
    if (connections) *connections = server->connections_accepted;
    if (requests) *requests = server->requests_processed;
    if (errors) *errors = server->errors;
    pthread_mutex_unlock(&server->mutex);
}
