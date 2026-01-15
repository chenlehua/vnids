# Implementation Tasks: Suricata-based Vehicle Network IDS

**Feature**: 001-suricata-ids
**Branch**: `001-suricata-ids`
**Generated**: 2026-01-15
**Spec**: [spec.md](./spec.md) | **Plan**: [plan.md](./plan.md)

## Task Format

```
- [ ] [TaskID] [Priority] [Story] Description `path/to/file.c`
```

- **TaskID**: Unique identifier (e.g., T001)
- **Priority**: P1 (Critical) > P2 (High) > P3 (Medium) > P4 (Low)
- **Story**: US1-US5 or SETUP/FOUND/POLISH

---

## Phase 1: Project Setup

### 1.1 Repository Structure

- [ ] [T001] [P1] [SETUP] Create root Makefile with build targets for vnidsd, vnids-cli, suricata `Makefile`
- [ ] [T002] [P1] [SETUP] Create vnidsd directory structure (src/, include/, tests/) `vnidsd/`
- [ ] [T003] [P1] [SETUP] Create vnids-cli directory structure (src/, include/) `vnids-cli/`
- [ ] [T004] [P1] [SETUP] Create CMakeLists.txt for cross-compilation support `CMakeLists.txt`
- [ ] [T005] [P2] [SETUP] Create shared/ directory for common headers and IPC definitions `shared/include/`
- [ ] [T006] [P2] [SETUP] Create deploy/ directory structure for platform init scripts `deploy/`

### 1.2 Build Infrastructure

- [ ] [T007] [P1] [SETUP] Create Android NDK toolchain configuration `cmake/android-toolchain.cmake`
- [ ] [T008] [P1] [SETUP] Create Suricata cross-compile script for Android ARM64 `suricata/scripts/build-android.sh`
- [ ] [T009] [P1] [SETUP] Create Suricata cross-compile script for Yocto Linux ARM64 `suricata/scripts/build-yocto.sh`
- [ ] [T010] [P2] [SETUP] Create Yocto ARM64 toolchain configuration `cmake/yocto-toolchain.cmake`
- [ ] [T011] [P2] [SETUP] Add GitHub Actions CI workflow for ARM64 builds `.github/workflows/build.yml`

### 1.3 Configuration Templates

- [ ] [T012] [P1] [SETUP] Create vnidsd.conf.example with all configuration options `vnidsd/vnidsd.conf.example`
- [ ] [T013] [P1] [SETUP] Create suricata.yaml.template with Unix socket EVE output `suricata/suricata.yaml.template`
- [ ] [T014] [P2] [SETUP] Create Android init.rc service definition `deploy/android/init.vnids.rc`
- [ ] [T015] [P2] [SETUP] Create systemd unit file for Yocto `deploy/yocto/vnids.service`
- [ ] [T016] [P3] [SETUP] Create BusyBox init script for Buildroot `deploy/buildroot/S90vnids`

---

## Phase 2: Foundational Components

### 2.1 Common Headers and Types

- [ ] [T017] [P1] [FOUND] Define common types (vnids_result_t, vnids_error_t) `shared/include/vnids_types.h`
- [ ] [T018] [P1] [FOUND] Define IPC message structures `shared/include/vnids_ipc.h`
- [ ] [T019] [P1] [FOUND] Define security event structures `shared/include/vnids_event.h`
- [ ] [T020] [P2] [FOUND] Define configuration structures `shared/include/vnids_config.h`
- [ ] [T021] [P2] [FOUND] Create logging macros and utilities `shared/include/vnids_log.h`

### 2.2 Configuration Module

- [ ] [T022] [P1] [FOUND] Implement config file parser (INI format) `vnidsd/src/config.c`
- [ ] [T023] [P1] [FOUND] Implement config structure and defaults `vnidsd/include/config.h`
- [ ] [T024] [P2] [FOUND] Add config validation with error messages `vnidsd/src/config_validate.c`
- [ ] [T025] [P2] [FOUND] Support environment variable overrides `vnidsd/src/config_env.c`
- [ ] [T026] [P3] [FOUND] Write unit tests for config loading `vnidsd/tests/test_config.c`

