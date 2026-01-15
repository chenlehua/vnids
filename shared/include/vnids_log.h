/**
 * VNIDS Logging Utilities
 *
 * Logging macros with syslog support.
 *
 * Copyright (c) 2026 VNIDS Authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef VNIDS_LOG_H
#define VNIDS_LOG_H

#include <stdio.h>
#include <syslog.h>
#include "vnids_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Global log level (set at startup) */
extern vnids_log_level_t vnids_log_level;
extern bool vnids_log_use_syslog;

/* Initialize logging */
void vnids_log_init(const char* ident, vnids_log_level_t level, bool use_syslog);

/* Shutdown logging */
void vnids_log_shutdown(void);

/* Core logging function */
void vnids_log_write(vnids_log_level_t level, const char* file, int line, const char* fmt, ...)
    __attribute__((format(printf, 4, 5)));

/* Logging macros - undefine syslog conflicts first */
#ifdef LOG_DEBUG
#undef LOG_DEBUG
#endif
#ifdef LOG_INFO
#undef LOG_INFO
#endif

#define LOG_TRACE(...) \
    do { if (vnids_log_level <= VNIDS_LOG_TRACE) \
        vnids_log_write(VNIDS_LOG_TRACE, __FILE__, __LINE__, __VA_ARGS__); } while(0)

#define LOG_DEBUG(...) \
    do { if (vnids_log_level <= VNIDS_LOG_DEBUG) \
        vnids_log_write(VNIDS_LOG_DEBUG, __FILE__, __LINE__, __VA_ARGS__); } while(0)

#define LOG_INFO(...) \
    do { if (vnids_log_level <= VNIDS_LOG_INFO) \
        vnids_log_write(VNIDS_LOG_INFO, __FILE__, __LINE__, __VA_ARGS__); } while(0)

#define LOG_WARN(...) \
    do { if (vnids_log_level <= VNIDS_LOG_WARN) \
        vnids_log_write(VNIDS_LOG_WARN, __FILE__, __LINE__, __VA_ARGS__); } while(0)

#define LOG_ERROR(...) \
    do { if (vnids_log_level <= VNIDS_LOG_ERROR) \
        vnids_log_write(VNIDS_LOG_ERROR, __FILE__, __LINE__, __VA_ARGS__); } while(0)

#define LOG_FATAL(...) \
    do { vnids_log_write(VNIDS_LOG_FATAL, __FILE__, __LINE__, __VA_ARGS__); } while(0)

/* Map VNIDS log levels to syslog priorities */
/* Use explicit values to avoid macro conflicts */
static inline int vnids_log_to_syslog(vnids_log_level_t level) {
    switch (level) {
        case VNIDS_LOG_TRACE:  return 7;  /* LOG_DEBUG */
        case VNIDS_LOG_DEBUG:  return 7;  /* LOG_DEBUG */
        case VNIDS_LOG_INFO:   return 6;  /* LOG_INFO */
        case VNIDS_LOG_WARN:   return 4;  /* LOG_WARNING */
        case VNIDS_LOG_ERROR:  return 3;  /* LOG_ERR */
        case VNIDS_LOG_FATAL:  return 2;  /* LOG_CRIT */
        default:               return 6;  /* LOG_INFO */
    }
}

#ifdef __cplusplus
}
#endif

#endif /* VNIDS_LOG_H */
