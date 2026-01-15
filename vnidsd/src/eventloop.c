/**
 * VNIDS Daemon - Event Loop (epoll-based)
 *
 * Copyright (c) 2026 VNIDS Authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/epoll.h>

#include "vnids_types.h"
#include "vnids_log.h"

#define MAX_EVENTS 64

typedef void (*vnids_event_callback_t)(int fd, uint32_t events, void* user_data);

typedef struct {
    int fd;
    vnids_event_callback_t callback;
    void* user_data;
} vnids_event_handler_t;

typedef struct vnids_eventloop {
    int epoll_fd;
    vnids_event_handler_t handlers[MAX_EVENTS];
    int handler_count;
    bool running;
} vnids_eventloop_t;

vnids_eventloop_t* vnids_eventloop_create(void) {
    vnids_eventloop_t* loop = calloc(1, sizeof(vnids_eventloop_t));
    if (!loop) {
        return NULL;
    }

    loop->epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    if (loop->epoll_fd < 0) {
        LOG_ERROR("Failed to create epoll fd: %s", strerror(errno));
        free(loop);
        return NULL;
    }

    return loop;
}

void vnids_eventloop_destroy(vnids_eventloop_t* loop) {
    if (!loop) return;

    if (loop->epoll_fd >= 0) {
        close(loop->epoll_fd);
    }
    free(loop);
}

int vnids_eventloop_add(vnids_eventloop_t* loop, int fd, uint32_t events,
                        vnids_event_callback_t callback, void* user_data) {
    if (!loop || fd < 0 || loop->handler_count >= MAX_EVENTS) {
        return -1;
    }

    struct epoll_event ev;
    ev.events = events;
    ev.data.fd = fd;

    if (epoll_ctl(loop->epoll_fd, EPOLL_CTL_ADD, fd, &ev) < 0) {
        LOG_ERROR("epoll_ctl ADD failed: %s", strerror(errno));
        return -1;
    }

    /* Store handler */
    loop->handlers[loop->handler_count].fd = fd;
    loop->handlers[loop->handler_count].callback = callback;
    loop->handlers[loop->handler_count].user_data = user_data;
    loop->handler_count++;

    return 0;
}

int vnids_eventloop_remove(vnids_eventloop_t* loop, int fd) {
    if (!loop || fd < 0) {
        return -1;
    }

    if (epoll_ctl(loop->epoll_fd, EPOLL_CTL_DEL, fd, NULL) < 0) {
        LOG_WARN("epoll_ctl DEL failed: %s", strerror(errno));
    }

    /* Remove handler */
    for (int i = 0; i < loop->handler_count; i++) {
        if (loop->handlers[i].fd == fd) {
            /* Shift remaining handlers */
            memmove(&loop->handlers[i], &loop->handlers[i + 1],
                    (loop->handler_count - i - 1) * sizeof(vnids_event_handler_t));
            loop->handler_count--;
            break;
        }
    }

    return 0;
}

static vnids_event_handler_t* find_handler(vnids_eventloop_t* loop, int fd) {
    for (int i = 0; i < loop->handler_count; i++) {
        if (loop->handlers[i].fd == fd) {
            return &loop->handlers[i];
        }
    }
    return NULL;
}

int vnids_eventloop_run(vnids_eventloop_t* loop, int timeout_ms) {
    if (!loop) return -1;

    struct epoll_event events[MAX_EVENTS];
    loop->running = true;

    while (loop->running) {
        int nfds = epoll_wait(loop->epoll_fd, events, MAX_EVENTS, timeout_ms);

        if (nfds < 0) {
            if (errno == EINTR) {
                continue;
            }
            LOG_ERROR("epoll_wait failed: %s", strerror(errno));
            return -1;
        }

        for (int i = 0; i < nfds; i++) {
            vnids_event_handler_t* handler = find_handler(loop, events[i].data.fd);
            if (handler && handler->callback) {
                handler->callback(events[i].data.fd, events[i].events,
                                  handler->user_data);
            }
        }
    }

    return 0;
}

void vnids_eventloop_stop(vnids_eventloop_t* loop) {
    if (loop) {
        loop->running = false;
    }
}
