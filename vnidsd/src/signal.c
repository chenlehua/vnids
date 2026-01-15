/**
 * VNIDS Daemon - Signal Handling
 *
 * Copyright (c) 2026 VNIDS Authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <signal.h>
#include <string.h>

#include "vnids_types.h"

/* Signal names for logging */
const char* vnids_signal_name(int signum) {
    switch (signum) {
        case SIGTERM: return "SIGTERM";
        case SIGINT:  return "SIGINT";
        case SIGHUP:  return "SIGHUP";
        case SIGUSR1: return "SIGUSR1";
        case SIGUSR2: return "SIGUSR2";
        case SIGCHLD: return "SIGCHLD";
        case SIGPIPE: return "SIGPIPE";
        default:      return "UNKNOWN";
    }
}

/* Block all signals except specified ones */
int vnids_block_signals(void) {
    sigset_t mask;
    sigfillset(&mask);
    sigdelset(&mask, SIGTERM);
    sigdelset(&mask, SIGINT);
    sigdelset(&mask, SIGHUP);
    sigdelset(&mask, SIGUSR1);
    sigdelset(&mask, SIGCHLD);

    return pthread_sigmask(SIG_SETMASK, &mask, NULL);
}

/* Unblock all signals */
int vnids_unblock_signals(void) {
    sigset_t mask;
    sigemptyset(&mask);
    return pthread_sigmask(SIG_SETMASK, &mask, NULL);
}
