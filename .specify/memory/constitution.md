<!--
  Sync Impact Report
  ==================
  Version change: N/A → 1.0.0 (initial ratification)

  Added Principles:
    - I. ARM Architecture Memory Optimization
    - II. Real-Time Packet Processing Performance
    - III. Automotive-Grade Reliability (ISO 26262 Awareness)
    - IV. Suricata Rule Compatibility
    - V. Android NDK Integration Standards
    - VI. Embedded Power Efficiency
    - VII. Security-First Coding Practices
    - VIII. Comprehensive Testing Discipline

  Added Sections:
    - Performance & Resource Constraints
    - Development Workflow & Quality Gates
    - Governance

  Removed Sections: N/A (initial creation)

  Templates Requiring Updates:
    - .specify/templates/plan-template.md: ✅ No updates required (Constitution Check section already generic)
    - .specify/templates/spec-template.md: ✅ No updates required (requirements structure compatible)
    - .specify/templates/tasks-template.md: ✅ No updates required (phase structure supports testing discipline)

  Follow-up TODOs: None
-->

# VNIDS Constitution

## Core Principles

### I. ARM Architecture Memory Optimization

All code targeting ARM platforms MUST be optimized for ARM-specific memory characteristics:

- **Cache-Aware Data Structures**: Data structures MUST be aligned to cache line boundaries (typically 64 bytes on ARM Cortex-A series). Hot data paths MUST minimize cache misses.
- **Memory Pool Allocation**: Dynamic memory allocation during packet processing is PROHIBITED. All runtime buffers MUST use pre-allocated memory pools with deterministic allocation patterns.
- **NEON/SIMD Utilization**: Performance-critical paths (pattern matching, checksum calculation, data copying) MUST leverage ARM NEON intrinsics where beneficial.
- **Memory Footprint Limits**: Total static memory allocation MUST NOT exceed 64MB for core engine. Configuration MUST allow tuning for devices with 128MB-2GB RAM.
- **Zero-Copy Design**: Packet data MUST flow through the processing pipeline without unnecessary copies. DMA buffers SHOULD be accessed directly where platform permits.

**Rationale**: Vehicle ECUs operate under strict memory constraints. ARM Cortex-A/R series processors have specific cache hierarchies that differ from x86. Ignoring these characteristics leads to 10-100x performance degradation.

### II. Real-Time Packet Processing Performance

Packet processing latency MUST meet automotive real-time requirements:

- **Latency Budget**: End-to-end processing latency from packet ingress to alert generation MUST be <10ms at p99 for all supported traffic loads.
- **Throughput Target**: System MUST sustain 100 Mbps CAN-to-Ethernet bridged traffic without packet drops.
- **Lock-Free Data Paths**: The hot path from packet capture to rule matching MUST NOT use mutex locks. Use lock-free queues and atomic operations.
- **Deterministic Scheduling**: Critical processing threads MUST be configurable for SCHED_FIFO with appropriate priorities.
- **Interrupt Coalescing**: Network interrupt handling MUST be tuned to balance latency vs CPU overhead for target hardware.

**Rationale**: Vehicle network attacks often exploit timing windows measured in milliseconds. Detection delays beyond 10ms risk missing time-critical attack patterns on CAN bus and automotive Ethernet.

### III. Automotive-Grade Reliability (ISO 26262 Awareness)

Code MUST be developed with awareness of ISO 26262 functional safety principles:

- **Defensive Coding**: All external inputs MUST be validated. Functions MUST check preconditions and handle violations gracefully.
- **No Undefined Behavior**: Code MUST NOT rely on undefined behavior. All integer operations MUST be checked for overflow where relevant.
- **Fail-Safe Defaults**: On detection of internal errors, system MUST fail to a safe state (continue monitoring with degraded functionality rather than crash).
- **Watchdog Integration**: System MUST support hardware/software watchdog integration with configurable timeout and recovery actions.
- **MISRA-C Alignment**: C code SHOULD follow MISRA-C:2012 guidelines where practical. Deviations MUST be documented with rationale.

**Rationale**: While VNIDS itself is not a safety-critical component, it operates in safety-critical environments. Unreliable behavior could mask actual vehicle network issues or cause system instability.

### IV. Suricata Rule Compatibility

Rule processing MUST maintain compatibility with Suricata rule syntax:

- **Rule Parser Compatibility**: System MUST parse Suricata 7.x rule format. Unsupported keywords MUST generate clear warnings, not silent failures.
- **Signature ID Preservation**: Rule SIDs and GIDs MUST be preserved in alerts for integration with existing SIEM/SOC tooling.
- **Automotive Protocol Extensions**: Custom keywords for automotive protocols (CAN, UDS, DoIP, SOME/IP) MUST follow Suricata keyword naming conventions.
- **Ruleset Updates**: System MUST support hot-reload of rulesets without service interruption.
- **Performance Metadata**: Rules MUST support performance hints (priority, rate-limiting) for resource-constrained operation.

**Rationale**: Suricata is the industry standard for network-based IDS rules. Compatibility enables reuse of existing automotive IDS rulesets and integration with established security operations workflows.

### V. Android NDK Integration Standards

Native components MUST integrate cleanly with Android application layer:

- **ABI Compliance**: Native libraries MUST support arm64-v8a (primary) and armeabi-v7a (legacy) ABIs. x86_64 support for emulator testing is RECOMMENDED.
- **JNI Interface Design**: JNI boundaries MUST minimize crossing frequency. Batch operations over individual calls. All JNI functions MUST handle exceptions properly.
- **Android Lifecycle Awareness**: Native components MUST respond correctly to Android lifecycle events (pause, resume, low memory).
- **Permission Handling**: Code MUST gracefully handle permission denials. Network capture fallback modes MUST be documented.
- **NDK Version Pinning**: Project MUST specify minimum NDK version (r25+) and document any version-specific features used.

