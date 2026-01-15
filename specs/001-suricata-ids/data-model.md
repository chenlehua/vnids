# Data Model: Suricata-based Vehicle Network IDS

**Feature**: 001-suricata-ids
**Date**: 2026-01-15
**Status**: Complete

## Entity Relationship Diagram

```
┌─────────────────┐       ┌─────────────────┐       ┌─────────────────┐
│    Ruleset      │       │ DetectionRule   │       │ RuleCategory    │
├─────────────────┤       ├─────────────────┤       ├─────────────────┤
│ id: UUID        │──1:N──│ id: UUID        │──N:1──│ id: String      │
│ version: String │       │ sid: Integer    │       │ name: String    │
│ created_at: TS  │       │ gid: Integer    │       │ description: Str│
│ status: Enum    │       │ rev: Integer    │       └─────────────────┘
│ rule_count: Int │       │ message: String │
│ checksum: String│       │ severity: Enum  │
└─────────────────┘       │ enabled: Boolean│
                          │ content: Text   │
                          │ category_id: FK │
                          │ ruleset_id: FK  │
                          └─────────────────┘
                                   │
                                   │ triggers
                                   ▼
┌─────────────────┐       ┌─────────────────┐       ┌─────────────────┐
│ ProtocolSession │       │ SecurityEvent   │       │  PacketMetadata │
├─────────────────┤       ├─────────────────┤       ├─────────────────┤
│ id: UUID        │──1:N──│ id: UUID        │──1:1──│ id: UUID        │
│ protocol: Enum  │       │ timestamp: TS   │       │ capture_ts: TS  │
│ src_addr: String│       │ event_type: Enum│       │ interface_id: St│
│ src_port: Int   │       │ severity: Enum  │       │ eth_src: String │
│ dst_addr: String│       │ src_addr: String│       │ eth_dst: String │
│ dst_port: Int   │       │ src_port: Int   │       │ ip_proto: Int   │
│ start_time: TS  │       │ dst_addr: String│       │ payload_len: Int│
│ packet_count:Int│       │ dst_port: Int   │       │ payload_hash:Str│
│ bytes_count: Int│       │ protocol: Enum  │       │ event_id: FK    │
│ state: Enum     │       │ rule_sid: Int   │       └─────────────────┘
└─────────────────┘       │ rule_gid: Int   │
                          │ message: String │
                          │ metadata: JSON  │
                          │ session_id: FK  │
                          │ exported: Bool  │
                          └─────────────────┘
```

## Entities

### 1. Ruleset

A versioned collection of detection rules loaded into Suricata.

| Field | Type | Constraints | Description |
|-------|------|-------------|-------------|
| id | UUID | PK, NOT NULL | Unique ruleset identifier |
| version | String(32) | NOT NULL | Semantic version (e.g., "1.2.3") |
| created_at | Timestamp | NOT NULL | When ruleset was created |
| activated_at | Timestamp | NULLABLE | When ruleset was activated |
| status | Enum | NOT NULL | pending, active, archived, failed |
| rule_count | Integer | NOT NULL, >= 0 | Number of rules in set |
| checksum | String(64) | NOT NULL | SHA-256 of concatenated rules |
| source | Enum | NOT NULL | builtin, custom, remote |
| description | String(256) | NULLABLE | Human-readable description |

**Validation Rules**:
- Version must follow semantic versioning pattern
- Only one ruleset can have status = active at a time
- Checksum must match actual content on load

### 2. DetectionRule

Individual Suricata rule for threat detection.

| Field | Type | Constraints | Description |
|-------|------|-------------|-------------|
| id | UUID | PK, NOT NULL | Internal identifier |
| sid | Integer | NOT NULL, UNIQUE | Suricata Signature ID |
| gid | Integer | NOT NULL, DEFAULT 1 | Generator ID |
| rev | Integer | NOT NULL, >= 1 | Rule revision |
| message | String(256) | NOT NULL | Alert message |
| severity | Enum | NOT NULL | critical, high, medium, low, info |
| enabled | Boolean | NOT NULL, DEFAULT true | Whether rule is active |
| content | Text | NOT NULL | Full Suricata rule text |
| category_id | String(64) | FK, NOT NULL | Reference to RuleCategory |
| ruleset_id | UUID | FK, NOT NULL | Reference to parent Ruleset |
| created_at | Timestamp | NOT NULL | When rule was added |
| hit_count | Integer | NOT NULL, DEFAULT 0 | Number of times triggered |
| last_hit_at | Timestamp | NULLABLE | Most recent trigger time |