### 2.3 IPC Socket Infrastructure

- [ ] [T027] [P1] [FOUND] Implement Unix socket server for API endpoint `vnidsd/src/ipc_server.c`
- [ ] [T028] [P1] [FOUND] Implement Unix socket client for Suricata EVE connection `vnidsd/src/ipc_client.c`
- [ ] [T029] [P1] [FOUND] Implement IPC message serialization (JSON) `vnidsd/src/ipc_message.c`
- [ ] [T030] [P1] [FOUND] Implement control socket for sending commands to Suricata `vnidsd/src/ipc_control.c`
- [ ] [T031] [P2] [FOUND] Add socket path management and cleanup on startup `vnidsd/src/ipc_utils.c`
- [ ] [T032] [P2] [FOUND] Write unit tests for IPC message serialization `vnidsd/tests/test_ipc.c`

### 2.4 Event Processing Core

- [ ] [T033] [P1] [FOUND] Implement EVE JSON event parser (using cJSON) `vnidsd/src/eve_parser.c`
- [ ] [T034] [P1] [FOUND] Implement lock-free MPSC event queue `vnidsd/src/event_queue.c`
- [ ] [T035] [P1] [FOUND] Implement SecurityEvent structure and helpers `vnidsd/src/event.c`
- [ ] [T036] [P2] [FOUND] Add event timestamp normalization (microsecond precision) `vnidsd/src/timestamp.c`
- [ ] [T037] [P2] [FOUND] Write unit tests for event parsing `vnidsd/tests/test_event.c`

### 2.5 SQLite Storage

- [ ] [T038] [P1] [FOUND] Implement SQLite database initialization with schema `vnidsd/src/storage.c`
- [ ] [T039] [P1] [FOUND] Implement event insertion with batching `vnidsd/src/storage_event.c`
- [ ] [T040] [P2] [FOUND] Implement event query API with filters `vnidsd/src/storage_query.c`
- [ ] [T041] [P2] [FOUND] Implement data retention cleanup job `vnidsd/src/storage_retention.c`
- [ ] [T042] [P2] [FOUND] Enable WAL mode and optimize for concurrent access `vnidsd/src/storage.c`
- [ ] [T043] [P3] [FOUND] Write integration tests for storage layer `vnidsd/tests/test_storage.c`

### 2.6 Daemon Entry Point

- [ ] [T044] [P1] [FOUND] Implement main.c with CLI argument parsing (getopt) `vnidsd/src/main.c`
- [ ] [T045] [P1] [FOUND] Implement daemon initialization sequence `vnidsd/src/daemon.c`
- [ ] [T046] [P1] [FOUND] Implement signal handling (SIGTERM, SIGHUP, SIGUSR1) `vnidsd/src/signal.c`
- [ ] [T047] [P2] [FOUND] Implement logging with syslog support `vnidsd/src/log.c`
- [ ] [T048] [P2] [FOUND] Implement PID file management `vnidsd/src/pidfile.c`
- [ ] [T049] [P2] [FOUND] Implement event loop using epoll `vnidsd/src/eventloop.c`

---

## Phase 3: US1 - Basic Ethernet Intrusion Detection

### 3.1 Suricata Process Management

- [ ] [T050] [P1] [US1] Implement Suricata process spawning with fork/exec `vnidsd/src/suricata_spawn.c`
- [ ] [T051] [P1] [US1] Implement Suricata watchdog thread with health checks `vnidsd/src/watchdog.c`
- [ ] [T052] [P1] [US1] Implement crash detection and automatic restart `vnidsd/src/watchdog_recovery.c`
- [ ] [T053] [P1] [US1] Implement IPC heartbeat monitoring (5s interval) `vnidsd/src/watchdog_heartbeat.c`
- [ ] [T054] [P2] [US1] Implement restart backoff strategy (immediate → 5s → 30s → 60s) `vnidsd/src/watchdog_backoff.c`
- [ ] [T055] [P2] [US1] Write integration tests for watchdog `vnidsd/tests/test_watchdog.c`

