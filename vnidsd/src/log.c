/**
 * VNIDS Daemon - Logging Implementation
 *
 * Copyright (c) 2026 VNIDS Authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <syslog.h>

#include "vnids_log.h"

/* Global log state */
vnids_log_level_t vnids_log_level = VNIDS_LOG_INFO;
bool vnids_log_use_syslog = false;

static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;
static const char* log_ident = "vnids";

void vnids_log_init(const char* ident, vnids_log_level_t level, bool use_syslog) {
    pthread_mutex_lock(&log_mutex);

    log_ident = ident ? ident : "vnids";
    vnids_log_level = level;
    vnids_log_use_syslog = use_syslog;

    if (use_syslog) {
        openlog(log_ident, LOG_PID | LOG_NDELAY, LOG_DAEMON);
    }

    pthread_mutex_unlock(&log_mutex);
}

void vnids_log_shutdown(void) {
    pthread_mutex_lock(&log_mutex);

    if (vnids_log_use_syslog) {
        closelog();
    }

    pthread_mutex_unlock(&log_mutex);
}

static const char* level_prefix(vnids_log_level_t level) {
    switch (level) {
        case VNIDS_LOG_TRACE: return "TRACE";
        case VNIDS_LOG_DEBUG: return "DEBUG";
        case VNIDS_LOG_INFO:  return "INFO ";
        case VNIDS_LOG_WARN:  return "WARN ";
        case VNIDS_LOG_ERROR: return "ERROR";
        case VNIDS_LOG_FATAL: return "FATAL";
        default: return "?????";
    }
}

void vnids_log_write(vnids_log_level_t level, const char* file,
                     int line, const char* fmt, ...) {
    va_list args;
    char message[1024];
    char timestamp[32];
    time_t now;
    struct tm tm_info;

    /* Format message */
    va_start(args, fmt);
    vsnprintf(message, sizeof(message), fmt, args);
    va_end(args);

    pthread_mutex_lock(&log_mutex);

    if (vnids_log_use_syslog) {
        /* Log to syslog */
        syslog(vnids_log_to_syslog(level), "[%s:%d] %s", file, line, message);
    } else {
        /* Log to stderr with timestamp */
        now = time(NULL);
        localtime_r(&now, &tm_info);
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &tm_info);

        fprintf(stderr, "%s [%s] [%s:%d] %s\n",
                timestamp, level_prefix(level), file, line, message);
        fflush(stderr);
    }

    pthread_mutex_unlock(&log_mutex);
}