**Rationale**: VNIDS is deployed as an Android application on vehicle head units and telematics devices. Poor NDK integration leads to crashes, battery drain, and failed Google Play certification.

### VI. Embedded Power Efficiency

Power consumption MUST be minimized for battery-operated and thermally-constrained deployments:

- **Idle Power Budget**: System in idle monitoring state MUST NOT cause >5% battery drain per hour on reference hardware.
- **Adaptive Processing**: CPU-intensive operations (deep inspection, ML inference) MUST be deferrable to charging/parked state.
- **Wake Lock Discipline**: Android wake locks MUST be minimized. Prefer WorkManager for background processing.
- **Thermal Awareness**: System MUST reduce processing intensity when thermal throttling is detected.
- **Network Radio Efficiency**: Alert transmission MUST batch where possible to minimize cellular radio wake-ups.

**Rationale**: Vehicle telematics devices operate on vehicle battery. Excessive power consumption can prevent vehicle starting. Thermal constraints in dashboard environments are severe.

### VII. Security-First Coding Practices

All code MUST follow secure coding principles:

- **Input Validation**: ALL external input (network packets, configuration files, IPC messages) MUST be validated before processing. Assume all input is malicious.
- **Memory Safety**: Prefer Rust for new components. C code MUST use bounds-checked operations. Buffer sizes MUST be explicitly tracked.
- **Cryptographic Standards**: Use only approved cryptographic primitives (AES-256-GCM, SHA-256+, Ed25519/X25519). No custom cryptography.
- **Secrets Management**: Cryptographic keys and credentials MUST NOT be hardcoded. Support Android Keystore and secure element storage.
- **Least Privilege**: Each component MUST operate with minimum required permissions. Use Android sandboxing and SELinux policies.
- **Secure Defaults**: Default configuration MUST be secure. Insecure options MUST require explicit opt-in with warnings.

**Rationale**: An IDS that can be compromised defeats its purpose. Vehicle networks are high-value attack targets. Security vulnerabilities in the IDS itself would be catastrophic.

### VIII. Comprehensive Testing Discipline

Testing MUST cover multiple dimensions to ensure quality:

- **Unit Testing**: All modules MUST have unit tests with minimum 80% line coverage. Critical path coverage MUST be 100%.
- **Integration Testing**: Cross-component interactions MUST be tested. Android/native boundary testing is MANDATORY.
- **Fuzz Testing**: All parsers (rule parser, protocol decoders) MUST undergo continuous fuzz testing. Use libFuzzer or AFL.
- **Performance Testing**: Latency and throughput benchmarks MUST run in CI. Regressions >10% MUST block merge.
- **Hardware-in-Loop (HIL) Testing**: Release candidates MUST pass HIL validation on reference hardware with real CAN/Ethernet traffic.
- **Security Testing**: SAST tools MUST run in CI. Periodic penetration testing is REQUIRED before major releases.

**Rationale**: Vehicle IDS operates in an environment where failures are difficult to diagnose and fix. Comprehensive testing catches issues before deployment to potentially millions of vehicles.

## Performance & Resource Constraints

### Memory Budgets

| Component | Maximum Allocation |
|-----------|-------------------|
| Rule Engine | 32 MB |
| Packet Buffers | 16 MB |
| Alert Queue | 4 MB |
| Logging | 8 MB |
| Misc/Overhead | 4 MB |
| **Total** | **64 MB** |

### Latency Budgets

| Stage | Maximum Latency |
|-------|-----------------|
| Packet Capture | 1 ms |
| Protocol Decode | 2 ms |
| Rule Matching | 5 ms |
| Alert Generation | 1 ms |
| IPC to App | 1 ms |
| **Total** | **<10 ms p99** |

### Platform Support Matrix

| Platform | Priority | Status |
|----------|----------|--------|
| Android 10+ arm64-v8a | P0 | Required |
| Android 10+ armeabi-v7a | P1 | Required |
| Linux ARM64 (Yocto) | P1 | Required |
| Android x86_64 (emulator) | P2 | Recommended |

## Development Workflow & Quality Gates

### Pre-Commit Requirements

1. All code MUST pass static analysis (clippy for Rust, clang-tidy for C/C++)
2. All code MUST be formatted (rustfmt, clang-format)
3. Unit tests MUST pass locally

### PR Merge Requirements

1. CI pipeline MUST pass (build, test, lint, security scan)
2. Code review approval from at least one maintainer
3. Performance benchmarks MUST NOT regress >10%
4. New code MUST include appropriate tests

### Release Requirements

1. All PR merge requirements
2. Integration test suite MUST pass
3. Fuzz testing MUST run for minimum 1 hour without crashes
4. HIL validation on reference hardware
5. Security review for code changes affecting attack surface

## Governance

- This constitution supersedes all other development practices for the VNIDS project.
- Amendments require: (1) written proposal, (2) review by core maintainers, (3) documented rationale, (4) migration plan for existing code if applicable.
- All pull requests and code reviews MUST verify compliance with these principles.
- Complexity beyond these guidelines MUST be justified in writing with clear rationale.
- Version numbering follows semantic versioning: MAJOR (breaking governance changes), MINOR (new principles/sections), PATCH (clarifications).

**Version**: 1.0.0 | **Ratified**: 2026-01-15 | **Last Amended**: 2026-01-15
