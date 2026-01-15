/**
 * VNIDS Daemon - Suricata Watchdog
 *
 * Monitors and supervises the Suricata subprocess.
 *
 * Copyright (c) 2026 VNIDS Authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>

#include "vnids_types.h"
#include "vnids_config.h"
#include "vnids_log.h"

#define WATCHDOG_DEFAULT_INTERVAL_MS 5000
#define WATCHDOG_MAX_RESTART_ATTEMPTS 5
#define WATCHDOG_RESTART_BACKOFF_MS 1000
#define SURICATA_STARTUP_TIMEOUT_S 30

/**
 * Watchdog state.
 */
typedef enum {
    WATCHDOG_STATE_STOPPED,
    WATCHDOG_STATE_STARTING,
    WATCHDOG_STATE_RUNNING,
    WATCHDOG_STATE_RESTARTING,
    WATCHDOG_STATE_FAILED
} vnids_watchdog_state_t;

/**
 * Watchdog context.
 */
typedef struct vnids_watchdog {
    /* Suricata process */
    pid_t suricata_pid;
    char suricata_binary[VNIDS_MAX_PATH_LEN];
    char suricata_config[VNIDS_MAX_PATH_LEN];
    char eve_socket[VNIDS_MAX_PATH_LEN];
    char rules_dir[VNIDS_MAX_PATH_LEN];
    char log_dir[VNIDS_MAX_PATH_LEN];
    char* interfaces[16];
    int interface_count;

    /* State */
    vnids_watchdog_state_t state;
    int restart_count;
    time_t last_start_time;
    time_t last_stop_time;

    /* Thread */
    pthread_t thread;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    bool running;
    bool thread_started;

    /* Configuration */
    int check_interval_ms;
    int max_restart_attempts;
    bool auto_restart;
} vnids_watchdog_t;

/* Forward declaration */
void vnids_watchdog_stop(vnids_watchdog_t* wd);

/**
 * Create watchdog.
 */
vnids_watchdog_t* vnids_watchdog_create(void) {
    vnids_watchdog_t* wd = calloc(1, sizeof(vnids_watchdog_t));
    if (!wd) return NULL;

    wd->suricata_pid = -1;
    wd->state = WATCHDOG_STATE_STOPPED;

    pthread_mutex_init(&wd->mutex, NULL);
    pthread_cond_init(&wd->cond, NULL);

    wd->check_interval_ms = WATCHDOG_DEFAULT_INTERVAL_MS;
    wd->max_restart_attempts = WATCHDOG_MAX_RESTART_ATTEMPTS;
    wd->auto_restart = true;

    return wd;
}

/**
 * Destroy watchdog.
 */
void vnids_watchdog_destroy(vnids_watchdog_t* wd) {
    if (!wd) return;

    vnids_watchdog_stop(wd);

    /* Free interface strings */
    for (int i = 0; i < wd->interface_count; i++) {
        free(wd->interfaces[i]);
    }

    pthread_mutex_destroy(&wd->mutex);
    pthread_cond_destroy(&wd->cond);

    free(wd);
}

/**
 * Configure watchdog with Suricata settings.
 */
int vnids_watchdog_configure(vnids_watchdog_t* wd,
                              const char* binary,
                              const char* config,
                              const char* eve_socket,
                              const char* rules_dir,
                              const char* log_dir) {
    if (!wd) return -1;

    pthread_mutex_lock(&wd->mutex);

    if (binary) {
        strncpy(wd->suricata_binary, binary, VNIDS_MAX_PATH_LEN - 1);
    }
    if (config) {
        strncpy(wd->suricata_config, config, VNIDS_MAX_PATH_LEN - 1);
    }
    if (eve_socket) {
        strncpy(wd->eve_socket, eve_socket, VNIDS_MAX_PATH_LEN - 1);
    }
    if (rules_dir) {
        strncpy(wd->rules_dir, rules_dir, VNIDS_MAX_PATH_LEN - 1);
    }
    if (log_dir) {
        strncpy(wd->log_dir, log_dir, VNIDS_MAX_PATH_LEN - 1);
    }

    pthread_mutex_unlock(&wd->mutex);
    return 0;
}

/**
 * Add a network interface to monitor.
 */
