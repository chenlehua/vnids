# Feature Specification: Suricata-based Vehicle Network IDS with DPI

**Feature Branch**: `001-suricata-ids`
**Created**: 2026-01-15
**Status**: Draft
**Input**: User description: "基于Suricata开发网络入侵检测及深度包检测功能，支持跑在车载ARMv8架构、操作系统Android12+平台"

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Basic Ethernet Intrusion Detection (Priority: P1)

As a vehicle security system, I need to monitor Ethernet traffic and detect common network attacks in real-time, generating security event logs for analysis and response.

**Why this priority**: This is the core IDS functionality. Without basic intrusion detection, no other features provide value. Detecting network attacks like flood attacks and malformed packets is the fundamental capability of an IDS.

**Independent Test**: Can be fully tested by sending known attack traffic patterns (SYN flood, malformed packets) through a test network interface and verifying that security event logs are generated with correct classification.

**Acceptance Scenarios**:

1. **Given** the IDS engine is running and monitoring an Ethernet interface, **When** a TCP SYN flood attack pattern is detected (threshold: >1000 SYN packets/sec from single source), **Then** a security event log is generated within 10ms containing event type, source IP, timestamp, and severity level.

2. **Given** the IDS engine is running, **When** a malformed TCP packet (invalid header flags, incorrect checksum) is received, **Then** a security event log is generated identifying the specific malformation type.

3. **Given** the IDS engine is running, **When** a TCP Land attack is detected (source IP equals destination IP), **Then** a high-severity security event is logged immediately.

4. **Given** the IDS engine is running, **When** an ICMP flood attack exceeds threshold, **Then** the event is logged with packet count and source identification.

---

### User Story 2 - Deep Packet Inspection for Automotive Protocols (Priority: P2)

As a vehicle security system, I need to perform deep packet inspection on automotive-specific protocols (SOME/IP, DoIP) to detect protocol-level anomalies and policy violations.

**Why this priority**: Automotive protocols are vehicle-specific attack vectors. After basic network protection is established (P1), protecting automotive communication protocols is the next critical layer.

**Independent Test**: Can be fully tested by replaying captured SOME/IP and DoIP traffic (both legitimate and malicious samples) and verifying that protocol parsing, rule matching, and event generation work correctly.

**Acceptance Scenarios**:

1. **Given** the DPI engine is processing SOME/IP traffic, **When** a SOME/IP message matches a configured detection rule (e.g., unauthorized service ID access), **Then** a security event is logged with protocol-specific details (service ID, method ID, client ID).

2. **Given** the DPI engine is processing DoIP traffic, **When** anomalous DoIP behavior is detected (e.g., unexpected diagnostic session, unauthorized ECU access attempt), **Then** a security event is logged with DoIP-specific context.

3. **Given** a custom detection rule is configured for SOME/IP field values, **When** traffic matching the rule is detected, **Then** the event log contains the matched field values and rule identifier.

---

### User Story 3 - Standard Protocol DPI (Priority: P3)

As a vehicle security system, I need to perform deep packet inspection on standard IT protocols (HTTP, TLS, DNS, MQTT) to detect application-layer threats and policy violations.

**Why this priority**: Standard protocols may carry threats from connected services. After automotive protocol protection (P2), general IT protocol inspection extends coverage to internet-connected vehicle features.

**Independent Test**: Can be fully tested by generating HTTP/DNS/MQTT traffic with known malicious patterns and verifying detection and logging.

**Acceptance Scenarios**:

1. **Given** the DPI engine is inspecting HTTP traffic, **When** a suspicious HTTP request matches a detection rule (e.g., SQL injection pattern, malicious user-agent), **Then** a security event is logged with HTTP method, URI, and matched pattern.

2. **Given** the DPI engine is inspecting DNS traffic, **When** a DNS query to a known malicious domain is detected, **Then** a security event is logged with the queried domain and response.

3. **Given** the DPI engine is inspecting MQTT traffic, **When** unauthorized topic subscription or publish is detected, **Then** a security event is logged with topic, client ID, and action type.

---

### User Story 4 - Rule Management and Hot Reload (Priority: P4)

As a security operations team, I need to update detection rules without stopping the IDS engine, ensuring continuous protection during rule updates.

**Why this priority**: Operational flexibility is important but not critical for initial detection capability. Hot reload enables production maintenance without security gaps.

**Independent Test**: Can be fully tested by updating a rule file while traffic is being monitored and verifying that new rules take effect without packet loss or service interruption.

**Acceptance Scenarios**:

1. **Given** the IDS engine is running with ruleset version A, **When** a new ruleset version B is deployed via the update mechanism, **Then** new rules take effect within 5 seconds without packet drops.

2. **Given** a rule update is in progress, **When** attack traffic matching a rule in both versions is received, **Then** the attack is detected continuously without gaps.

3. **Given** an invalid ruleset is pushed, **When** the engine attempts to load it, **Then** the engine continues with the previous valid ruleset and logs an error event.

---

### User Story 5 - GPL-Compliant Process Isolation (Priority: P5)

As a vehicle OEM, I need the architecture to isolate GPL-licensed Suricata components from proprietary application code, ensuring open-source license compliance.

**Why this priority**: License compliance is a legal requirement but does not affect detection functionality. This architectural concern is addressed through design rather than feature implementation.

**Independent Test**: Can be verified by architectural review confirming that Suricata runs as a separate process communicating only via IPC (Unix socket/shared memory), with no static or dynamic linking to proprietary code.

