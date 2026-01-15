# Implementation Plan: Suricata-based Vehicle Network IDS with DPI

**Branch**: `001-suricata-ids` | **Date**: 2026-01-15 | **Spec**: [spec.md](./spec.md)
**Input**: Feature specification from `/specs/001-suricata-ids/spec.md`

## Summary

Build a network intrusion detection system (NIDS) with deep packet inspection (DPI) for vehicle networks, based on Suricata running on Android 12+ ARMv8 platforms. The system detects network attacks (flood, malformed packets, Land attacks) and inspects automotive protocols (SOME/IP, DoIP, GB/T 32960.3) and standard protocols (HTTP, DNS, MQTT). Architecture enforces GPL isolation through process separation, using IPC for communication between the GPL-licensed Suricata engine and proprietary native daemon (vnidsd).

**Architecture Change**: Native binary daemon (`vnidsd`) replaces Android Application layer. No Kotlin/Java code - pure native C implementation for the control daemon and Suricata.

## Technical Context

**Language/Version**:
- C17 (vnidsd daemon, vnids-cli, IPC client, event processing)
- C17 (Suricata modifications, protocol parsers)

**Primary Dependencies**:
- Suricata 7.x (GPL, isolated process)
- Android NDK r25+ (native cross-compilation)
- cJSON (JSON parsing for EVE events)
- libsqlite3 (event persistence)
- pthread (threading)
- epoll (event loop)
- libsystemd (optional, for Linux/Yocto init integration)

**Storage**:
- SQLite (security event persistence)
- Files (rule storage, configuration in /etc/vnids/ or /data/vnids/)

**Testing**:
- CUnit/Unity (C unit tests)
- libFuzzer (parser fuzz testing)
- pytest + scapy (traffic replay integration tests)
- Valgrind (memory leak detection)

**Target Platform**:
- Android 12+ (API 31+), ARMv8 (AArch64) - as native daemon
- Linux ARM64 (Yocto/Buildroot) - for non-Android automotive platforms

**Project Type**: Native Embedded System (dual-process daemon architecture)

**Performance Goals**:
- <10ms p99 packet-to-alert latency
- 100 Mbps sustained throughput
- <5% idle battery drain/hour

**Constraints**:
- 64MB memory budget for IDS engine
- GPL isolation (no linking with proprietary code)
- No dynamic allocation in hot path
- No Android framework dependencies (pure native)

**Scale/Scope**:
- ~30 built-in attack detection rules
- 5 automotive protocol parsers
- 6 standard protocol parsers

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Requirement | Compliance | Notes |
|-----------|-------------|------------|-------|
| I. ARM Memory Optimization | Cache-aligned structs, memory pools, NEON, 64MB limit, zero-copy | ✅ PLANNED | Memory pool design in research.md |
| II. Real-Time Performance | <10ms p99, 100Mbps, lock-free paths, SCHED_FIFO | ✅ PLANNED | Latency budget in spec aligns |
| III. Automotive Reliability | Defensive coding, no UB, fail-safe, watchdog | ✅ PLANNED | Watchdog in edge cases |
| IV. Suricata Compatibility | 7.x rule format, SID/GID preserved, hot reload | ✅ PLANNED | FR-010, FR-011 cover this |
| V. Android NDK Integration | arm64-v8a, JNI batching, lifecycle awareness | ✅ ADAPTED | Native binary, no JNI needed; NDK used for cross-compile only |
| VI. Power Efficiency | <5% idle drain, adaptive processing, wake lock discipline | ✅ PLANNED | Native daemon uses less power than Android service |
| VII. Security-First | Input validation, memory safety, approved crypto | ✅ PLANNED | Strict C coding standards, static analysis |
| VIII. Testing Discipline | 80% unit, integration, fuzz, perf, HIL, security | ✅ PLANNED | Test strategy in research.md |

**Gate Status**: ✅ PASS - All principles addressed in design

## Project Structure

### Documentation (this feature)

```text
specs/001-suricata-ids/
├── plan.md              # This file
├── research.md          # Phase 0 output
├── data-model.md        # Phase 1 output
├── quickstart.md        # Phase 1 output
├── contracts/           # Phase 1 output
│   ├── ipc-protocol.md  # Suricata ↔ vnidsd IPC contract
│   └── event-schema.json # Security event JSON schema
└── tasks.md             # Phase 2 output (/speckit.tasks command)
```

### Source Code (repository root)

