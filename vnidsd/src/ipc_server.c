/**
 * VNIDS Daemon - IPC Server Implementation
 *
 * Unix domain socket server for API and control interface.
 *
 * Copyright (c) 2026 VNIDS Authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "vnids_ipc.h"
#include "vnids_log.h"

struct vnids_ipc_ctx {
    int server_fd;
    int client_fd;
    char socket_path[VNIDS_MAX_PATH_LEN];
    bool is_server;
};

vnids_ipc_ctx_t* vnids_ipc_create(void) {
    vnids_ipc_ctx_t* ctx = calloc(1, sizeof(vnids_ipc_ctx_t));
    if (ctx) {
        ctx->server_fd = -1;
        ctx->client_fd = -1;
    }
    return ctx;
}

void vnids_ipc_destroy(vnids_ipc_ctx_t* ctx) {
    if (!ctx) return;

    if (ctx->client_fd >= 0) {
        close(ctx->client_fd);
    }

    if (ctx->server_fd >= 0) {
        close(ctx->server_fd);
        if (ctx->is_server && strlen(ctx->socket_path) > 0) {
            unlink(ctx->socket_path);
        }
    }

    free(ctx);
}

int vnids_ipc_server_init(vnids_ipc_ctx_t* ctx, const char* socket_path) {
    if (!ctx || !socket_path) {
        return -1;
    }

    /* Remove existing socket */
    unlink(socket_path);

    /* Create socket */
    ctx->server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (ctx->server_fd < 0) {
        LOG_ERROR("Failed to create socket: %s", strerror(errno));
        return -1;
    }

    /* Set non-blocking */
    int flags = fcntl(ctx->server_fd, F_GETFL, 0);
    fcntl(ctx->server_fd, F_SETFL, flags | O_NONBLOCK);

    /* Bind */
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);

    if (bind(ctx->server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        LOG_ERROR("Failed to bind socket %s: %s", socket_path, strerror(errno));
        close(ctx->server_fd);
        ctx->server_fd = -1;
        return -1;
    }

    /* Set permissions */
    chmod(socket_path, VNIDS_SOCKET_PERMISSIONS);

    /* Listen */
    if (listen(ctx->server_fd, VNIDS_SOCKET_BACKLOG) < 0) {
        LOG_ERROR("Failed to listen on socket: %s", strerror(errno));
        close(ctx->server_fd);
        unlink(socket_path);
        ctx->server_fd = -1;
        return -1;
    }

    strncpy(ctx->socket_path, socket_path, VNIDS_MAX_PATH_LEN - 1);
    ctx->is_server = true;

    LOG_INFO("IPC server listening on %s", socket_path);
    return 0;
}

int vnids_ipc_server_accept(vnids_ipc_ctx_t* ctx) {
    if (!ctx || ctx->server_fd < 0) {
        return -1;
    }

    struct sockaddr_un addr;
    socklen_t addr_len = sizeof(addr);

    int client_fd = accept(ctx->server_fd, (struct sockaddr*)&addr, &addr_len);
    if (client_fd < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            LOG_ERROR("Accept failed: %s", strerror(errno));
        }
        return -1;
    }

    /* Set socket buffer size */
    int bufsize = VNIDS_SOCKET_BUFFER_SIZE;
    setsockopt(client_fd, SOL_SOCKET, SO_RCVBUF, &bufsize, sizeof(bufsize));
    setsockopt(client_fd, SOL_SOCKET, SO_SNDBUF, &bufsize, sizeof(bufsize));

    ctx->client_fd = client_fd;
    LOG_DEBUG("Accepted client connection (fd=%d)", client_fd);

    return client_fd;
}

void vnids_ipc_server_close(vnids_ipc_ctx_t* ctx) {
    if (!ctx) return;

    if (ctx->client_fd >= 0) {
        close(ctx->client_fd);
        ctx->client_fd = -1;
    }

    if (ctx->server_fd >= 0) {
        close(ctx->server_fd);
        ctx->server_fd = -1;
    }

    if (ctx->is_server && strlen(ctx->socket_path) > 0) {
        unlink(ctx->socket_path);
        ctx->socket_path[0] = '\0';
    }
}

