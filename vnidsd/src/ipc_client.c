/**
 * VNIDS Daemon - IPC Client Implementation
 *
 * Client for connecting to Suricata's EVE Unix socket.
 *
 * Copyright (c) 2026 VNIDS Authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>

#include "vnids_ipc.h"
#include "vnids_log.h"

#define EVE_READ_BUFFER_SIZE 65536
#define EVE_LINE_MAX_SIZE    131072

typedef struct vnids_eve_client {
    int fd;
    char socket_path[VNIDS_MAX_PATH_LEN];
    char* buffer;
    size_t buffer_size;
    size_t buffer_used;
    bool connected;
} vnids_eve_client_t;

vnids_eve_client_t* vnids_eve_client_create(void) {
    vnids_eve_client_t* client = calloc(1, sizeof(vnids_eve_client_t));
    if (!client) {
        return NULL;
    }

    client->fd = -1;
    client->buffer = malloc(EVE_READ_BUFFER_SIZE);
    if (!client->buffer) {
        free(client);
        return NULL;
    }
    client->buffer_size = EVE_READ_BUFFER_SIZE;
    client->buffer_used = 0;

    return client;
}

void vnids_eve_client_destroy(vnids_eve_client_t* client) {
    if (!client) return;

    if (client->fd >= 0) {
        close(client->fd);
    }
    free(client->buffer);
    free(client);
}

int vnids_eve_client_connect(vnids_eve_client_t* client, const char* socket_path) {
    if (!client || !socket_path) {
        return -1;
    }

    /* Close existing connection */
    if (client->fd >= 0) {
        close(client->fd);
        client->fd = -1;
        client->connected = false;
    }

    /* Create socket */
    client->fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (client->fd < 0) {
        LOG_ERROR("Failed to create EVE client socket: %s", strerror(errno));
        return -1;
    }

    /* Set non-blocking */
    int flags = fcntl(client->fd, F_GETFL, 0);
    fcntl(client->fd, F_SETFL, flags | O_NONBLOCK);

    /* Connect */
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);

    if (connect(client->fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        if (errno != EINPROGRESS) {
            LOG_ERROR("Failed to connect to EVE socket %s: %s",
                      socket_path, strerror(errno));
            close(client->fd);
            client->fd = -1;
            return -1;
        }
    }

    strncpy(client->socket_path, socket_path, VNIDS_MAX_PATH_LEN - 1);
    client->connected = true;
    client->buffer_used = 0;

    LOG_INFO("Connected to EVE socket: %s", socket_path);
    return 0;
}

void vnids_eve_client_disconnect(vnids_eve_client_t* client) {
    if (!client) return;

    if (client->fd >= 0) {
        close(client->fd);
        client->fd = -1;
    }
    client->connected = false;
    client->buffer_used = 0;
}

bool vnids_eve_client_is_connected(vnids_eve_client_t* client) {
    return client && client->connected && client->fd >= 0;
}

int vnids_eve_client_get_fd(vnids_eve_client_t* client) {
    return client ? client->fd : -1;
}

/**
 * Read available data from the EVE socket into the internal buffer.
 * Returns number of bytes read, 0 on connection close, -1 on error.
 */
static ssize_t eve_client_fill_buffer(vnids_eve_client_t* client) {
    if (!client || client->fd < 0) {
        return -1;
    }

    /* Ensure we have room in buffer */
    if (client->buffer_used >= client->buffer_size - 1) {
        /* Buffer full, need to expand or caller needs to consume data */
        size_t new_size = client->buffer_size * 2;
        if (new_size > EVE_LINE_MAX_SIZE) {
            LOG_WARN("EVE buffer overflow, discarding data");
            client->buffer_used = 0;
            return -1;
        }
        char* new_buffer = realloc(client->buffer, new_size);
        if (!new_buffer) {
            LOG_ERROR("Failed to expand EVE buffer");
            return -1;
        }
        client->buffer = new_buffer;
        client->buffer_size = new_size;
    }

    ssize_t bytes = read(client->fd,
                         client->buffer + client->buffer_used,
                         client->buffer_size - client->buffer_used - 1);

    if (bytes < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0;  /* No data available */
        }
        LOG_ERROR("EVE read error: %s", strerror(errno));
        client->connected = false;
        return -1;
    }

    if (bytes == 0) {
        /* Connection closed */
        LOG_WARN("EVE socket connection closed");
        client->connected = false;
        return 0;
    }

    client->buffer_used += bytes;
    client->buffer[client->buffer_used] = '\0';

    return bytes;
}

/**
 * Read a complete JSON line from the EVE socket.
 * Returns pointer to the line (null-terminated, caller must NOT free),
 * or NULL if no complete line is available.
 *
 * The returned pointer is valid until the next call to this function.
 */
char* vnids_eve_client_read_line(vnids_eve_client_t* client) {
    if (!client || !client->connected) {
        return NULL;
    }

    /* Try to find a complete line in the buffer */
    char* newline = memchr(client->buffer, '\n', client->buffer_used);

    /* If no complete line, try reading more data */
    if (!newline) {
        ssize_t bytes = eve_client_fill_buffer(client);
        if (bytes <= 0) {
            return NULL;
        }
        newline = memchr(client->buffer, '\n', client->buffer_used);
    }

    if (!newline) {
        return NULL;
    }

    /* Extract the line */
    *newline = '\0';
    size_t line_len = newline - client->buffer;

    /* We'll return a pointer to a static buffer to avoid malloc overhead */
    static char line_buffer[EVE_LINE_MAX_SIZE];
    if (line_len >= sizeof(line_buffer)) {
        LOG_WARN("EVE line too long (%zu bytes), truncating", line_len);
        line_len = sizeof(line_buffer) - 1;
    }
    memcpy(line_buffer, client->buffer, line_len);
    line_buffer[line_len] = '\0';

    /* Shift remaining data */
    size_t remaining = client->buffer_used - line_len - 1;
    if (remaining > 0) {
        memmove(client->buffer, newline + 1, remaining);
    }
    client->buffer_used = remaining;

    return line_buffer;
}

/**
 * Wait for data to become available on the EVE socket.
 * Returns 1 if data available, 0 on timeout, -1 on error.
 */
int vnids_eve_client_wait(vnids_eve_client_t* client, int timeout_ms) {
    if (!client || client->fd < 0) {
        return -1;
    }

    /* Check if we already have data in buffer */
    if (client->buffer_used > 0 &&
        memchr(client->buffer, '\n', client->buffer_used)) {
        return 1;
    }

    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(client->fd, &rfds);

    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    int ret = select(client->fd + 1, &rfds, NULL, NULL,
                     timeout_ms >= 0 ? &tv : NULL);

    if (ret < 0) {
        if (errno == EINTR) {
            return 0;
        }
        LOG_ERROR("EVE select error: %s", strerror(errno));
        return -1;
    }

    return ret > 0 ? 1 : 0;
}

/**
 * Reconnect to the EVE socket if disconnected.
 * Returns 0 on success, -1 on failure.
 */
int vnids_eve_client_reconnect(vnids_eve_client_t* client) {
    if (!client || strlen(client->socket_path) == 0) {
        return -1;
    }

    if (client->connected) {
        return 0;  /* Already connected */
    }

    return vnids_eve_client_connect(client, client->socket_path);
}