```text
# VNIDS Daemon (C, proprietary)
vnidsd/
├── src/
│   ├── main.c               # Daemon entry point
│   ├── daemon.c             # Daemon initialization
│   ├── config.c             # Configuration loading (INI format)
│   ├── config_validate.c    # Config validation
│   ├── eventloop.c          # epoll-based event loop
│   ├── signal.c             # Signal handling
│   ├── log.c                # Logging with syslog
│   ├── pidfile.c            # PID file management
│   ├── ipc_server.c         # Unix socket API server
│   ├── ipc_client.c         # Connect to Suricata EVE socket
│   ├── ipc_control.c        # Send control commands
│   ├── ipc_message.c        # JSON message serialization
│   ├── eve_parser.c         # EVE JSON parsing (cJSON)
│   ├── eve_reader.c         # EVE event reader
│   ├── event.c              # SecurityEvent structure
│   ├── event_queue.c        # Lock-free event queue
│   ├── handler_alert.c      # Alert event handler
│   ├── handler_flow.c       # Flow event handler
│   ├── handler_stats.c      # Stats event handler
│   ├── storage.c            # SQLite initialization
│   ├── storage_event.c      # Event persistence
│   ├── storage_query.c      # Event queries
│   ├── storage_retention.c  # Data retention cleanup
│   ├── rule_loader.c        # Load rules from disk
│   ├── rule_parser.c        # Rule parsing
│   ├── rule_validate.c      # Syntax validation
│   ├── rule_reload.c        # SIGUSR2 hot reload
│   ├── rule_rollback.c      # Rollback on failure
│   ├── watchdog.c           # Process supervision
│   ├── suricata_spawn.c     # Suricata lifecycle
│   ├── metadata.c           # Protocol metadata
│   ├── metadata_someip.c    # SOME/IP metadata
│   ├── metadata_doip.c      # DoIP metadata
│   ├── metadata_http.c      # HTTP metadata
│   ├── metadata_dns.c       # DNS metadata
│   ├── mempool.c            # Memory pool allocator
│   └── metrics.c            # Performance metrics
├── include/
│   ├── vnidsd.h             # Main header
│   ├── config.h             # Config structures
│   ├── event.h              # Event structures
│   ├── rule.h               # Rule structures
│   ├── ipc.h                # IPC definitions
│   ├── storage.h            # Storage API
│   ├── watchdog.h           # Watchdog API
│   ├── mempool.h            # Memory pool API
│   ├── error.h              # Error codes
│   └── align.h              # Cache alignment macros
├── tests/
│   ├── test_config.c        # Config unit tests
│   ├── test_event.c         # Event unit tests
│   ├── test_ipc.c           # IPC unit tests
│   ├── test_storage.c       # Storage unit tests
│   └── test_watchdog.c      # Watchdog unit tests
├── Makefile
├── CMakeLists.txt
└── vnidsd.conf.example      # Example configuration

# CLI Tools (C, proprietary)
vnids-cli/
├── src/
│   ├── main.c               # CLI entry point (getopt)
│   ├── client.c             # Connect to vnidsd API
│   ├── output.c             # Colored output formatting
│   ├── cmd_status.c         # vnidsctl status
│   ├── cmd_stats.c          # vnidsctl stats
│   ├── cmd_events.c         # vnidsctl events list/tail/export
│   ├── cmd_rules.c          # vnidsctl rules list/reload/validate
│   └── cmd_config.c         # vnidsctl config show/get
├── include/
│   ├── vnidsctl.h           # Main header
│   └── client.h             # Client API
├── Makefile
└── CMakeLists.txt

# Suricata Engine (GPL, isolated process)
suricata/
├── src/
│   ├── protocols/           # Custom protocol parsers
│   │   ├── someip/          # SOME/IP parser
│   │   ├── doip/            # DoIP parser
│   │   └── gbt32960/        # GB/T 32960.3 parser
│   ├── output/              # Custom EVE output (Unix socket)
│   └── patches/             # Android/ARM patches
├── rules/
│   ├── baseline/            # Built-in detection rules
│   │   ├── flood-attacks.rules
│   │   ├── malformed-packets.rules
│   │   ├── automotive.rules
│   │   └── weak-credentials.rules
│   └── custom/              # User-uploaded rules
├── scripts/
│   ├── build-android.sh     # Cross-compile for Android ARM64
│   └── build-yocto.sh       # Cross-compile for Yocto Linux
└── suricata.yaml.template   # Config template

# Shared (non-GPL, interface definitions only)
shared/
├── include/
│   ├── vnids_types.h        # Common types
│   ├── vnids_ipc.h          # IPC message structures
│   ├── vnids_event.h        # Security event structures
│   ├── vnids_config.h       # Config structures
│   └── vnids_log.h          # Logging macros
└── docs/
    └── architecture.md

# Platform Integration
deploy/
├── android/
│   ├── init.vnids.rc        # Android init.rc service definition
│   └── vnids.te             # SELinux policy
├── yocto/
│   ├── vnids.service        # systemd unit file
│   └── vnids.bb             # Yocto recipe
└── scripts/
    ├── install.sh           # Installation script
    └── uninstall.sh

# Tests
tests/
├── unit/                    # C unit tests (CUnit/Unity)
├── integration/             # Traffic replay tests
│   ├── pcaps/               # Test capture files
│   └── test_detection.py
├── fuzz/                    # Fuzz test harnesses
│   ├── fuzz_rule_parser/
│   └── fuzz_someip_parser/
├── benchmark/               # Performance benchmarks
└── hil/                     # Hardware-in-loop test configs
```

