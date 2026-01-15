/**
 * VNIDS CLI - Client Connection
 *
 * Handles communication with the VNIDS daemon.
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
#include <arpa/inet.h>

#include "vnids_types.h"
#include "vnids_ipc.h"

#define CLI_BUFFER_SIZE 65536
#define CLI_TIMEOUT_MS  5000

static int g_client_fd = -1;

/**
 * Connect to the daemon socket.
 */
int vnids_cli_client_connect(const char* socket_path) {
    if (!socket_path) return -1;

    /* Create socket */
    g_client_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (g_client_fd < 0) {
        return -1;
    }

    /* Set receive timeout */
    struct timeval tv;
    tv.tv_sec = CLI_TIMEOUT_MS / 1000;
    tv.tv_usec = (CLI_TIMEOUT_MS % 1000) * 1000;
    setsockopt(g_client_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(g_client_fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    /* Connect */
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);

    if (connect(g_client_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(g_client_fd);
        g_client_fd = -1;
        return -1;
    }

    return 0;
}

/**
 * Disconnect from the daemon.
 */
void vnids_cli_client_disconnect(void) {
    if (g_client_fd >= 0) {
        close(g_client_fd);
        g_client_fd = -1;
    }
}

/**
 * Send a request and receive a response.
 * Returns allocated JSON string (caller must free) or NULL on error.
 */
char* vnids_cli_client_request(const char* request) {
    if (g_client_fd < 0 || !request) {
        return NULL;
    }

    size_t request_len = strlen(request);

    /* Send length prefix */
    uint32_t net_len = htonl((uint32_t)request_len);
    if (send(g_client_fd, &net_len, sizeof(net_len), 0) != sizeof(net_len)) {
        return NULL;
    }

    /* Send request */
    if (send(g_client_fd, request, request_len, 0) != (ssize_t)request_len) {
        return NULL;
    }

    /* Receive response length */
    uint32_t resp_len;
    ssize_t received = recv(g_client_fd, &resp_len, sizeof(resp_len), MSG_WAITALL);
    if (received != sizeof(resp_len)) {
        return NULL;
    }

    resp_len = ntohl(resp_len);
    if (resp_len == 0 || resp_len > CLI_BUFFER_SIZE) {
        return NULL;
    }

    /* Receive response */
    char* response = malloc(resp_len + 1);
    if (!response) {
        return NULL;
    }

    received = recv(g_client_fd, response, resp_len, MSG_WAITALL);
    if (received != (ssize_t)resp_len) {
        free(response);
        return NULL;
    }

    response[resp_len] = '\0';

    return response;
}

/**
 * Check if client is connected.
 */
int vnids_cli_client_is_connected(void) {
    return g_client_fd >= 0;
}