int vnids_watchdog_add_interface(vnids_watchdog_t* wd, const char* iface) {
    if (!wd || !iface) return -1;

    pthread_mutex_lock(&wd->mutex);

    if (wd->interface_count >= 16) {
        pthread_mutex_unlock(&wd->mutex);
        return -1;
    }

    wd->interfaces[wd->interface_count] = strdup(iface);
    if (!wd->interfaces[wd->interface_count]) {
        pthread_mutex_unlock(&wd->mutex);
        return -1;
    }
    wd->interface_count++;

    pthread_mutex_unlock(&wd->mutex);
    return 0;
}

/**
 * Build Suricata command line arguments.
 */
static char** build_suricata_args(vnids_watchdog_t* wd) {
    /* Calculate number of arguments */
    int argc = 1;  /* binary name */
    argc += 2;     /* -c config */
    argc += 2;     /* --unix-socket eve_socket */

    if (strlen(wd->rules_dir) > 0) {
        argc += 2;  /* -S rules_dir */
    }
    if (strlen(wd->log_dir) > 0) {
        argc += 2;  /* -l log_dir */
    }

    for (int i = 0; i < wd->interface_count; i++) {
        argc += 2;  /* -i interface */
    }

    argc += 1;  /* --runmode workers */
    argc += 1;  /* NULL terminator */

    char** args = calloc(argc + 5, sizeof(char*));
    if (!args) return NULL;

    int idx = 0;
    args[idx++] = strdup(wd->suricata_binary);
    args[idx++] = strdup("-c");
    args[idx++] = strdup(wd->suricata_config);
    args[idx++] = strdup("--unix-socket");
    args[idx++] = strdup(wd->eve_socket);

    if (strlen(wd->rules_dir) > 0) {
        args[idx++] = strdup("-S");
        args[idx++] = strdup(wd->rules_dir);
    }

    if (strlen(wd->log_dir) > 0) {
        args[idx++] = strdup("-l");
        args[idx++] = strdup(wd->log_dir);
    }

    for (int i = 0; i < wd->interface_count; i++) {
        args[idx++] = strdup("-i");
        args[idx++] = strdup(wd->interfaces[i]);
    }

    args[idx++] = strdup("--runmode");
    args[idx++] = strdup("workers");

    args[idx] = NULL;

    return args;
}

/**
 * Free argument list.
 */
static void free_args(char** args) {
    if (!args) return;
    for (int i = 0; args[i]; i++) {
        free(args[i]);
    }
    free(args);
}

/**
 * Start Suricata process.
 */
static int start_suricata(vnids_watchdog_t* wd) {
    LOG_INFO("Starting Suricata: %s", wd->suricata_binary);

    /* Verify binary exists */
    if (access(wd->suricata_binary, X_OK) != 0) {
        LOG_ERROR("Suricata binary not found or not executable: %s", wd->suricata_binary);
        return -1;
    }

    /* Build argument list */
    char** args = build_suricata_args(wd);
    if (!args) {
        LOG_ERROR("Failed to build Suricata arguments");
        return -1;
    }

    /* Log command */
    char cmd_log[2048] = "";
    for (int i = 0; args[i]; i++) {
        strncat(cmd_log, args[i], sizeof(cmd_log) - strlen(cmd_log) - 2);
        strncat(cmd_log, " ", sizeof(cmd_log) - strlen(cmd_log) - 1);
    }
    LOG_DEBUG("Suricata command: %s", cmd_log);

    /* Fork and exec */
    pid_t pid = fork();

    if (pid < 0) {
        LOG_ERROR("Fork failed: %s", strerror(errno));
        free_args(args);
        return -1;
    }

    if (pid == 0) {
        /* Child process */

        /* Redirect stdout/stderr to log file if configured */
        if (strlen(wd->log_dir) > 0) {
            char stdout_path[VNIDS_MAX_PATH_LEN];
            snprintf(stdout_path, sizeof(stdout_path), "%s/suricata.log", wd->log_dir);

            int fd = open(stdout_path, O_WRONLY | O_CREAT | O_APPEND, 0644);
            if (fd >= 0) {
                dup2(fd, STDOUT_FILENO);
                dup2(fd, STDERR_FILENO);
                close(fd);
            }
        }

        /* Execute Suricata */
        execv(wd->suricata_binary, args);

        /* If we get here, exec failed */
        _exit(127);
    }

    /* Parent process */
    free_args(args);

    wd->suricata_pid = pid;
    wd->last_start_time = time(NULL);

    LOG_INFO("Suricata started with PID %d", pid);

    return 0;
}

