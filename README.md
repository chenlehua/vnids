# VNIDS - Vehicle Network Intrusion Detection System

A Suricata-based network intrusion detection system designed for automotive
networks, with support for automotive protocols (SOME/IP, DoIP, GB/T 32960.3).

## Overview

VNIDS provides real-time network-based intrusion detection for vehicle networks:

- **Deep packet inspection** using Suricata engine
- **Automotive protocol support** (SOME/IP, DoIP, GB/T 32960.3)
- **Network flood detection** (SYN, UDP, ICMP floods)
- **Anomaly detection** for suspicious network behavior
- **SQLite event storage** for alert persistence
- **CLI tool** for monitoring and control

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                        vnidsd daemon                         │
├─────────────┬─────────────┬──────────────┬─────────────────┤
│  Watchdog   │  EVE Reader │    Event     │   API Server    │
│  (Suricata  │  (JSON      │   Handler    │   (Unix         │
│   process)  │   parser)   │   (Storage)  │    socket)      │
└──────┬──────┴──────┬──────┴──────┬───────┴────────┬────────┘
       │             │             │                │
       ▼             │             ▼                ▼
┌──────────────┐     │     ┌─────────────┐  ┌─────────────┐
│   Suricata   │     │     │   SQLite    │  │  vnids-cli  │
│  (separate   │◄────┘     │   Database  │  │    (CLI)    │
│   process)   │           └─────────────┘  └─────────────┘
└──────────────┘
       │
       ▼
   Network Interface
```

## Requirements

- Linux (ARM64 or x86_64)
- Suricata 7.0+
- SQLite 3.x
- cJSON library

### Platform Support

- **Android 12+** (API level 31+)
- **Yocto Linux** (Kirkstone, Scarthgap)
- **Buildroot**

## Building

### Standard Linux Build

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
```

### Android Build (AOSP)

```bash
# Copy to AOSP tree
cp -r vnids ${AOSP_ROOT}/vendor/vnids

# Build with Android build system
cd ${AOSP_ROOT}
source build/envsetup.sh
lunch <target>
m vnidsd vnids-cli
```

### Yocto Build

Add the meta-vnids layer to your Yocto build:

```bash
# In your Yocto build directory
bitbake-layers add-layer /path/to/meta-vnids
bitbake vnids
```

## Installation

### Quick Install

```bash
sudo ./deploy/common/install.sh
```

### Manual Installation

1. Copy binaries to `/usr/local/bin/`
2. Copy configuration to `/etc/vnids/`
3. Copy rules to `/etc/vnids/rules/`
4. Create runtime directories
5. Enable systemd service

## Configuration

Main configuration file: `/etc/vnids/vnidsd.conf`

```ini
[general]
log_level = info

[suricata]
binary = /usr/bin/suricata
config = /etc/vnids/suricata/suricata.yaml
rules_dir = /etc/vnids/rules
interfaces = eth0

[storage]
database = /var/lib/vnids/events.db
max_events = 100000
```

## Usage

### Starting the Daemon

```bash
# Using systemd
sudo systemctl start vnidsd

# Manual start
sudo vnidsd -c /etc/vnids/vnidsd.conf
```

### CLI Commands

```bash
# Check status
vnids-cli status

# View statistics
vnids-cli stats

# List recent events
vnids-cli events --limit 20

# Reload detection rules
vnids-cli reload

# Shutdown daemon
vnids-cli shutdown
```

## Detection Rules

VNIDS includes baseline detection rules:

- **vnids-baseline.rules** - Core network security rules
- **vnids-someip.rules** - SOME/IP protocol security
- **vnids-doip.rules** - DoIP protocol security
- **vnids-flood.rules** - Network flood detection
- **vnids-anomaly.rules** - Anomaly detection

### Custom Rules

Add custom rules to `/etc/vnids/rules/` and reload:

```bash
vnids-cli reload
```

## License

Apache License 2.0

Copyright (c) 2026 VNIDS Authors

Note: Suricata is licensed under GPLv2. VNIDS isolates Suricata in a separate
process and communicates via Unix sockets to maintain license separation.
