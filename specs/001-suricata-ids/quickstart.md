# Quickstart: VNIDS - Vehicle Network Intrusion Detection System

This guide walks you through building, installing, and running VNIDS on ARM64 platforms (Android or Yocto Linux).

## Prerequisites

### Development Environment

- **Host OS**: Linux (Ubuntu 22.04+ recommended) or macOS
- **C Compiler**: GCC 11+ or Clang 14+
- **CMake**: 3.20+
- **Android NDK**: r25c or later (for Android builds)
- **ARM64 Toolchain**: aarch64-linux-gnu-gcc (for Yocto builds)
- **Python**: 3.10+ (for integration tests)

### Target Device

- **Android**: 12+ (API 31+), ARMv8, root access
- **Yocto/Buildroot**: Linux ARM64, systemd or BusyBox init
- **RAM**: 128MB+ available
- **Storage**: 100MB+ free

## Step 1: Clone and Setup

```bash
# Clone repository
git clone https://github.com/your-org/vnids.git
cd vnidsru

# Set NDK path (for Android builds)
export ANDROID_NDK_HOME=/path/to/android-ndk-r25c

# Install Python dependencies for tests
pip install -r tests/requirements.txt
```

## Step 2: Build Suricata for Target Platform

### For Android ARM64

```bash
cd suricata
./scripts/build-android.sh

# Output: suricata/build/android/arm64-v8a/bin/suricata
```

### For Yocto/Buildroot Linux ARM64

```bash
cd suricata
./scripts/build-yocto.sh

# Output: suricata/build/linux/arm64/bin/suricata
```

This builds Suricata with:
- Custom SOME/IP, DoIP, GB/T 32960.3 parsers
- Unix socket EVE output
- Static linking (no external dependencies)

## Step 3: Build VNIDS Daemon (vnidsd)

### For Android ARM64

```bash
cd vnidsd

# Configure with Android NDK toolchain
cmake -B build-android \
    -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake \
    -DANDROID_ABI=arm64-v8a \
    -DANDROID_PLATFORM=android-31

# Build
cmake --build build-android --target vnidsd

# Output: build-android/vnidsd
```

### For Yocto/Buildroot Linux ARM64

```bash
cd vnidsd

# Configure with cross-compiler
cmake -B build-yocto \
    -DCMAKE_C_COMPILER=aarch64-linux-gnu-gcc \
    -DCMAKE_SYSTEM_NAME=Linux \
    -DCMAKE_SYSTEM_PROCESSOR=aarch64

# Build
cmake --build build-yocto --target vnidsd

# Output: build-yocto/vnidsd
```

## Step 4: Build CLI Tool (vnidsctl)

```bash
cd vnids-cli

# Same toolchain as vnidsd
cmake -B build-android \
    -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake \
    -DANDROID_ABI=arm64-v8a \
    -DANDROID_PLATFORM=android-31

cmake --build build-android --target vnidsctl
# OR for Yocto
cmake -B build-yocto \
    -DCMAKE_C_COMPILER=aarch64-linux-gnu-gcc

cmake --build build-yocto --target vnidsctl

# Output: build-*/vnidsctl
```

## Step 5: Deploy to Device

### Android Deployment

```bash
# Connect device via USB (USB debugging enabled, root access)
adb root
adb remount

# Push binaries
adb push vnidsd/build-android/vnidsd /system/bin/
adb push vnids-cli/build-android/vnidsctl /system/bin/
adb push suricata/build/android/arm64-v8a/bin/suricata /system/bin/

# Push configuration
adb shell mkdir -p /data/vnids/rules/baseline
adb push deploy/android/vnidsd.conf /data/vnids/
adb push suricata/suricata.yaml.template /data/vnids/suricata.yaml
adb push suricata/rules/baseline/ /data/vnids/rules/baseline/

# Push init script
adb push deploy/android/init.vnids.rc /system/etc/init/

# Set permissions
adb shell chmod 755 /system/bin/vnidsd
adb shell chmod 755 /system/bin/vnidsctl
adb shell chmod 755 /system/bin/suricata

# Reboot to start service
adb reboot
```

### Yocto/Buildroot Deployment

```bash
# Copy via SCP or other method to target
scp vnidsd/build-yocto/vnidsd root@<device-ip>:/usr/bin/
scp vnids-cli/build-yocto/vnidsctl root@<device-ip>:/usr/bin/
scp suricata/build/linux/arm64/bin/suricata root@<device-ip>:/usr/bin/

# Copy configuration
ssh root@<device-ip> "mkdir -p /etc/vnids/rules/baseline"
scp deploy/yocto/vnidsd.conf root@<device-ip>:/etc/vnids/
scp suricata/suricata.yaml.template root@<device-ip>:/etc/vnids/suricata.yaml
scp -r suricata/rules/baseline/ root@<device-ip>:/etc/vnids/rules/

# Install systemd service
scp deploy/yocto/vnids.service root@<device-ip>:/usr/lib/systemd/system/

# Enable and start
ssh root@<device-ip> "systemctl daemon-reload && systemctl enable vnids && systemctl start vnids"
```

## Step 6: Verify Installation

### Check Service Status

```bash
# Android
adb shell vnidsctl status

# Yocto
ssh root@<device-ip> vnidsctl status
```

Expected output:
```
VNIDS Status
============
vnidsd:     running (pid 1234)
suricata:   running (pid 1235)
uptime:     00:05:23
rules:      127 loaded
events:     42 (last hour)
memory:     58 MB / 64 MB
```

### Check Suricata Process

```bash
# Android
adb shell ps | grep suricata

# Yocto
ssh root@<device-ip> pgrep -a suricata
```

### Check IPC Sockets

```bash
# Android
adb shell ls -la /var/run/vnids/

# Yocto
ssh root@<device-ip> ls -la /var/run/vnids/
```

