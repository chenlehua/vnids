# VNIDS Development Guidelines

Auto-generated from feature plans. Last updated: 2026-01-15

## Active Technologies

- **Rust 1.75+**: vnidsd daemon, vnidsctl CLI, IPC client, event processing
- **C17**: Suricata modifications, automotive protocol parsers (GPL)
- **Suricata 7.x**: Core IDS engine (GPL, isolated process)
- **Android NDK r25+**: Cross-compilation for Android ARM64
- **ARM64 Toolchain**: Cross-compilation for Yocto/Buildroot

## Project Structure

```text
# VNIDS Daemon (Rust, proprietary)
vnidsd/
├── src/
│   ├── main.rs              # Daemon entry point
│   ├── config/              # Configuration loading
│   ├── ipc/                 # Unix socket IPC with Suricata
│   ├── events/              # Event processing and storage
│   ├── rules/               # Rule management
│   ├── watchdog/            # Process supervision
│   ├── capture/             # Packet capture (optional)
│   └── api/                 # Control API for CLI
└── Cargo.toml

# CLI Tools (Rust, proprietary)
vnids-cli/
├── src/
│   ├── main.rs              # CLI entry point (vnidsctl)
│   └── commands/            # status, rules, events, config
└── Cargo.toml

# Suricata Engine (GPL, isolated process)
suricata/
├── src/protocols/           # Custom parsers (SOME/IP, DoIP, GB/T 32960.3)
├── rules/baseline/          # Built-in detection rules
└── scripts/                 # Build scripts

# Platform Integration
deploy/
├── android/                 # init.rc, SELinux policy
└── yocto/                   # systemd unit, Yocto recipe

# Tests
tests/
├── unit/                    # Rust unit tests
├── integration/             # Traffic replay tests
├── fuzz/                    # Fuzz test harnesses
└── hil/                     # Hardware-in-loop tests
```

## Commands

```bash
# Build vnidsd for Android ARM64
cd vnidsd && cargo ndk -t arm64-v8a build --release

# Build vnidsd for Yocto Linux ARM64
cd vnidsd && cargo build --release --target aarch64-unknown-linux-gnu

# Build Suricata for Android ARM64
cd suricata && ./scripts/build-android.sh

# Build Suricata for Yocto Linux ARM64
cd suricata && ./scripts/build-yocto.sh

# Run Rust tests
cargo test --workspace

# Run integration tests
cd tests/integration && python test_detection.py

# Run fuzz tests
cd tests/fuzz && cargo fuzz run fuzz_someip_parser
```

## Code Style

- **Rust**: Follow `rustfmt` and `clippy` defaults
- **C**: Follow MISRA-C:2012 guidelines where practical, use `clang-format`

## Constitution Principles

Per `.specify/memory/constitution.md`:

1. **ARM Memory Optimization**: Cache-aligned (64B), memory pools, no dynamic alloc in hot path
2. **Real-Time Performance**: <10ms p99 latency, lock-free queues
3. **Automotive Reliability**: Defensive coding, watchdog integration, fail-safe
4. **Suricata Compatibility**: 7.x rule format, hot reload support
5. **Android NDK Integration**: arm64-v8a primary, NDK for cross-compile only (native daemon)
6. **Power Efficiency**: <5% idle drain, adaptive processing
7. **Security-First**: Input validation, Rust for new code, approved crypto only
8. **Testing Discipline**: 80% unit coverage, fuzz all parsers, HIL validation

## GPL Compliance

- Suricata runs as **separate process** (fork/exec by vnidsd)
- Communication via **Unix domain sockets only**
- No static or dynamic linking between GPL and proprietary code
- All Suricata modifications released under GPL

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    vnidsd (Rust daemon)                      │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌─────────────┐  │
│  │ Watchdog │  │  Events  │  │  Rules   │  │  API/CLI    │  │
│  │          │  │  Queue   │  │  Loader  │  │  Interface  │  │
│  └────┬─────┘  └────┬─────┘  └────┬─────┘  └──────┬──────┘  │
│       │             │             │               │          │
│       └─────────────┴──────┬──────┴───────────────┘          │
│                            │ Unix Socket IPC                 │
└────────────────────────────┼─────────────────────────────────┘
                             │
         Process Boundary    │    (fork/exec)
                             │
┌────────────────────────────┴─────────────────────────────────┐
│                  Suricata 7.x (GPL)                           │
│  ┌──────────────┐  ┌──────────────┐  ┌────────────────────┐  │
│  │   Capture    │  │    Detect    │  │   EVE Output       │  │
│  │  (AF_PACKET) │  │  (Rules+DPI) │  │   (Unix Socket)    │  │
│  └──────────────┘  └──────────────┘  └────────────────────┘  │
└──────────────────────────────────────────────────────────────┘
```

## Deployment Platforms

| Platform | Init System | Install Path | Config Path |
|----------|-------------|--------------|-------------|
| Android 12+ | init.rc | /system/bin/vnidsd | /data/vnids/ |
| Yocto Linux | systemd | /usr/bin/vnidsd | /etc/vnids/ |
| Buildroot | BusyBox init | /usr/bin/vnidsd | /etc/vnids/ |

## Recent Changes

- 001-suricata-ids: Changed from Android Application to native daemon (vnidsd)

<!-- MANUAL ADDITIONS START -->
<!-- MANUAL ADDITIONS END -->
