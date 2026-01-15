# IPC Protocol: Suricata ↔ vnidsd

**Feature**: 001-suricata-ids
**Date**: 2026-01-15
**Version**: 1.0.0

## Overview

This document defines the inter-process communication protocol between the Suricata IDS engine (GPL process) and the vnidsd daemon (proprietary process). The protocol uses Unix domain sockets with JSON-formatted messages.

## Transport Layer

### Socket Paths

| Socket | Path | Purpose |
|--------|------|---------|
| Event | `/var/run/vnids/events.sock` | Suricata → vnidsd (events) |
| Control | `/var/run/vnids/control.sock` | vnidsd → Suricata (commands) |
| Stats | `/var/run/vnids/stats.sock` | Suricata → vnidsd (metrics) |
| API | `/var/run/vnids/api.sock` | CLI → vnidsd (management) |

### Socket Configuration

```
Type:           SOCK_STREAM (TCP-like reliable stream)
Buffer Size:    65536 bytes
Backlog:        5 connections
Permissions:    0660 (owner/group read/write)
Owner:          root:vnids
```

### Connection Lifecycle

```
vnidsd                               Suricata
 │                                      │
 │──────[create sockets]────────────────│
 │                                      │
 │──────[fork/exec suricata]───────────►│
 │                                      │
 │◄─────[EVENT: connected]──────────────│
 │                                      │
 │◄─────[EVENT: alert/flow/stats]───────│
 │         (continuous stream)          │
 │                                      │
 │──────[CTRL: reload_rules]───────────►│
 │◄─────[CTRL: ack/error]───────────────│
 │                                      │
 │──────[CTRL: shutdown]───────────────►│
 │                                      │
 │◄─────[EVENT: disconnected]───────────│
 │                                      │
```

## Message Format

### General Structure

All messages are newline-delimited JSON (NDJSON). Each message ends with `\n`.

```json
{
  "timestamp": "2026-01-15T10:30:45.123456Z",
  "message_type": "event|control|stats|ack|error",
  "payload": { ... }
}
```

### Timestamp Format

ISO 8601 with microsecond precision: `YYYY-MM-DDTHH:mm:ss.ffffffZ`

## Event Messages (Suricata → App)

### Event Types

| Type | Description |
|------|-------------|
| `alert` | Rule-triggered security event |
| `flow` | Session start/end |
| `stats` | Engine statistics |
| `heartbeat` | Connection keepalive |

### Alert Event

Sent when a detection rule matches.

```json
{
  "timestamp": "2026-01-15T10:30:45.123456Z",
  "message_type": "event",
  "payload": {
    "event_type": "alert",
    "signature_id": 2000001,
    "gid": 1,
    "rev": 1,
    "signature": "DoIP routing activation from external",
    "severity": 2,
    "category": "automotive-doip",
    "src_ip": "192.168.1.100",
    "src_port": 49152,
    "dest_ip": "192.168.1.10",
    "dest_port": 13400,
    "proto": "TCP",
    "app_proto": "doip",
    "flow_id": 1234567890,
    "metadata": {
      "doip": {
        "payload_type": "0x0005",
        "source_address": "0x0E80"
      }
    },
    "packet_info": {
      "linktype": 1,
      "length": 128,
      "hash": "a1b2c3d4..."
    }
  }
}
```

### Flow Event

Sent when a session starts, updates, or ends.

```json
{
  "timestamp": "2026-01-15T10:30:45.123456Z",
  "message_type": "event",
  "payload": {
    "event_type": "flow",
    "flow_id": 1234567890,
    "state": "new|established|closed",
    "src_ip": "192.168.1.100",
    "src_port": 49152,
    "dest_ip": "192.168.1.10",
    "dest_port": 30490,
    "proto": "UDP",
    "app_proto": "someip",
    "pkts_toserver": 15,
    "pkts_toclient": 12,
    "bytes_toserver": 1500,
    "bytes_toclient": 2400,
    "start": "2026-01-15T10:30:40.000000Z",
    "end": "2026-01-15T10:30:45.123456Z"
  }
}
```

### Heartbeat Event

Sent every 5 seconds to confirm connection is alive.

```json
{
  "timestamp": "2026-01-15T10:30:45.123456Z",
  "message_type": "event",
  "payload": {
    "event_type": "heartbeat",
    "uptime_seconds": 3600,
    "rules_loaded": 127,
    "memory_used_mb": 58
  }
}
```

## Control Messages (App → Suricata)

### Control Commands

| Command | Description |
|---------|-------------|
| `reload_rules` | Trigger rule hot reload |
| `get_stats` | Request current statistics |
| `set_config` | Update runtime configuration |
| `shutdown` | Graceful shutdown |

### Reload Rules Command