### 3.2 Suricata Configuration

- [ ] [T056] [P1] [US1] Configure Suricata AF_PACKET capture for eth0 `suricata/suricata.yaml.template`
- [ ] [T057] [P1] [US1] Configure EVE output to Unix socket /var/run/vnids/events.sock `suricata/suricata.yaml.template`
- [ ] [T058] [P1] [US1] Configure memory limits for 64MB budget `suricata/suricata.yaml.template`
- [ ] [T059] [P2] [US1] Configure low-latency detection profile `suricata/suricata.yaml.template`

### 3.3 Baseline Detection Rules - Flood Attacks

- [ ] [T060] [P1] [US1] Create TCP SYN flood detection rule (threshold: 1000 pps) `suricata/rules/baseline/flood-attacks.rules`
- [ ] [T061] [P1] [US1] Create TCP ACK flood detection rule `suricata/rules/baseline/flood-attacks.rules`
- [ ] [T062] [P1] [US1] Create UDP flood detection rule `suricata/rules/baseline/flood-attacks.rules`
- [ ] [T063] [P1] [US1] Create ICMP flood detection rule `suricata/rules/baseline/flood-attacks.rules`
- [ ] [T064] [P2] [US1] Create IGMP flood detection rule `suricata/rules/baseline/flood-attacks.rules`
- [ ] [T065] [P2] [US1] Create Ping of Death detection rule `suricata/rules/baseline/flood-attacks.rules`

### 3.4 Baseline Detection Rules - Malformed Packets

- [ ] [T066] [P1] [US1] Create TCP Land attack detection rule `suricata/rules/baseline/malformed-packets.rules`
- [ ] [T067] [P1] [US1] Create TCP malformed header detection rules `suricata/rules/baseline/malformed-packets.rules`
- [ ] [T068] [P1] [US1] Create UDP malformed packet detection rules `suricata/rules/baseline/malformed-packets.rules`
- [ ] [T069] [P2] [US1] Create IP fragmentation anomaly detection rules `suricata/rules/baseline/malformed-packets.rules`

### 3.5 Event Stream Processing

- [ ] [T070] [P1] [US1] Implement EVE event reader from Unix socket `vnidsd/src/eve_reader.c`
- [ ] [T071] [P1] [US1] Implement alert event processing and storage `vnidsd/src/handler_alert.c`
- [ ] [T072] [P1] [US1] Implement flow event processing `vnidsd/src/handler_flow.c`
- [ ] [T073] [P2] [US1] Implement stats event processing `vnidsd/src/handler_stats.c`
- [ ] [T074] [P2] [US1] Add latency measurement for packet-to-alert timing `vnidsd/src/metrics.c`

### 3.6 US1 Testing

- [ ] [T075] [P1] [US1] Create PCAP files for SYN flood attack `tests/integration/pcaps/syn_flood.pcap`
- [ ] [T076] [P1] [US1] Create PCAP files for Land attack `tests/integration/pcaps/land_attack.pcap`
- [ ] [T077] [P1] [US1] Create pytest test_flood_detection.py `tests/integration/test_flood_detection.py`
- [ ] [T078] [P1] [US1] Create pytest test_malformed_packets.py `tests/integration/test_malformed_packets.py`
- [ ] [T079] [P2] [US1] Create latency benchmark test `tests/integration/test_latency.py`

---

## Phase 4: US2 - Automotive Protocol DPI

### 4.1 SOME/IP Protocol Parser

- [ ] [T080] [P1] [US2] Implement SOME/IP header parser `suricata/src/protocols/someip/someip-parser.c`
- [ ] [T081] [P1] [US2] Implement SOME/IP app-layer registration `suricata/src/protocols/someip/someip.c`
- [ ] [T082] [P1] [US2] Add SOME/IP detection keywords (service_id, method_id, message_type) `suricata/src/protocols/someip/detect-someip.c`
- [ ] [T083] [P1] [US2] Configure SOME/IP port detection (UDP/TCP 30490-30491) `suricata/suricata.yaml.template`
- [ ] [T084] [P2] [US2] Add SOME/IP-SD (Service Discovery) message parsing `suricata/src/protocols/someip/someip-sd.c`
- [ ] [T085] [P2] [US2] Write unit tests for SOME/IP parser `suricata/src/protocols/someip/someip-parser-test.c`

