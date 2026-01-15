# Research: Suricata-based Vehicle Network IDS

**Feature**: 001-suricata-ids
**Date**: 2026-01-15
**Status**: Complete
**Architecture**: Native daemon (vnidsd) + Suricata subprocess

## Table of Contents

1. [Native Daemon Packet Capture](#1-native-daemon-packet-capture)
2. [Suricata Cross-Compilation for ARM64](#2-suricata-cross-compilation-for-arm64)
3. [GPL Isolation Architecture](#3-gpl-isolation-architecture)
4. [SOME/IP Protocol Parser Implementation](#4-someip-protocol-parser-implementation)
5. [DoIP Protocol Parser Implementation](#5-doip-protocol-parser-implementation)
6. [GB/T 32960.3 Protocol Parser Implementation](#6-gbt-329603-protocol-parser-implementation)
7. [Memory Pool Design for ARM](#7-memory-pool-design-for-arm)
8. [Lock-Free Event Queue Design](#8-lock-free-event-queue-design)
9. [Native Daemon Watchdog Implementation](#9-native-daemon-watchdog-implementation)
10. [Rule Hot Reload Mechanism](#10-rule-hot-reload-mechanism)
11. [Platform Init System Integration](#11-platform-init-system-integration)

---

## 1. Native Daemon Packet Capture

### Decision
Use AF_PACKET raw sockets for packet capture on rooted/system devices, with Suricata's native capture as primary method.

### Rationale
- Native daemon runs with root privileges on automotive platforms
- AF_PACKET provides Layer 2 access (Ethernet headers included)
- Suricata has built-in AF_PACKET support, optimized for performance
- No Android framework dependencies - works on Yocto, Buildroot, Android

### Implementation Approach
```rust
// vnidsd delegates capture to Suricata
// Suricata configuration (suricata.yaml)
// af-packet:
//   - interface: eth0
//     cluster-id: 99
//     cluster-type: cluster_flow
//     defrag: yes
```

```rust
// Optional: vnidsd can also capture for preprocessing
use libc::{socket, AF_PACKET, SOCK_RAW, ETH_P_ALL};

fn create_raw_socket(interface: &str) -> Result<RawFd, io::Error> {
    unsafe {
        let sock = socket(AF_PACKET, SOCK_RAW, (ETH_P_ALL as u16).to_be() as i32);
        if sock < 0 {
            return Err(io::Error::last_os_error());
        }
        bind_to_interface(sock, interface)?;
        Ok(sock)
    }
}
```

### Capture Modes

| Mode | Use Case | Requirements |
|------|----------|--------------|
| Suricata AF_PACKET | Primary - all detection | Root, kernel support |
| Suricata PCAP | Fallback - libpcap | libpcap installed |
| vnidsd passthrough | Preprocessing/filtering | Root |

### Alternatives Considered
| Alternative | Rejected Because |
|-------------|------------------|
| VpnService (Android) | Not available in native daemon, requires Android framework |
| eBPF/XDP | Requires kernel 5.x+, complex setup |
| DPDK | Overkill for automotive, requires hugepages |
| nfqueue | Higher latency, requires iptables |

### Platform-Specific Notes
- **Android**: Requires SELinux policy for raw socket access
- **Yocto**: Standard Linux capabilities, no special config
- **Buildroot**: May need kernel config for AF_PACKET

---

## 2. Suricata Cross-Compilation for ARM64

### Decision
Cross-compile Suricata 7.0.x using Android NDK r25 or standard ARM64 toolchain with static linking.

### Rationale
- Suricata 7.x has improved embedded support and lower memory footprint
- Static linking avoids shared library conflicts
- Same binary works on Android and Yocto with appropriate libc

### Build Configuration (Android)
```bash
# Environment setup
export ANDROID_NDK=/path/to/android-ndk-r25c
export TARGET=aarch64-linux-android
export API=31
export TOOLCHAIN=$ANDROID_NDK/toolchains/llvm/prebuilt/linux-x86_64

# Configure Suricata
./configure \
    --host=$TARGET \
    --prefix=/data/vnids \
    --disable-gccmarch-native \
    --disable-shared \
    --enable-static \
    --disable-python \
    --disable-suricata-update \
    --with-libpcre-includes=$DEPS/include \
    --with-libpcre-libraries=$DEPS/lib \
    --with-libyaml-includes=$DEPS/include \
    --with-libyaml-libraries=$DEPS/lib \
    CC=$TOOLCHAIN/bin/$TARGET$API-clang \
    CFLAGS="-O2 -fPIC -march=armv8-a"
```

### Build Configuration (Yocto/Buildroot)
```bash
# Standard ARM64 cross-compile
export CROSS_COMPILE=aarch64-linux-gnu-
export CC=${CROSS_COMPILE}gcc
export CXX=${CROSS_COMPILE}g++

./configure \
    --host=aarch64-linux-gnu \
    --prefix=/usr \
    --sysconfdir=/etc/vnids \
    --localstatedir=/var \
    --disable-shared \
    --enable-static \
    --disable-python
```

### Required Dependencies (statically linked)
- libpcre2 (pattern matching)
- libyaml (config parsing)
- libjansson (JSON output)
- zlib (compression)
- libpcap (optional, for pcap capture mode)

### Alternatives Considered
| Alternative | Rejected Because |
|-------------|------------------|
| Snort 3 | Heavier memory footprint, less embedded-friendly |
| Zeek | Requires full C++ runtime, 100MB+ memory |
| nDPI standalone | Detection only, no rule engine |
| Custom IDS engine | Years of development vs proven Suricata |

---

## 3. GPL Isolation Architecture

### Decision
Run Suricata as a separate executable process spawned by vnidsd, communicating via Unix domain sockets using JSON-formatted EVE events.

### Rationale
- GPL v2 "linking" requirement applies to static/dynamic library linking
- Separate processes communicating via IPC are considered independent works
- This is the established pattern (e.g., how GIMP plugins work)
- EVE JSON output is Suricata's standard external interface

### Architecture Diagram
```
┌─────────────────────────────────────────────────────────────────┐
│                    vnidsd (Rust daemon)                          │
│  ┌─────────────┐    ┌─────────────┐    ┌─────────────────────┐  │
│  │  Watchdog   │    │   Events    │    │   Unix Socket       │  │
│  │  Manager    │    │   Queue     │    │   /var/run/vnids/   │  │
│  └─────────────┘    └─────────────┘    └──────────┬──────────┘  │
│         (Proprietary - Apache 2.0)                │              │
└───────────────────────────────────────────────────┼──────────────┘
                                                    │
                        Process Boundary (fork/exec)│
                                                    │
┌───────────────────────────────────────────────────┼──────────────┐
│                   Suricata Process                │              │
│  ┌─────────────┐    ┌─────────────┐    ┌─────────┴───────────┐  │
│  │   Packet    │───→│    Rule     │───→│   EVE JSON Output   │  │
│  │  Capture    │    │   Engine    │    │   (Unix Socket)     │  │
│  └─────────────┘    └─────────────┘    └─────────────────────┘  │
│                     (GPL v2)                                     │
└──────────────────────────────────────────────────────────────────┘
```

### IPC Protocol Design
- Transport: Unix domain socket (SOCK_STREAM)
- Format: Newline-delimited JSON (EVE format)
- Flow: Unidirectional (Suricata → vnidsd) for events
- Control: Separate socket for commands (reload, stats)

### Socket Paths
| Socket | Path | Purpose |
|--------|------|---------|
| Event | `/var/run/vnids/events.sock` | Suricata → vnidsd (events) |
| Control | `/var/run/vnids/control.sock` | vnidsd → Suricata (commands) |
| API | `/var/run/vnids/api.sock` | CLI → vnidsd (management) |

### Alternatives Considered
| Alternative | Rejected Because |
|-------------|------------------|
| Shared memory | Complex synchronization, still may trigger GPL concerns |
| Named pipes (FIFO) | Less flexible than sockets, no bidirectional easily |
| D-Bus | Overkill for simple JSON streaming, adds dependency |
| LGPL wrapper library | Suricata is GPL, not LGPL - no linking exception |

---

## 4. SOME/IP Protocol Parser Implementation

### Decision
Implement SOME/IP parser as a Suricata app-layer protocol using the Rust-based detection framework.

### Rationale
- SOME/IP is a critical automotive middleware protocol
- Suricata's app-layer framework supports custom protocol parsers
- Rust parser can be GPL-compatible while ensuring memory safety

### SOME/IP Message Structure
```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|           Service ID          |           Method ID           |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                            Length                             |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|           Client ID           |           Session ID          |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|Proto Ver| Iface Ver |Msg Type |  Return Code  |    Payload... |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

### Detection Keywords
```suricata
# Example SOME/IP rules
alert someip any any -> any any (msg:"SOME/IP unauthorized service access"; \
    someip.service_id:0x1234; someip.method_id:0x8001; \
    sid:1000001; rev:1;)

alert someip any any -> any any (msg:"SOME/IP SD malformed"; \
    someip.message_type:notification; content:"|FF FF|"; \
    sid:1000002; rev:1;)
```

### Port Detection
- UDP/TCP port 30490 (default SOME/IP)
- UDP port 30491 (SOME/IP-SD)
- Dynamic port detection via SOME/IP-SD

---

## 5. DoIP Protocol Parser Implementation

### Decision
Implement DoIP (ISO 13400) parser covering diagnostic message types relevant to intrusion detection.

### Rationale
- DoIP is the standard for diagnostics over IP in modern vehicles
- Unauthorized diagnostic access is a primary attack vector
- Suricata's existing UDS over CAN patterns can inform DoIP detection

### DoIP Header Structure
```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
| Protocol Ver  | Inverse Ver   |        Payload Type           |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                         Payload Length                        |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                         Payload...                            |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

### Key Payload Types for Detection
| Type | Name | Security Relevance |
|------|------|-------------------|
| 0x0005 | Routing Activation Request | Unauthorized tester connection |
| 0x0006 | Routing Activation Response | Failed auth attempts |
| 0x8001 | Diagnostic Message | UDS service monitoring |
| 0x8002 | Diagnostic Message Ack | Response analysis |

### Detection Keywords
```suricata
alert doip any any -> any any (msg:"DoIP routing activation from external"; \
    doip.payload_type:0x0005; \
    doip.source_address:!0x0E00-0x0EFF; \  # Not from internal tester
    sid:2000001; rev:1;)
```

### Port Detection
- TCP port 13400 (DoIP data)
- UDP port 13400 (DoIP discovery)

---

## 6. GB/T 32960.3 Protocol Parser Implementation

### Decision
Implement GB/T 32960.3 (China national EV telematics standard) parser for vehicle-to-cloud communication monitoring.

### Rationale
- Mandatory for electric vehicles sold in China
- Contains sensitive battery and location data
- Potential attack vector for vehicle tracking/manipulation

### Message Structure
```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|  Start (##)   |   Command     |   Response    |      VIN...   |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                         ...VIN (17 bytes)...                  |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|  Encryption   |        Data Length            |    Data...    |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

### Key Commands for Detection
| Command | Name | Security Relevance |
|---------|------|-------------------|
| 0x01 | Vehicle Login | Auth monitoring |
| 0x02 | Real-time Data | Data exfiltration |
| 0x03 | Reissue Data | Replay detection |
| 0x04 | Vehicle Logout | Session monitoring |
| 0x05 | Platform Login | Server-side auth |

### Detection Keywords
```suricata
alert gbt32960 any any -> any any (msg:"GB/T 32960 unencrypted real-time data"; \
    gbt32960.command:0x02; gbt32960.encryption:0x01; \  # Unencrypted
    sid:3000001; rev:1;)
```

---

## 7. Memory Pool Design for ARM

### Decision
Implement a slab allocator with per-CPU pools optimized for ARM cache line sizes (64 bytes).

### Rationale
- Constitution requires no dynamic allocation in hot path
- ARM Cortex-A cache line is 64 bytes
- Per-CPU pools avoid lock contention on multi-core
- Fixed-size slabs match packet buffer requirements

### Design
```c
// Cache-aligned slab header
struct slab_header {
    uint64_t bitmap;        // 64 slots per slab
    struct slab_header *next;
    uint8_t cpu_id;
    uint8_t size_class;     // 0=64B, 1=128B, 2=256B, 3=512B, 4=1024B, 5=2048B
    uint16_t free_count;
    uint32_t padding;
} __attribute__((aligned(64)));

// Size classes for common allocations
static const size_t SIZE_CLASSES[] = {
    64,    // Cache line, small metadata
    128,   // EVE event structure
    256,   // Protocol header parsed
    512,   // Small packet
    1024,  // Medium packet
    2048   // Large packet/MTU
};

// Per-CPU pool structure
struct cpu_pool {
    struct slab_header *partial[6];  // One list per size class
    struct slab_header *full[6];
    uint64_t alloc_count;
    uint64_t free_count;
} __attribute__((aligned(64)));
```

### Memory Budget Allocation
| Pool | Size Class | Count | Total |
|------|-----------|-------|-------|
| Packet buffers | 2048B | 4096 | 8 MB |
| Events | 128B | 16384 | 2 MB |
| Sessions | 256B | 8192 | 2 MB |
| Rules (loaded) | varies | - | 32 MB |
| Misc | varies | - | 20 MB |
| **Total** | | | **64 MB** |

---

## 8. Lock-Free Event Queue Design

### Decision
Use a multi-producer single-consumer (MPSC) lock-free queue based on atomic operations for event delivery.

### Rationale
- Constitution requires lock-free hot paths
- Multiple Suricata worker threads produce events
- Single IPC sender thread consumes
- ARM has strong atomic operation support

### Design
```c
#include <stdatomic.h>
#include <stddef.h>

#define QUEUE_SIZE 32768

typedef struct {
    void *buffer[QUEUE_SIZE];
    _Atomic size_t head;  // Next write position (producers)
    _Atomic size_t tail;  // Next read position (consumer)
} lock_free_queue_t;

static inline int queue_push(lock_free_queue_t *q, void *item) {
    size_t head = atomic_load_explicit(&q->head, memory_order_relaxed);
    size_t next_head = (head + 1) % QUEUE_SIZE;

    if (next_head == atomic_load_explicit(&q->tail, memory_order_acquire)) {
        return -1; // Queue full
    }

    q->buffer[head] = item;
    atomic_store_explicit(&q->head, next_head, memory_order_release);
    return 0;
}

static inline void *queue_pop(lock_free_queue_t *q) {
    size_t tail = atomic_load_explicit(&q->tail, memory_order_relaxed);

    if (tail == atomic_load_explicit(&q->head, memory_order_acquire)) {
        return NULL; // Queue empty
    }

    void *item = q->buffer[tail];
    atomic_store_explicit(&q->tail, (tail + 1) % QUEUE_SIZE, memory_order_release);
    return item;
}
```

### Queue Sizing
- Event queue capacity: 32768 events
- Event size: 128 bytes (fixed)
- Total queue memory: 4 MB
- Expected throughput: >100K events/sec

---

## 9. Native Daemon Watchdog Implementation

### Decision
Implement a dedicated watchdog thread in vnidsd that monitors Suricata process health and performs automatic recovery.

### Rationale
- Constitution requires watchdog integration
- Native thread provides sub-second crash detection
- Matches SC-008: 3-second recovery target
- No Android framework dependencies

### Implementation
```c
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/un.h>

typedef struct {
    pid_t suricata_pid;
    char config_path[256];
    char ipc_socket[256];
    int restart_count;
    time_t last_restart;
} suricata_watchdog_t;

void *watchdog_thread(void *arg) {
    suricata_watchdog_t *wd = (suricata_watchdog_t *)arg;

    while (1) {
        usleep(500000); // 500ms

        if (wd->suricata_pid <= 0) {
            log_warn("Suricata not running, starting...");
            start_suricata(wd);
            continue;
        }

        // Check if process still alive
        int status;
        pid_t result = waitpid(wd->suricata_pid, &status, WNOHANG);

        if (result == wd->suricata_pid) {
            // Process exited
            log_error("Suricata exited with status: %d", WEXITSTATUS(status));
            handle_crash(wd);
        } else if (result == 0) {
            // Still running, check IPC heartbeat
            if (!check_ipc_heartbeat(wd)) {
                log_error("Suricata IPC unresponsive");
                handle_hang(wd);
            }
        }
    }
    return NULL;
}

static void handle_crash(suricata_watchdog_t *wd) {
    log_crash_event(wd);
    cleanup_sockets(wd);
    apply_restart_backoff(wd);
    start_suricata(wd);
}

static void handle_hang(suricata_watchdog_t *wd) {
    kill(wd->suricata_pid, SIGKILL);
    wd->suricata_pid = -1;
    handle_crash(wd);
}

static int start_suricata(suricata_watchdog_t *wd) {
    pid_t pid = fork();
    if (pid == 0) {
        // Child process
        execl("/usr/bin/suricata", "suricata",
              "-c", wd->config_path,
              "--af-packet",
              "-D", NULL);
        _exit(1);
    } else if (pid > 0) {
        wd->suricata_pid = pid;
        wd->restart_count++;
        wd->last_restart = time(NULL);
        log_info("Suricata started (pid: %d, restart count: %d)",
                 pid, wd->restart_count);
        return 0;
    }
    return -1;
}

static int check_ipc_heartbeat(suricata_watchdog_t *wd) {
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) return 0;

    struct sockaddr_un addr = {0};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, wd->ipc_socket, sizeof(addr.sun_path) - 1);

    struct timeval tv = {.tv_sec = 2, .tv_usec = 0};
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(sock);
        return 0;
    }

    char buf[1024];
    int result = recv(sock, buf, sizeof(buf), 0) > 0;
    close(sock);
    return result;
}
```

### Recovery Actions
1. Log crash event with timestamp and exit status
2. Kill hung process if still exists (SIGKILL)
3. Clean up IPC socket files
4. Apply exponential backoff if restarting too frequently
5. Restart Suricata with preserved configuration
6. Resume event streaming

### Restart Backoff
| Restart Count | Delay Before Restart |
|---------------|---------------------|
| 1-3 | Immediate |
| 4-10 | 5 seconds |
| 11-20 | 30 seconds |
| 21+ | 60 seconds |

---

## 10. Rule Hot Reload Mechanism

### Decision
Use Suricata's built-in rule reload signal (SIGUSR2) with pre-validation in vnidsd.

### Rationale
- Suricata natively supports SIGUSR2 for reload without restart
- Pre-validation catches syntax errors before disrupting detection
- Matches FR-011/FR-012/FR-013 requirements

### Reload Flow
```
┌──────────────────┐    ┌──────────────────┐    ┌──────────────────┐
│   Rule Update    │───→│   Validation     │───→│   Apply Reload   │
│   (new .rules)   │    │   (dry-run)      │    │   (SIGUSR2)      │
└──────────────────┘    └──────────────────┘    └──────────────────┘
         │                       │                       │
         │                       │ Fail                  │
         │                       ▼                       │
         │              ┌──────────────────┐             │
         │              │ Log Error,       │             │
         │              │ Keep Old Rules   │             │
         │              └──────────────────┘             │
         │                                               │
         ▼                                               ▼
┌──────────────────┐                        ┌──────────────────┐
│   rules/custom/  │                        │   Detection      │
│   (staging area) │                        │   Continues      │
└──────────────────┘                        └──────────────────┘
```

### Validation Step
```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

typedef struct {
    int code;
    char message[512];
} rule_error_t;

int validate_rules(const char *rules_path, rule_error_t *error) {
    pid_t pid = fork();
    if (pid == 0) {
        // Child: run Suricata in validation mode
        int null_fd = open("/dev/null", O_WRONLY);
        dup2(null_fd, STDOUT_FILENO);

        execl("/usr/bin/suricata", "suricata",
              "-T", "-c", "/etc/vnids/suricata.yaml",
              "-S", rules_path, NULL);
        _exit(1);
    }

    int status;
    waitpid(pid, &status, 0);

    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        return 0; // Success
    }

    error->code = WEXITSTATUS(status);
    snprintf(error->message, sizeof(error->message),
             "Rule validation failed with exit code %d", error->code);
    return -1;
}
```

### Reload Implementation
```c
#include <signal.h>
#include <time.h>

int reload_rules(pid_t suricata_pid, int timeout_sec) {
    // Send SIGUSR2 to trigger reload
    if (kill(suricata_pid, SIGUSR2) < 0) {
        return -1;
    }

    // Wait for reload completion (check stats socket)
    time_t start = time(NULL);
    while (time(NULL) - start < timeout_sec) {
        if (check_reload_complete()) {
            return 0;
        }
        usleep(100000); // 100ms
    }

    return -1; // Timeout
}
```

### Rollback Mechanism
- Keep previous rules in `rules/backup/`
- If reload fails or detection gaps detected, restore from backup
- Log all reload attempts with success/failure status

---

## 11. Platform Init System Integration

### Decision
Provide init scripts for Android init.rc, systemd (Yocto), and BusyBox init (Buildroot).

### Rationale
- VNIDS must start on boot without user intervention
- Different automotive platforms use different init systems
- Need to handle dependencies (network, storage)

### Android init.rc
```rc
# /system/etc/init/vnids.rc
service vnidsd /system/bin/vnidsd --config /data/vnids/vnidsd.toml
    class main
    user root
    group root inet net_admin net_raw
    disabled
    oneshot
    seclabel u:r:vnidsd:s0

on property:sys.boot_completed=1
    start vnidsd
```

### systemd Unit (Yocto)
```ini
# /usr/lib/systemd/system/vnids.service
[Unit]
Description=Vehicle Network Intrusion Detection System
After=network-online.target
Wants=network-online.target

[Service]
Type=notify
ExecStart=/usr/bin/vnidsd --config /etc/vnids/vnidsd.toml
ExecReload=/bin/kill -USR2 $MAINPID
Restart=always
RestartSec=5
WatchdogSec=30
MemoryMax=80M
CPUQuota=50%

[Install]
WantedBy=multi-user.target
```

### BusyBox init (Buildroot)
```sh
#!/bin/sh
# /etc/init.d/S90vnids

DAEMON=/usr/bin/vnidsd
PIDFILE=/var/run/vnidsd.pid
CONFIG=/etc/vnids/vnidsd.toml

case "$1" in
    start)
        echo "Starting vnidsd..."
        start-stop-daemon -S -b -m -p $PIDFILE -x $DAEMON -- --config $CONFIG
        ;;
    stop)
        echo "Stopping vnidsd..."
        start-stop-daemon -K -p $PIDFILE
        ;;
    restart)
        $0 stop
        sleep 1
        $0 start
        ;;
    *)
        echo "Usage: $0 {start|stop|restart}"
        exit 1
esac
```

### SELinux Policy (Android)
```te
# vnids.te
type vnidsd, domain;
type vnidsd_exec, exec_type, file_type;

init_daemon_domain(vnidsd)
net_domain(vnidsd)

# Allow raw packet capture
allow vnidsd self:capability { net_admin net_raw };
allow vnidsd self:packet_socket create_socket_perms;

# Allow Unix socket IPC
allow vnidsd vnidsd_socket:sock_file create_file_perms;
allow vnidsd vnidsd_socket:unix_stream_socket { create bind listen accept };

# Allow Suricata execution
allow vnidsd suricata_exec:file { execute execute_no_trans };
```

---

## Summary of Key Decisions

| Topic | Decision | Key Benefit |
|-------|----------|-------------|
| Packet Capture | AF_PACKET raw sockets | Layer 2 access, no framework deps |
| Suricata Build | NDK/ARM64 toolchain, static linking | Portable, no library conflicts |
| GPL Isolation | Process separation + Unix sockets | Clear legal boundary |
| SOME/IP Parser | Suricata app-layer module | Native rule integration |
| DoIP Parser | Suricata app-layer module | UDS monitoring |
| GB/T 32960.3 | Suricata app-layer module | China EV compliance |
| Memory | Slab allocator, 64B aligned | ARM cache efficiency |
| Event Queue | Lock-free MPSC | No mutex contention |
| Watchdog | Dedicated thread in vnidsd | Fast crash recovery |
| Rule Reload | Pre-validate + SIGUSR2 | Zero-gap updates |
| Init System | Multi-platform support | Android/Yocto/Buildroot |