```json
{
  "timestamp": "2026-01-15T10:30:45.123456Z",
  "message_type": "control",
  "payload": {
    "command": "reload_rules",
    "request_id": "req-12345",
    "params": {
      "rules_path": "/data/data/com.vnids/rules/custom/"
    }
  }
}
```

### Get Stats Command

```json
{
  "timestamp": "2026-01-15T10:30:45.123456Z",
  "message_type": "control",
  "payload": {
    "command": "get_stats",
    "request_id": "req-12346"
  }
}
```

### Set Config Command

```json
{
  "timestamp": "2026-01-15T10:30:45.123456Z",
  "message_type": "control",
  "payload": {
    "command": "set_config",
    "request_id": "req-12347",
    "params": {
      "outputs.eve-log.enabled": true,
      "detect.profile": "high"
    }
  }
}
```

### Shutdown Command

```json
{
  "timestamp": "2026-01-15T10:30:45.123456Z",
  "message_type": "control",
  "payload": {
    "command": "shutdown",
    "request_id": "req-12348",
    "params": {
      "graceful": true,
      "timeout_seconds": 30
    }
  }
}
```

## Response Messages (Suricata → App)

### Acknowledgment

Sent in response to successful control commands.

```json
{
  "timestamp": "2026-01-15T10:30:45.200000Z",
  "message_type": "ack",
  "payload": {
    "request_id": "req-12345",
    "command": "reload_rules",
    "status": "success",
    "details": {
      "rules_loaded": 135,
      "rules_failed": 2,
      "reload_time_ms": 450
    }
  }
}
```

### Error

Sent when a command fails.

```json
{
  "timestamp": "2026-01-15T10:30:45.200000Z",
  "message_type": "error",
  "payload": {
    "request_id": "req-12347",
    "command": "set_config",
    "error_code": "INVALID_CONFIG_KEY",
    "error_message": "Unknown configuration key: detect.profile",
    "recoverable": true
  }
}
```

### Error Codes

| Code | Description | Recoverable |
|------|-------------|-------------|
| `INVALID_COMMAND` | Unknown command type | Yes |
| `INVALID_PARAMS` | Missing or invalid parameters | Yes |
| `INVALID_CONFIG_KEY` | Unknown config key | Yes |
| `RULE_PARSE_ERROR` | Rule syntax error | Yes |
| `RESOURCE_EXHAUSTED` | Memory or file limit | No |
| `INTERNAL_ERROR` | Unexpected engine error | No |
| `SHUTDOWN_IN_PROGRESS` | Already shutting down | No |

## Statistics Messages

### Full Statistics

Sent in response to `get_stats` or periodically (every 60 seconds).

```json
{
  "timestamp": "2026-01-15T10:30:45.123456Z",
  "message_type": "stats",
  "payload": {
    "uptime_seconds": 3600,
    "capture": {
      "packets": 1500000,
      "bytes": 750000000,
      "dropped": 0,
      "errors": 0
    },
    "detect": {
      "alerts": 127,
      "rules_loaded": 135,
      "rules_failed": 2
    },
    "flow": {
      "active": 45,
      "total": 12500,
      "tcp": 8000,
      "udp": 4500
    },
    "app_layer": {
      "someip": { "tx": 5000, "rx": 4800 },
      "doip": { "tx": 200, "rx": 180 },
      "http": { "tx": 1500, "rx": 1400 },
      "dns": { "tx": 3000, "rx": 2950 }
    },
    "memory": {
      "used_mb": 58,
      "limit_mb": 64,
      "pools": {
        "packet": { "allocated": 4096, "used": 128 },
        "flow": { "allocated": 8192, "used": 45 }
      }
    },
    "performance": {
      "avg_latency_us": 450,
      "p99_latency_us": 2500,
      "pps": 15000
    }
  }
}
```

## Protocol Guarantees

### Ordering
- Events are delivered in capture order
- Control responses reference request_id for correlation

### Reliability
- TCP-like reliable delivery within socket session
- No built-in message acknowledgment for events (fire-and-forget)
- Control commands always receive ack or error response

### Timeout
- Control command timeout: 30 seconds
- Heartbeat interval: 5 seconds
- Connection considered dead if no heartbeat for 15 seconds

### Backpressure
- If event socket buffer fills, oldest events are dropped
- Drop count included in next stats message
- App should read events promptly to avoid drops

## Versioning

The protocol version is exchanged in the first heartbeat after connection:

```json
{
  "timestamp": "2026-01-15T10:30:45.123456Z",
  "message_type": "event",
  "payload": {
    "event_type": "heartbeat",
    "protocol_version": "1.0.0",
    "suricata_version": "7.0.3",
    "uptime_seconds": 0,
    "rules_loaded": 0,
    "memory_used_mb": 12
  }
}
```

### Version Compatibility
- Major version change: Breaking changes, requires app update
- Minor version change: New optional fields, backward compatible
- Patch version change: Bug fixes only