### 4.2 SOME/IP Detection Rules

- [ ] [T086] [P1] [US2] Create SOME/IP unauthorized service access rule `suricata/rules/baseline/automotive.rules`
- [ ] [T087] [P1] [US2] Create SOME/IP malformed message rule `suricata/rules/baseline/automotive.rules`
- [ ] [T088] [P2] [US2] Create SOME/IP-SD anomaly rules `suricata/rules/baseline/automotive.rules`

### 4.3 DoIP Protocol Parser

- [ ] [T089] [P1] [US2] Implement DoIP header parser `suricata/src/protocols/doip/doip-parser.c`
- [ ] [T090] [P1] [US2] Implement DoIP app-layer registration `suricata/src/protocols/doip/doip.c`
- [ ] [T091] [P1] [US2] Add DoIP detection keywords (payload_type, source_address, target_address) `suricata/src/protocols/doip/detect-doip.c`
- [ ] [T092] [P1] [US2] Configure DoIP port detection (TCP/UDP 13400) `suricata/suricata.yaml.template`
- [ ] [T093] [P2] [US2] Parse UDS diagnostic messages within DoIP `suricata/src/protocols/doip/doip-uds.c`
- [ ] [T094] [P2] [US2] Write unit tests for DoIP parser `suricata/src/protocols/doip/doip-parser-test.c`

### 4.4 DoIP Detection Rules

- [ ] [T095] [P1] [US2] Create DoIP routing activation from external rule `suricata/rules/baseline/automotive.rules`
- [ ] [T096] [P1] [US2] Create DoIP unauthorized diagnostic session rule `suricata/rules/baseline/automotive.rules`
- [ ] [T097] [P2] [US2] Create UDS security access brute force rule `suricata/rules/baseline/automotive.rules`

### 4.5 vnidsd Automotive Metadata Handling

- [ ] [T098] [P1] [US2] Add SOME/IP metadata struct to event handling `vnidsd/src/metadata_someip.c`
- [ ] [T099] [P1] [US2] Add DoIP metadata struct to event handling `vnidsd/src/metadata_doip.c`
- [ ] [T100] [P2] [US2] Implement protocol-specific metadata parsing from EVE JSON `vnidsd/src/metadata.c`

### 4.6 US2 Testing

- [ ] [T101] [P1] [US2] Create PCAP files for SOME/IP traffic `tests/integration/pcaps/someip_normal.pcap`
- [ ] [T102] [P1] [US2] Create PCAP files for SOME/IP anomalies `tests/integration/pcaps/someip_anomaly.pcap`
- [ ] [T103] [P1] [US2] Create PCAP files for DoIP traffic `tests/integration/pcaps/doip_routing.pcap`
- [ ] [T104] [P1] [US2] Create pytest test_someip_detection.py `tests/integration/test_someip_detection.py`
- [ ] [T105] [P1] [US2] Create pytest test_doip_detection.py `tests/integration/test_doip_detection.py`
- [ ] [T106] [P2] [US2] Create fuzz test harness for SOME/IP parser `tests/fuzz/fuzz_someip_parser/`

---

## Phase 5: US3 - Standard Protocol DPI

### 5.1 GB/T 32960.3 Protocol Parser