**Validation Rules**:
- SID must be unique within the system
- SID ranges: 1-999999 (ET rules), 1000000-1999999 (custom), 2000000+ (local)
- Content must pass Suricata syntax validation

### 3. RuleCategory

Classification of rules by attack type or protocol.

| Field | Type | Constraints | Description |
|-------|------|-------------|-------------|
| id | String(64) | PK, NOT NULL | Category identifier (slug) |
| name | String(128) | NOT NULL | Display name |
| description | String(512) | NULLABLE | Detailed description |
| parent_id | String(64) | FK, NULLABLE | Parent category for hierarchy |
| priority | Integer | NOT NULL, DEFAULT 0 | Display order |

**Built-in Categories**:
- `flood-attacks` - TCP/UDP/ICMP flood detection
- `malformed-packets` - Protocol violation detection
- `automotive-someip` - SOME/IP protocol anomalies
- `automotive-doip` - DoIP diagnostic attacks
- `automotive-gbt32960` - GB/T 32960.3 monitoring
- `weak-credentials` - Telnet/FTP weak password
- `vulnerability` - CVE-based detection
- `miscellaneous` - Other detection rules

### 4. SecurityEvent

A detected security incident from Suricata.

| Field | Type | Constraints | Description |
|-------|------|-------------|-------------|
| id | UUID | PK, NOT NULL | Unique event identifier |
| timestamp | Timestamp | NOT NULL, INDEXED | When event was detected |
| event_type | Enum | NOT NULL | alert, anomaly, flow, stats |
| severity | Enum | NOT NULL | critical, high, medium, low, info |
| src_addr | String(45) | NOT NULL | Source IP (IPv4 or IPv6) |
| src_port | Integer | NULLABLE, 0-65535 | Source port |
| dst_addr | String(45) | NOT NULL | Destination IP |
| dst_port | Integer | NULLABLE, 0-65535 | Destination port |
| protocol | Enum | NOT NULL | tcp, udp, icmp, someip, doip, etc. |
| rule_sid | Integer | NULLABLE | Matched rule SID |
| rule_gid | Integer | NULLABLE | Matched rule GID |
| message | String(256) | NOT NULL | Alert message |
| metadata | JSON | NOT NULL | Protocol-specific context |
| session_id | UUID | FK, NULLABLE | Related protocol session |
| packet_id | UUID | FK, NULLABLE | Related packet metadata |
| exported | Boolean | NOT NULL, DEFAULT false | Sent to external SIEM |
| exported_at | Timestamp | NULLABLE | When exported |

**Metadata JSON Structure** (varies by protocol):
```json
// For SOME/IP
{
  "service_id": 4660,
  "method_id": 32769,
  "client_id": 256,
  "session_id": 1,
  "message_type": "REQUEST"
}

// For DoIP
{
  "payload_type": "0x8001",
  "source_address": "0x0E80",
  "target_address": "0x1010",
  "uds_service": "0x10"
}

// For flood attack
{
  "packet_count": 15420,
  "duration_ms": 1000,
  "pps_rate": 15420,
  "threshold_exceeded": true
}
```

**Indexes**:
- `idx_events_timestamp` on (timestamp DESC)
- `idx_events_severity` on (severity, timestamp DESC)
- `idx_events_src_addr` on (src_addr, timestamp DESC)
- `idx_events_rule_sid` on (rule_sid)
- `idx_events_exported` on (exported) WHERE exported = false

### 5. ProtocolSession

Tracked network session for stateful analysis.