int vnids_ipc_client_connect(vnids_ipc_ctx_t* ctx, const char* socket_path) {
    if (!ctx || !socket_path) {
        return -1;
    }

    ctx->client_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (ctx->client_fd < 0) {
        LOG_ERROR("Failed to create client socket: %s", strerror(errno));
        return -1;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);

    if (connect(ctx->client_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        LOG_ERROR("Failed to connect to %s: %s", socket_path, strerror(errno));
        close(ctx->client_fd);
        ctx->client_fd = -1;
        return -1;
    }

    /* Set socket buffer size */
    int bufsize = VNIDS_SOCKET_BUFFER_SIZE;
    setsockopt(ctx->client_fd, SOL_SOCKET, SO_RCVBUF, &bufsize, sizeof(bufsize));
    setsockopt(ctx->client_fd, SOL_SOCKET, SO_SNDBUF, &bufsize, sizeof(bufsize));

    strncpy(ctx->socket_path, socket_path, VNIDS_MAX_PATH_LEN - 1);
    ctx->is_server = false;

    LOG_DEBUG("Connected to %s", socket_path);
    return 0;
}

void vnids_ipc_client_disconnect(vnids_ipc_ctx_t* ctx) {
    if (!ctx) return;

    if (ctx->client_fd >= 0) {
        close(ctx->client_fd);
        ctx->client_fd = -1;
    }
}

int vnids_ipc_send(vnids_ipc_ctx_t* ctx, const vnids_ipc_header_t* header,
                   const void* payload) {
    if (!ctx || !header) {
        return -1;
    }

    int fd = ctx->client_fd;
    if (fd < 0) {
        return -1;
    }

    /* Send header */
    ssize_t sent = send(fd, header, sizeof(vnids_ipc_header_t), MSG_NOSIGNAL);
    if (sent != sizeof(vnids_ipc_header_t)) {
        LOG_ERROR("Failed to send header: %s", strerror(errno));
        return -1;
    }

    /* Send payload if present */
    if (payload && header->payload_len > 0) {
        sent = send(fd, payload, header->payload_len, MSG_NOSIGNAL);
        if (sent != (ssize_t)header->payload_len) {
            LOG_ERROR("Failed to send payload: %s", strerror(errno));
            return -1;
        }
    }

    return 0;
}

int vnids_ipc_recv(vnids_ipc_ctx_t* ctx, vnids_ipc_header_t* header,
                   void* payload, size_t max_len) {
    if (!ctx || !header) {
        return -1;
    }

    int fd = ctx->client_fd;
    if (fd < 0) {
        return -1;
    }

    /* Receive header */
    ssize_t received = recv(fd, header, sizeof(vnids_ipc_header_t), MSG_WAITALL);
    if (received != sizeof(vnids_ipc_header_t)) {
        if (received == 0) {
            return 0;  /* Connection closed */
        }
        LOG_ERROR("Failed to receive header: %s", strerror(errno));
        return -1;
    }

    /* Receive payload if expected */
    if (payload && header->payload_len > 0) {
        size_t to_read = header->payload_len;
        if (to_read > max_len) {
            to_read = max_len;
        }

        received = recv(fd, payload, to_read, MSG_WAITALL);
        if (received != (ssize_t)to_read) {
            LOG_ERROR("Failed to receive payload: %s", strerror(errno));
            return -1;
        }

        /* Discard remaining if buffer was too small */
        if (header->payload_len > max_len) {
            char discard[1024];
            size_t remaining = header->payload_len - max_len;
            while (remaining > 0) {
                size_t chunk = remaining > sizeof(discard) ? sizeof(discard) : remaining;
                recv(fd, discard, chunk, 0);
                remaining -= chunk;
            }
        }
    }

    return 1;
}