**Structure Decision**: Native dual-process daemon architecture. `vnidsd` (C) runs as the control daemon, managing Suricata lifecycle, event processing, and providing CLI/API interface. Suricata (C, GPL) runs as an isolated subprocess. Communication via Unix domain sockets only. No Android framework dependencies - can run on any Linux ARM64 platform.

## Complexity Tracking

| Aspect | Justification |
|--------|---------------|
| Pure C implementation | Direct hardware control, maximum performance on embedded ARM, consistent with Suricata |
| Process isolation | Required for GPL compliance - cannot link Suricata into daemon |
| Custom protocol parsers | SOME/IP, DoIP, GB/T 32960.3 not in upstream Suricata; automotive requirement |
| Memory pool allocator | Required for real-time performance on constrained ARM devices |
| Native daemon vs Android app | Simpler deployment, lower overhead, works on Yocto/Android/any Linux ARM64 |

## Architecture Diagram

```
┌─────────────────────────────────────────────────────────────────────┐
│                         Host Platform                                │
│  (Android 12+ / Yocto Linux / Buildroot)                            │
│                                                                      │
│  ┌─────────────────────────────────────────────────────────────┐    │
│  │                    vnidsd (C daemon)                         │    │
│  │  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌─────────────┐  │    │
│  │  │ Watchdog │  │  Events  │  │  Rules   │  │  API/CLI    │  │    │
│  │  │          │  │  Queue   │  │  Loader  │  │  Interface  │  │    │
│  │  └────┬─────┘  └────┬─────┘  └────┬─────┘  └──────┬──────┘  │    │
│  │       │             │             │               │          │    │
│  │       └─────────────┴──────┬──────┴───────────────┘          │    │
│  │                            │                                  │    │
│  │                    Unix Socket IPC                            │    │
│  │                     /var/run/vnids/                           │    │
│  └────────────────────────────┬─────────────────────────────────┘    │
│                               │                                      │
│         Process Boundary      │      (fork/exec)                     │
│                               │                                      │
│  ┌────────────────────────────┴─────────────────────────────────┐    │
│  │                  Suricata 7.x (GPL)                           │    │
│  │  ┌──────────────┐  ┌──────────────┐  ┌────────────────────┐  │    │
│  │  │   Capture    │  │    Detect    │  │   EVE Output       │  │    │
│  │  │  (AF_PACKET) │  │  (Rules +    │  │   (Unix Socket)    │  │    │
│  │  │              │  │   DPI)       │  │                    │  │    │
│  │  └──────────────┘  └──────────────┘  └────────────────────┘  │    │
│  │                                                               │    │
│  │  Custom Parsers: SOME/IP | DoIP | GB/T 32960.3               │    │
│  └───────────────────────────────────────────────────────────────┘    │
│                                                                      │
│  ┌─────────────────┐                                                 │
│  │   vnids-cli     │  (vnidsctl status/rules/events/config)         │
│  │   (C CLI)       │                                                 │
│  └─────────────────┘                                                 │
└──────────────────────────────────────────────────────────────────────┘
```

## Deployment Modes

| Platform | Init System | Install Path | Config Path |
|----------|-------------|--------------|-------------|
| Android 12+ | init.rc | /system/bin/vnidsd | /data/vnids/ |
| Android (Magisk) | init.rc | /data/adb/modules/vnids/ | /data/vnids/ |
| Yocto Linux | systemd | /usr/bin/vnidsd | /etc/vnids/ |
| Buildroot | BusyBox init | /usr/bin/vnidsd | /etc/vnids/ |