Expected:
```
srwxr-xr-x 1 root root 0 Jan 15 10:30 api.sock
srwxr-xr-x 1 root root 0 Jan 15 10:30 control.sock
srwxr-xr-x 1 root root 0 Jan 15 10:30 events.sock
```

## Step 7: Test Detection

### Generate Test Traffic

On your development machine:

```bash
cd tests/integration

# Run test suite (requires device IP)
python test_detection.py --device-ip <device-ip> --test all

# Or test specific attack types
python test_detection.py --device-ip <device-ip> --test syn_flood
python test_detection.py --device-ip <device-ip> --test someip_malformed
```

Expected output:
```
Running detection tests against <device-ip>...
✓ SYN flood detected (SID: 1000001)
✓ ACK flood detected (SID: 1000002)
✓ SOME/IP unauthorized access detected (SID: 1000101)
✓ DoIP routing activation detected (SID: 2000001)

4/4 tests passed
```

### View Events

```bash
# List recent events
vnidsctl events list --limit 10

# Filter by severity
vnidsctl events list --severity critical

# Export to JSON
vnidsctl events export --format json --output /tmp/events.json
```

## Step 8: Manage Rules

### View Loaded Rules

```bash
vnidsctl rules list

# Output:
# SID       REV  SEVERITY  CATEGORY           MESSAGE
# 1000001   1    critical  flood-attacks      TCP SYN flood detected
# 1000002   1    high      flood-attacks      TCP ACK flood detected
# ...
```

### Add Custom Rules

```bash
# Create custom rule file
cat > /tmp/custom.rules << 'EOF'
alert tcp any any -> any 8080 (msg:"HTTP traffic on non-standard port"; \
    flow:to_server,established; sid:9000001; rev:1;)
EOF

# Copy to device
adb push /tmp/custom.rules /data/vnids/rules/custom/
# OR
scp /tmp/custom.rules root@<device-ip>:/etc/vnids/rules/custom/

# Trigger hot reload
vnidsctl rules reload
```

### Validate Rules

```bash
vnidsctl rules validate /etc/vnids/rules/custom/custom.rules

# Output:
# Validating rules...
# ✓ Rule validation passed (1 rules)
```

## Configuration Reference

### vnidsd.conf

```ini
# /etc/vnids/vnidsd.conf or /data/vnids/vnidsd.conf

[general]
log_level = info
pid_file = /var/run/vnidsd.pid

[suricata]
binary = /usr/bin/suricata
config = /etc/vnids/suricata.yaml
rules_dir = /etc/vnids/rules
interface = eth0

[ipc]
socket_dir = /var/run/vnids
event_buffer_size = 32768

[storage]
database = /var/lib/vnids/events.db
retention_days = 7

[watchdog]
check_interval_ms = 500
heartbeat_timeout_s = 2
max_restart_attempts = 10
```

### suricata.yaml (Key Sections)

```yaml
# Interface configuration
af-packet:
  - interface: eth0
    cluster-id: 99
    cluster-type: cluster_flow
    defrag: yes

# EVE output to Unix socket
outputs:
  - eve-log:
      enabled: yes
      filetype: unix_stream
      filename: /var/run/vnids/events.sock
      types:
        - alert
        - flow
        - stats

# Memory limits (for 64MB budget)
detect:
  profile: low
  sgh-mpm-context: single

flow:
  hash-size: 32768
  prealloc: 5000
```

## Troubleshooting

### vnidsd Won't Start

```
Error: Failed to bind to /var/run/vnids/api.sock
```

**Solution**: Another instance may be running. Check and kill:
```bash
pkill vnidsd
rm -f /var/run/vnids/*.sock
vnidsd --config /etc/vnids/vnidsd.conf
```

### Suricata Crashes on Startup

```
Error: Suricata exited with status: 1
```

**Solution**: Check configuration:
```bash
suricata -T -c /etc/vnids/suricata.yaml
```

Common issues:
- Interface name incorrect
- Rules directory not found
- Insufficient memory

### No Events Appearing

1. Verify Suricata is capturing:
```bash
vnidsctl status
# Check "packets" counter is increasing
```

2. Verify rules are loaded:
```bash
vnidsctl rules list | wc -l
```

3. Check Suricata logs:
```bash
cat /var/log/suricata/suricata.log
```

### High Memory Usage

Edit `/etc/vnids/suricata.yaml`:
```yaml
detect:
  profile: low
flow:
  hash-size: 16384
  prealloc: 2500
```

Restart:
```bash
vnidsctl rules reload
# Or full restart
systemctl restart vnids
```

## CLI Command Reference

```bash
# Status
vnidsctl status              # Show service status
vnidsctl stats               # Show detailed statistics

# Events
vnidsctl events list         # List recent events
vnidsctl events tail         # Follow events in real-time
vnidsctl events export       # Export events to file

# Rules
vnidsctl rules list          # List loaded rules
vnidsctl rules reload        # Hot reload rules
vnidsctl rules validate      # Validate rule file

# Configuration
vnidsctl config show         # Show current configuration
vnidsctl config get <key>    # Get specific config value

# Control
vnidsctl start               # Start Suricata
vnidsctl stop                # Stop Suricata
vnidsctl restart             # Restart Suricata
```

## Next Steps

- [Full Configuration Guide](./docs/configuration.md)
- [Rule Writing Guide](./docs/rule-writing.md)
- [IPC Protocol Reference](./contracts/ipc-protocol.md)
- [Architecture Overview](../shared/docs/architecture.md)

## Support

- GitHub Issues: [github.com/your-org/vnids/issues](https://github.com/your-org/vnids/issues)
- Documentation: [github.com/your-org/vnids/wiki](https://github.com/your-org/vnids/wiki)