- [ ] [T107] [P1] [US3] Implement GB/T 32960.3 message parser `suricata/src/protocols/gbt32960/gbt32960-parser.c`
- [ ] [T108] [P1] [US3] Implement GB/T 32960.3 app-layer registration `suricata/src/protocols/gbt32960/gbt32960.c`
- [ ] [T109] [P1] [US3] Add GB/T 32960.3 detection keywords (command, vin, encryption) `suricata/src/protocols/gbt32960/detect-gbt32960.c`
- [ ] [T110] [P2] [US3] Configure GB/T 32960.3 default port detection `suricata/suricata.yaml.template`
- [ ] [T111] [P2] [US3] Write unit tests for GB/T 32960.3 parser `suricata/src/protocols/gbt32960/gbt32960-parser-test.c`

### 5.2 GB/T 32960.3 Detection Rules

- [ ] [T112] [P1] [US3] Create unencrypted real-time data transmission rule `suricata/rules/baseline/automotive.rules`
- [ ] [T113] [P2] [US3] Create VIN format validation rule `suricata/rules/baseline/automotive.rules`
- [ ] [T114] [P2] [US3] Create abnormal command sequence rule `suricata/rules/baseline/automotive.rules`

### 5.3 Standard Protocol Configuration

- [ ] [T115] [P1] [US3] Enable HTTP protocol parsing in Suricata config `suricata/suricata.yaml.template`
- [ ] [T116] [P1] [US3] Enable DNS protocol parsing in Suricata config `suricata/suricata.yaml.template`
- [ ] [T117] [P1] [US3] Enable MQTT protocol parsing in Suricata config `suricata/suricata.yaml.template`
- [ ] [T118] [P2] [US3] Enable TLS protocol parsing (metadata only) `suricata/suricata.yaml.template`
- [ ] [T119] [P2] [US3] Enable FTP protocol parsing `suricata/suricata.yaml.template`
- [ ] [T120] [P2] [US3] Enable Telnet protocol parsing `suricata/suricata.yaml.template`

### 5.4 Standard Protocol Detection Rules

- [ ] [T121] [P1] [US3] Create HTTP SQL injection detection rules `suricata/rules/baseline/http.rules`
- [ ] [T122] [P1] [US3] Create HTTP suspicious user-agent rules `suricata/rules/baseline/http.rules`
- [ ] [T123] [P1] [US3] Create DNS malicious domain query rules `suricata/rules/baseline/dns.rules`
- [ ] [T124] [P2] [US3] Create DNS tunneling detection rules `suricata/rules/baseline/dns.rules`
- [ ] [T125] [P2] [US3] Create MQTT unauthorized topic access rules `suricata/rules/baseline/mqtt.rules`

### 5.5 Weak Credential Detection

- [ ] [T126] [P1] [US3] Create Telnet weak password detection rules `suricata/rules/baseline/weak-credentials.rules`
- [ ] [T127] [P1] [US3] Create FTP weak password detection rules `suricata/rules/baseline/weak-credentials.rules`
- [ ] [T128] [P2] [US3] Create common password dictionary file `suricata/rules/baseline/weak-passwords.list`

### 5.6 vnidsd Standard Protocol Metadata

- [ ] [T129] [P1] [US3] Add HTTP metadata struct to event handling `vnidsd/src/metadata_http.c`
- [ ] [T130] [P1] [US3] Add DNS metadata struct to event handling `vnidsd/src/metadata_dns.c`
- [ ] [T131] [P2] [US3] Add GB/T 32960.3 metadata struct `vnidsd/src/metadata_gbt32960.c`

### 5.7 US3 Testing

- [ ] [T132] [P1] [US3] Create pytest test_http_detection.py `tests/integration/test_http_detection.py`
- [ ] [T133] [P1] [US3] Create pytest test_dns_detection.py `tests/integration/test_dns_detection.py`
- [ ] [T134] [P2] [US3] Create pytest test_weak_credentials.py `tests/integration/test_weak_credentials.py`
- [ ] [T135] [P2] [US3] Create pytest test_gbt32960_detection.py `tests/integration/test_gbt32960_detection.py`
- [ ] [T136] [P2] [US3] Create fuzz test harness for GB/T 32960.3 parser `tests/fuzz/fuzz_gbt32960_parser/`

---

## Phase 6: US4 - Rule Hot Reload

### 6.1 Rule Loading