/**
 * Stop Suricata process.
 */
static void stop_suricata(vnids_watchdog_t* wd) {
    if (wd->suricata_pid <= 0) return;

    LOG_INFO("Stopping Suricata (PID %d)", wd->suricata_pid);

    /* Send SIGTERM first */
    if (kill(wd->suricata_pid, SIGTERM) == 0) {
        /* Wait for graceful shutdown */
        int status;
        int waited = 0;
        while (waited < 10) {  /* 10 seconds timeout */
            pid_t result = waitpid(wd->suricata_pid, &status, WNOHANG);
            if (result == wd->suricata_pid) {
                LOG_INFO("Suricata stopped gracefully");
                wd->suricata_pid = -1;
                wd->last_stop_time = time(NULL);
                return;
            }
            if (result < 0) {
                break;
            }
            sleep(1);
            waited++;
        }

        /* Force kill if still running */
        LOG_WARN("Suricata did not stop gracefully, sending SIGKILL");
        kill(wd->suricata_pid, SIGKILL);
        waitpid(wd->suricata_pid, &status, 0);
    }

    wd->suricata_pid = -1;
    wd->last_stop_time = time(NULL);
}

/**
 * Check if Suricata is running.
 */
static bool is_suricata_running(vnids_watchdog_t* wd) {
    if (wd->suricata_pid <= 0) return false;

    /* Check if process exists */
    if (kill(wd->suricata_pid, 0) == 0) {
        return true;
    }

    /* Process doesn't exist, reap zombie if any */
    int status;
    waitpid(wd->suricata_pid, &status, WNOHANG);
    wd->suricata_pid = -1;

    return false;
}

/**
 * Watchdog thread function.
 */
static void* watchdog_thread(void* arg) {
    vnids_watchdog_t* wd = (vnids_watchdog_t*)arg;

    LOG_INFO("Watchdog thread started");

    pthread_mutex_lock(&wd->mutex);

    /* Initial start */
    wd->state = WATCHDOG_STATE_STARTING;

    if (start_suricata(wd) == 0) {
        wd->state = WATCHDOG_STATE_RUNNING;
    } else {
        wd->state = WATCHDOG_STATE_FAILED;
    }

    while (wd->running) {
        /* Wait for interval or signal */
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += wd->check_interval_ms / 1000;
        ts.tv_nsec += (wd->check_interval_ms % 1000) * 1000000;
        if (ts.tv_nsec >= 1000000000) {
            ts.tv_sec++;
            ts.tv_nsec -= 1000000000;
        }

        pthread_cond_timedwait(&wd->cond, &wd->mutex, &ts);

        if (!wd->running) break;

        /* Check Suricata status */
        if (!is_suricata_running(wd)) {
            if (wd->state == WATCHDOG_STATE_RUNNING) {
                LOG_WARN("Suricata process died unexpectedly");
                wd->state = WATCHDOG_STATE_STOPPED;
            }

            /* Attempt restart if enabled */
            if (wd->auto_restart && wd->restart_count < wd->max_restart_attempts) {
                wd->state = WATCHDOG_STATE_RESTARTING;
                wd->restart_count++;

                /* Exponential backoff */
                int backoff_ms = WATCHDOG_RESTART_BACKOFF_MS * (1 << (wd->restart_count - 1));
                if (backoff_ms > 60000) backoff_ms = 60000;  /* Max 60 seconds */

                LOG_INFO("Restarting Suricata (attempt %d/%d) after %dms",
                         wd->restart_count, wd->max_restart_attempts, backoff_ms);

                pthread_mutex_unlock(&wd->mutex);
                usleep(backoff_ms * 1000);
                pthread_mutex_lock(&wd->mutex);

                if (!wd->running) break;

                if (start_suricata(wd) == 0) {
                    wd->state = WATCHDOG_STATE_RUNNING;
                    LOG_INFO("Suricata restarted successfully");
                } else {
                    LOG_ERROR("Failed to restart Suricata");
                    if (wd->restart_count >= wd->max_restart_attempts) {
                        wd->state = WATCHDOG_STATE_FAILED;
                        LOG_ERROR("Max restart attempts reached, giving up");
                    }
                }
            }
        } else {
            /* Suricata is running, reset restart counter */
            if (wd->state == WATCHDOG_STATE_RUNNING) {
                wd->restart_count = 0;
            }
        }
    }

    /* Stop Suricata on shutdown */
    pthread_mutex_unlock(&wd->mutex);
    stop_suricata(wd);
    pthread_mutex_lock(&wd->mutex);

    wd->state = WATCHDOG_STATE_STOPPED;

    pthread_mutex_unlock(&wd->mutex);

    LOG_INFO("Watchdog thread stopped");
    return NULL;
}