**Acceptance Scenarios**:

1. **Given** the system architecture, **When** the IDS engine (Suricata) is inspected, **Then** it runs as an independent process with no shared libraries linking to proprietary code.

2. **Given** the system is running, **When** the communication between Suricata and the proprietary application is analyzed, **Then** only IPC mechanisms (Unix sockets, named pipes, or shared memory) are used.

3. **Given** the source code distribution, **When** GPL compliance is audited, **Then** all Suricata modifications (if any) are available under GPL, while proprietary code remains separate.

---

### Edge Cases

- What happens when network interface becomes unavailable during monitoring?
  - System logs interface down event and attempts to reconnect with exponential backoff
- What happens when event log storage is full?
  - Oldest logs are rotated out; critical events are prioritized for retention
- How does system handle malformed rules in a ruleset update?
  - Invalid rules are skipped with warnings logged; valid rules in the same file are still loaded
- What happens when packet volume exceeds processing capacity?
  - System enters degraded mode, sampling packets and logging capacity warning
- How does system handle Suricata process crash?
  - Watchdog detects crash within 1 second and restarts Suricata process; gap in monitoring is logged

## Requirements *(mandatory)*

### Functional Requirements

#### Core IDS Engine

- **FR-001**: System MUST parse network packets from Layer 2 (Ethernet) through Layer 7 (Application)
- **FR-002**: System MUST detect and log the following attack types:
  - TCP Land attack
  - TCP ACK flood attack
  - TCP malformed packet attack
  - TCP SYN flood attack
  - UDP flood attack
  - UDP malformed packet attack
  - ICMP flood attack
  - Large ping (Ping of Death) attack
  - IGMP flood attack
- **FR-003**: System MUST generate security event logs containing: timestamp, event type, severity, source/destination addresses, matched rule ID, and protocol-specific context
- **FR-004**: System MUST process packets with end-to-end latency under 10ms at p99 under normal load

#### Deep Packet Inspection

- **FR-005**: System MUST support DPI for the following automotive protocols:
  - SOME/IP (Service-Oriented Middleware over IP)
  - DoIP (Diagnostics over IP)
  - GB/T 32960.3 (Chinese national EV communication standard)
- **FR-006**: System MUST support DPI for the following standard protocols:
  - HTTP/HTTPS (TLS inspection where keys available)
  - DNS
  - MQTT
  - Telnet
  - FTP
- **FR-007**: System MUST detect weak password usage in Telnet and FTP protocols
- **FR-008**: System MUST support vulnerability signature matching for known CVEs

#### Rule Management

- **FR-009**: System MUST include built-in baseline rules for all supported attack types
- **FR-010**: System MUST support Suricata-compatible rule format for custom rules
- **FR-011**: System MUST support hot reload of rules without service interruption
- **FR-012**: System MUST validate rule syntax before applying updates
- **FR-013**: System MUST fall back to previous valid ruleset if new rules fail validation

#### Platform Requirements

- **FR-014**: System MUST run on ARMv8 (AArch64) architecture
- **FR-015**: System MUST run on Android 12 or later
- **FR-016**: System MUST operate within 64MB memory budget for core engine
- **FR-017**: System MUST support arm64-v8a Android ABI

#### Architecture Requirements

- **FR-018**: System MUST isolate GPL-licensed components (Suricata) in a separate process
- **FR-019**: System MUST use only IPC mechanisms (Unix sockets, shared memory) for communication between GPL and proprietary components
- **FR-020**: System MUST NOT statically or dynamically link GPL code with proprietary code

### Key Entities

- **SecurityEvent**: A detected security incident - contains event ID, timestamp, event type, severity (critical/high/medium/low/info), source and destination addresses, matched rule ID, protocol type, and protocol-specific metadata
- **DetectionRule**: A pattern or condition for detecting threats - contains rule ID, rule category (network/protocol/vulnerability), match conditions, severity level, and enabled status
- **Ruleset**: A versioned collection of detection rules - contains version identifier, creation timestamp, rule count, and validation status
- **ProtocolSession**: A tracked communication session - contains session ID, protocol type, start time, packet count, and associated security events
- **PacketMetadata**: Information extracted from network packets - contains capture timestamp, interface ID, layer 2-7 headers, and payload hash

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: System detects 99% of known attack patterns from test suite within 10ms of packet arrival
- **SC-002**: System generates zero false positive alerts for baseline legitimate traffic patterns
- **SC-003**: System processes 100 Mbps sustained traffic without packet drops
- **SC-004**: Rule hot reload completes within 5 seconds with zero detection gaps
- **SC-005**: System operates continuously for 30 days without memory leaks or crashes requiring restart
- **SC-006**: System passes GPL license compliance audit with clear component separation
- **SC-007**: Security event logs are queryable within 1 second of generation
- **SC-008**: System recovers from process crash within 3 seconds via watchdog
- **SC-009**: Built-in rules detect all 20+ specified attack categories without custom configuration
- **SC-010**: System battery consumption in idle monitoring state is under 5% per hour on reference hardware

## Assumptions

- Android device has root access or VPN service capability for packet capture
- Network interface provides raw packet access via standard Android/Linux APIs
- Suricata 7.x can be cross-compiled for Android ARM64 target
- Vehicle network traffic volumes typically stay below 100 Mbps sustained
- Security event storage capacity is at least 1GB for log retention
- Device has at least 256MB total RAM available (64MB for IDS engine)