- [ ] [T137] [P1] [US4] Implement rule file discovery from rules directory `vnidsd/src/rule_loader.c`
- [ ] [T138] [P1] [US4] Implement rule file parsing and metadata extraction `vnidsd/src/rule_parser.c`
- [ ] [T139] [P1] [US4] Define Ruleset and DetectionRule structures `vnidsd/include/rule.h`
- [ ] [T140] [P2] [US4] Store rule metadata in SQLite `vnidsd/src/rule_storage.c`

### 6.2 Rule Validation

- [ ] [T141] [P1] [US4] Implement rule syntax validation via Suricata -T `vnidsd/src/rule_validate.c`
- [ ] [T142] [P1] [US4] Implement validation error parsing and reporting `vnidsd/src/rule_error.c`
- [ ] [T143] [P2] [US4] Add rule checksum calculation for change detection `vnidsd/src/rule_checksum.c`

### 6.3 Hot Reload Mechanism

- [ ] [T144] [P1] [US4] Implement SIGUSR2 signal sending to Suricata `vnidsd/src/rule_reload.c`
- [ ] [T145] [P1] [US4] Implement reload status monitoring via IPC `vnidsd/src/rule_status.c`
- [ ] [T146] [P1] [US4] Implement rollback to previous ruleset on failure `vnidsd/src/rule_rollback.c`
- [ ] [T147] [P2] [US4] Add inotify watcher for automatic reload on rules/ change `vnidsd/src/rule_watcher.c`

### 6.4 Rule Management API

- [ ] [T148] [P1] [US4] Implement rule reload command handler `vnidsd/src/api_reload.c`
- [ ] [T149] [P1] [US4] Implement rule list command handler `vnidsd/src/api_rules.c`
- [ ] [T150] [P2] [US4] Implement rule validate command handler `vnidsd/src/api_validate.c`

### 6.5 US4 Testing

- [ ] [T151] [P1] [US4] Create test for hot reload without packet loss `tests/integration/test_hot_reload.py`
- [ ] [T152] [P1] [US4] Create test for invalid ruleset rejection `tests/integration/test_rule_validation.py`
- [ ] [T153] [P2] [US4] Create test for rollback on reload failure `tests/integration/test_rule_rollback.py`
- [ ] [T154] [P2] [US4] Create fuzz test for rule parser `tests/fuzz/fuzz_rule_parser/`

---

## Phase 7: US5 - GPL-Compliant Process Isolation

### 7.1 Process Architecture Verification

- [ ] [T155] [P1] [US5] Document process boundary in architecture.md `shared/docs/architecture.md`
- [ ] [T156] [P1] [US5] Verify no static linking between vnidsd and Suricata `Makefile`
- [ ] [T157] [P1] [US5] Verify IPC-only communication in code review checklist `docs/code-review-checklist.md`

### 7.2 Socket-Only IPC

- [ ] [T158] [P1] [US5] Audit all vnidsd→Suricata communication paths `vnidsd/src/ipc_*.c`
- [ ] [T159] [P1] [US5] Audit all Suricata→vnidsd communication paths `vnidsd/src/eve_*.c`
- [ ] [T160] [P2] [US5] Add IPC protocol version checking on connection `vnidsd/src/ipc_version.c`

### 7.3 License Compliance

- [ ] [T161] [P1] [US5] Add GPL v2 license file for Suricata modifications `suricata/LICENSE`
- [ ] [T162] [P1] [US5] Add Apache 2.0 license file for vnidsd `vnidsd/LICENSE`
- [ ] [T163] [P1] [US5] Add license headers to all source files `scripts/add-license-headers.sh`
- [ ] [T164] [P2] [US5] Create NOTICE file listing all dependencies `NOTICE`

### 7.4 Build Separation

- [ ] [T165] [P1] [US5] Ensure separate build targets for GPL and proprietary code `Makefile`
- [ ] [T166] [P2] [US5] Add build verification that no GPL code linked into vnidsd `scripts/verify-gpl-isolation.sh`

### 7.5 US5 Testing

