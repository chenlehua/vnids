/**
 * VNIDS Daemon - PID File Management
 *
 * Copyright (c) 2026 VNIDS Authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <sys/stat.h>

#include "vnids_log.h"

static char g_pidfile_path[256] = "";

int vnids_pidfile_create(const char* path) {
    if (!path || strlen(path) == 0) {
        return -1;
    }

    /* Check if another instance is running */
    FILE* fp = fopen(path, "r");
    if (fp) {
        pid_t pid;
        if (fscanf(fp, "%d", &pid) == 1) {
            fclose(fp);
            /* Check if process exists */
            if (kill(pid, 0) == 0 || errno != ESRCH) {
                LOG_ERROR("Another instance is running (PID %d)", pid);
                return -1;
            }
            /* Stale PID file, remove it */
            LOG_WARN("Removing stale PID file");
        } else {
            fclose(fp);
        }
    }

    /* Create new PID file */
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        LOG_ERROR("Failed to create PID file %s: %s", path, strerror(errno));
        return -1;
    }

    char buf[32];
    int len = snprintf(buf, sizeof(buf), "%d\n", getpid());

    if (write(fd, buf, len) != len) {
        LOG_ERROR("Failed to write PID file: %s", strerror(errno));
        close(fd);
        unlink(path);
        return -1;
    }

    close(fd);

    /* Save path for cleanup */
    strncpy(g_pidfile_path, path, sizeof(g_pidfile_path) - 1);

    LOG_DEBUG("Created PID file %s", path);
    return 0;
}

void vnids_pidfile_remove(const char* path) {
    const char* target = path;
    if (!target || strlen(target) == 0) {
        target = g_pidfile_path;
    }

    if (target && strlen(target) > 0) {
        if (unlink(target) == 0) {
            LOG_DEBUG("Removed PID file %s", target);
        }
    }
}