| Field | Type | Constraints | Description |
|-------|------|-------------|-------------|
| id | UUID | PK, NOT NULL | Unique session identifier |
| protocol | Enum | NOT NULL | tcp, udp, someip, doip, etc. |
| src_addr | String(45) | NOT NULL | Source IP |
| src_port | Integer | NOT NULL, 0-65535 | Source port |
| dst_addr | String(45) | NOT NULL | Destination IP |
| dst_port | Integer | NOT NULL, 0-65535 | Destination port |
| start_time | Timestamp | NOT NULL | Session start |
| end_time | Timestamp | NULLABLE | Session end |
| packet_count | Integer | NOT NULL, DEFAULT 0 | Packets in session |
| bytes_in | Integer | NOT NULL, DEFAULT 0 | Bytes received |
| bytes_out | Integer | NOT NULL, DEFAULT 0 | Bytes sent |
| state | Enum | NOT NULL | new, established, closed, timeout |
| app_proto | String(32) | NULLABLE | Detected application protocol |

**Indexes**:
- `idx_sessions_active` on (state) WHERE state IN ('new', 'established')
- `idx_sessions_flow` on (src_addr, dst_addr, src_port, dst_port)

### 6. PacketMetadata

Extracted metadata from captured packets.

| Field | Type | Constraints | Description |
|-------|------|-------------|-------------|
| id | UUID | PK, NOT NULL | Unique packet identifier |
| capture_ts | Timestamp | NOT NULL | Capture timestamp (microsecond precision) |
| interface_id | String(32) | NOT NULL | Capture interface name |
| eth_src | String(17) | NULLABLE | Source MAC (if available) |
| eth_dst | String(17) | NULLABLE | Destination MAC |
| vlan_id | Integer | NULLABLE | VLAN tag if present |
| ip_version | Integer | NOT NULL | 4 or 6 |
| ip_proto | Integer | NOT NULL | IP protocol number |
| payload_len | Integer | NOT NULL, >= 0 | Payload length in bytes |
| payload_hash | String(64) | NULLABLE | SHA-256 of first 256 bytes |
| event_id | UUID | FK, UNIQUE | Associated security event |

**Notes**:
- Packet payload is NOT stored (only hash for deduplication)
- Metadata retained for forensic analysis
- Retention policy: 7 days by default

## Enumerations

### Severity
```
critical  = 1  # Immediate action required, active attack
high      = 2  # Significant threat, urgent attention
medium    = 3  # Moderate concern, scheduled review
low       = 4  # Minor issue, informational
info      = 5  # Normal activity logged for analysis
```

### EventType
```
alert    # Rule-triggered detection
anomaly  # Statistical anomaly detection
flow     # Flow-based event (session start/end)
stats    # Engine statistics
```

### Protocol
```
# Transport
tcp, udp, icmp, igmp

# Automotive
someip, doip, gbt32960

# Application
http, tls, dns, mqtt, ftp, telnet

# Other
unknown
```

### RulesetStatus
```
pending   # Uploaded, not validated
active    # Currently in use
archived  # Previously used, kept for history
failed    # Validation failed
```

### SessionState
```
new         # First packet seen
established # Bidirectional traffic
closed      # Graceful close
timeout     # Timed out
```

## State Transitions

### Ruleset Status
```
pending ──┬──[validation passes]──→ active
          │
          └──[validation fails]───→ failed

active ───[new ruleset activated]──→ archived
```

### Session State
```
     ┌────────────────────────────────────────┐
     │                                        │
     ▼                                        │
   new ──[bidirectional traffic]──→ established
     │                                   │
     │                                   │
     └──[timeout]──→ timeout ←──[timeout]┘
                         ▲
                         │
     established ──[FIN/RST]──→ closed
```

## Storage Considerations

### SQLite Schema (Android)
- Single database file: `/data/data/com.vnids/databases/vnids.db`
- WAL mode enabled for concurrent reads during writes
- Auto-vacuum enabled
- Event retention: 7 days (configurable)
- Max database size: 500MB (configurable)

### Indexes for Performance
- All foreign keys indexed
- Timestamp-based queries optimized
- Composite indexes for common query patterns

### Data Retention Policy
| Entity | Retention | Cleanup |
|--------|-----------|---------|
| SecurityEvent | 7 days | Daily job |
| PacketMetadata | 7 days | With event |
| ProtocolSession | 1 day | Hourly job |
| Ruleset (archived) | 30 days | Weekly job |
| DetectionRule | Forever | With ruleset |