- [ ] [T167] [P1] [US5] Create test verifying process separation `tests/integration/test_process_isolation.py`
- [ ] [T168] [P1] [US5] Create test verifying IPC-only communication `tests/integration/test_ipc_only.py`
- [ ] [T169] [P2] [US5] Document GPL compliance for legal review `docs/gpl-compliance.md`

---

## Phase 8: CLI Tool (vnidsctl)

### 8.1 CLI Framework

- [ ] [T170] [P1] [FOUND] Implement CLI main with getopt argument parser `vnids-cli/src/main.c`
- [ ] [T171] [P1] [FOUND] Implement Unix socket client to vnidsd API `vnids-cli/src/client.c`
- [ ] [T172] [P2] [FOUND] Add colored output and table formatting `vnids-cli/src/output.c`

### 8.2 Status Commands

- [ ] [T173] [P1] [FOUND] Implement `vnidsctl status` command `vnids-cli/src/cmd_status.c`
- [ ] [T174] [P1] [FOUND] Implement `vnidsctl stats` command `vnids-cli/src/cmd_stats.c`

### 8.3 Event Commands

- [ ] [T175] [P1] [FOUND] Implement `vnidsctl events list` command `vnids-cli/src/cmd_events.c`
- [ ] [T176] [P2] [FOUND] Implement `vnidsctl events tail` (follow mode) `vnids-cli/src/cmd_events.c`
- [ ] [T177] [P2] [FOUND] Implement `vnidsctl events export` command `vnids-cli/src/cmd_events.c`

### 8.4 Rule Commands

- [ ] [T178] [P1] [US4] Implement `vnidsctl rules list` command `vnids-cli/src/cmd_rules.c`
- [ ] [T179] [P1] [US4] Implement `vnidsctl rules reload` command `vnids-cli/src/cmd_rules.c`
- [ ] [T180] [P2] [US4] Implement `vnidsctl rules validate` command `vnids-cli/src/cmd_rules.c`

### 8.5 Config Commands

- [ ] [T181] [P2] [FOUND] Implement `vnidsctl config show` command `vnids-cli/src/cmd_config.c`
- [ ] [T182] [P3] [FOUND] Implement `vnidsctl config get <key>` command `vnids-cli/src/cmd_config.c`

---

## Phase 9: Platform Deployment

### 9.1 Android Deployment

- [ ] [T183] [P1] [SETUP] Create Android installation script `deploy/scripts/install-android.sh`
- [ ] [T184] [P1] [SETUP] Create SELinux policy for vnidsd `deploy/android/vnids.te`
- [ ] [T185] [P2] [SETUP] Create Magisk module structure for rooted devices `deploy/android/magisk/`

### 9.2 Yocto Deployment

- [ ] [T186] [P1] [SETUP] Create Yocto recipe for vnids package `deploy/yocto/vnids.bb`
- [ ] [T187] [P2] [SETUP] Create Yocto installation script `deploy/scripts/install-yocto.sh`

### 9.3 Buildroot Deployment

- [ ] [T188] [P2] [SETUP] Create Buildroot package definition `deploy/buildroot/vnids.mk`
- [ ] [T189] [P3] [SETUP] Create Buildroot installation script `deploy/scripts/install-buildroot.sh`

---

## Phase 10: Polish & Documentation

### 10.1 Performance Optimization

- [ ] [T190] [P2] [POLISH] Profile and optimize event queue performance `vnidsd/src/event_queue.c`
- [ ] [T191] [P2] [POLISH] Optimize SQLite batch insert performance `vnidsd/src/storage_event.c`
- [ ] [T192] [P2] [POLISH] Implement memory pool allocator for hot path `vnidsd/src/mempool.c`
- [ ] [T193] [P3] [POLISH] Add cache-aligned structures for ARM64 `vnidsd/include/align.h`

### 10.2 Error Handling

- [ ] [T194] [P2] [POLISH] Implement comprehensive error codes `vnidsd/include/error.h`
- [ ] [T195] [P2] [POLISH] Add graceful degradation on resource exhaustion `vnidsd/src/daemon.c`
- [ ] [T196] [P3] [POLISH] Add structured logging for all error conditions `vnidsd/src/log.c`