/**
 * Start watchdog.
 */
int vnids_watchdog_start(vnids_watchdog_t* wd) {
    if (!wd) return -1;

    pthread_mutex_lock(&wd->mutex);

    if (wd->thread_started) {
        pthread_mutex_unlock(&wd->mutex);
        return -1;
    }

    /* Validate configuration */
    if (strlen(wd->suricata_binary) == 0) {
        LOG_ERROR("Suricata binary not configured");
        pthread_mutex_unlock(&wd->mutex);
        return -1;
    }

    if (strlen(wd->suricata_config) == 0) {
        LOG_ERROR("Suricata config not configured");
        pthread_mutex_unlock(&wd->mutex);
        return -1;
    }

    wd->running = true;

    int ret = pthread_create(&wd->thread, NULL, watchdog_thread, wd);
    if (ret != 0) {
        LOG_ERROR("Failed to create watchdog thread: %s", strerror(ret));
        wd->running = false;
        pthread_mutex_unlock(&wd->mutex);
        return -1;
    }

    wd->thread_started = true;

    pthread_mutex_unlock(&wd->mutex);
    return 0;
}

/**
 * Stop watchdog.
 */
void vnids_watchdog_stop(vnids_watchdog_t* wd) {
    if (!wd) return;

    pthread_mutex_lock(&wd->mutex);

    if (!wd->thread_started) {
        pthread_mutex_unlock(&wd->mutex);
        return;
    }

    wd->running = false;
    pthread_cond_signal(&wd->cond);

    pthread_mutex_unlock(&wd->mutex);

    pthread_join(wd->thread, NULL);

    pthread_mutex_lock(&wd->mutex);
    wd->thread_started = false;
    pthread_mutex_unlock(&wd->mutex);

    LOG_INFO("Watchdog stopped");
}

/**
 * Check if Suricata is running.
 */
bool vnids_watchdog_is_suricata_running(vnids_watchdog_t* wd) {
    if (!wd) return false;

    pthread_mutex_lock(&wd->mutex);
    bool running = is_suricata_running(wd);
    pthread_mutex_unlock(&wd->mutex);

    return running;
}

/**
 * Get Suricata PID.
 */
pid_t vnids_watchdog_get_pid(vnids_watchdog_t* wd) {
    if (!wd) return -1;

    pthread_mutex_lock(&wd->mutex);
    pid_t pid = wd->suricata_pid;
    pthread_mutex_unlock(&wd->mutex);

    return pid;
}

/**
 * Send signal to reload rules.
 */
int vnids_watchdog_reload_rules(vnids_watchdog_t* wd) {
    if (!wd) return -1;

    pthread_mutex_lock(&wd->mutex);

    if (wd->suricata_pid <= 0) {
        pthread_mutex_unlock(&wd->mutex);
        return -1;
    }

    LOG_INFO("Sending SIGUSR2 to Suricata for rule reload");

    int ret = kill(wd->suricata_pid, SIGUSR2);
    if (ret != 0) {
        LOG_ERROR("Failed to send SIGUSR2: %s", strerror(errno));
    }

    pthread_mutex_unlock(&wd->mutex);
    return ret;
}

/**
 * Get watchdog state.
 */
const char* vnids_watchdog_state_str(vnids_watchdog_t* wd) {
    if (!wd) return "unknown";

    pthread_mutex_lock(&wd->mutex);
    const char* state_str;
    switch (wd->state) {
        case WATCHDOG_STATE_STOPPED:    state_str = "stopped"; break;
        case WATCHDOG_STATE_STARTING:   state_str = "starting"; break;
        case WATCHDOG_STATE_RUNNING:    state_str = "running"; break;
        case WATCHDOG_STATE_RESTARTING: state_str = "restarting"; break;
        case WATCHDOG_STATE_FAILED:     state_str = "failed"; break;
        default:                        state_str = "unknown"; break;
    }
    pthread_mutex_unlock(&wd->mutex);

    return state_str;
}
