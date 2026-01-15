/**
 * VNIDS Daemon - Main Header
 *
 * Copyright (c) 2026 VNIDS Authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef VNIDSD_H
#define VNIDSD_H

#include "vnids_types.h"
#include "vnids_config.h"
#include "vnids_ipc.h"
#include "vnids_event.h"
#include "vnids_log.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Daemon context forward declaration */
typedef struct vnidsd_ctx vnidsd_ctx_t;

/* Create daemon context */
vnidsd_ctx_t* vnidsd_create(void);

/* Destroy daemon context */
void vnidsd_destroy(vnidsd_ctx_t* ctx);

/* Initialize daemon with configuration */
vnids_result_t vnidsd_init(vnidsd_ctx_t* ctx, const vnids_config_t* config);

/* Run daemon main loop (blocking) */
vnids_result_t vnidsd_run(vnidsd_ctx_t* ctx);

/* Request daemon shutdown */
void vnidsd_shutdown(vnidsd_ctx_t* ctx);

/* Check if daemon is running */
bool vnidsd_is_running(const vnidsd_ctx_t* ctx);

/* Get daemon statistics */
vnids_result_t vnidsd_get_stats(const vnidsd_ctx_t* ctx, vnids_stats_t* stats);

#ifdef __cplusplus
}
#endif

#endif /* VNIDSD_H */