### 10.3 Documentation

- [ ] [T197] [P2] [POLISH] Write API documentation with Doxygen comments `vnidsd/src/*.c`
- [ ] [T198] [P2] [POLISH] Create rule writing guide `docs/rule-writing.md`
- [ ] [T199] [P2] [POLISH] Create troubleshooting guide `docs/troubleshooting.md`
- [ ] [T200] [P3] [POLISH] Generate Doxygen HTML documentation `docs/api/`

### 10.4 Final Integration Testing

- [ ] [T201] [P1] [POLISH] Create end-to-end integration test suite `tests/integration/test_e2e.py`
- [ ] [T202] [P1] [POLISH] Create 30-day stability test plan `tests/hil/stability_test.md`
- [ ] [T203] [P2] [POLISH] Create performance benchmark suite `tests/benchmark/`
- [ ] [T204] [P2] [POLISH] Create memory leak detection test (Valgrind) `tests/integration/test_memory_leak.py`

---

## Summary

| Phase | Tasks | P1 | P2 | P3 |
|-------|-------|----|----|-----|
| 1. Setup | T001-T016 | 8 | 6 | 2 |
| 2. Foundational | T017-T049 | 19 | 12 | 2 |
| 3. US1 - Basic IDS | T050-T079 | 22 | 8 | 0 |
| 4. US2 - Automotive DPI | T080-T106 | 19 | 8 | 0 |
| 5. US3 - Standard DPI | T107-T136 | 14 | 16 | 0 |
| 6. US4 - Hot Reload | T137-T154 | 11 | 7 | 0 |
| 7. US5 - GPL Isolation | T155-T169 | 10 | 5 | 0 |
| 8. CLI Tool | T170-T182 | 8 | 4 | 1 |
| 9. Deployment | T183-T189 | 3 | 3 | 1 |
| 10. Polish | T190-T204 | 2 | 10 | 3 |
| **Total** | **204** | **116** | **79** | **9** |

## Dependencies

```
Phase 1 (Setup)
    ↓
Phase 2 (Foundational) ──→ Phase 8 (CLI - partial)
    ↓
Phase 3 (US1) ──────────→ Phase 7 (US5 - partial)
    ↓
Phase 4 (US2)
    ↓
Phase 5 (US3)
    ↓
Phase 6 (US4) ──────────→ Phase 8 (CLI - rules commands)
    ↓
Phase 7 (US5)
    ↓
Phase 9 (Deployment)
    ↓
Phase 10 (Polish)
```

## Critical Path

1. **T001-T004**: Build system setup (blocks all C development)
2. **T008-T009**: Suricata build scripts (blocks all detection testing)
3. **T027-T030**: IPC infrastructure (blocks vnidsd↔Suricata communication)
4. **T050-T053**: Watchdog (blocks stable Suricata process management)
5. **T060-T068**: Baseline rules (blocks US1 acceptance testing)
6. **T080-T097**: Automotive parsers (blocks US2 acceptance testing)
7. **T144-T146**: Hot reload mechanism (blocks US4 acceptance testing)

## C Language Dependencies

| Library | Purpose | Source |
|---------|---------|--------|
| cJSON | JSON parsing for EVE events | https://github.com/DaveGamble/cJSON |
| SQLite3 | Event storage | System package |
| pthread | Threading | System libc |
| libevent/epoll | Event loop | System / manual epoll |

## Build Commands

```bash
# Configure for Android ARM64
cmake -B build-android -DCMAKE_TOOLCHAIN_FILE=cmake/android-toolchain.cmake

# Configure for Yocto ARM64
cmake -B build-yocto -DCMAKE_TOOLCHAIN_FILE=cmake/yocto-toolchain.cmake

# Build vnidsd and vnids-cli
cmake --build build-android --target vnidsd vnidsctl

# Build Suricata
./suricata/scripts/build-android.sh
```
