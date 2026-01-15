#!/bin/bash
# Run Android integration tests in Docker environment
# This script starts the emulator and runs tests

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORKSPACE="${WORKSPACE:-/workspace}"
TEST_TIMEOUT="${TEST_TIMEOUT:-300}"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

log_info() { echo -e "${GREEN}[INFO]${NC} $1"; }
log_warn() { echo -e "${YELLOW}[WARN]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }

cleanup() {
    log_info "Cleaning up..."
    # Kill emulator if running
    adb emu kill 2>/dev/null || true
    # Kill any background processes
    pkill -f "emulator" 2>/dev/null || true
}

trap cleanup EXIT

# Start emulator in background
start_emulator() {
    log_info "Starting Android emulator..."

    # Check if KVM is available
    if [ -e /dev/kvm ]; then
        log_info "KVM acceleration available"
        EMU_ACCEL="-accel on"
    else
        log_warn "KVM not available, using software emulation (slower)"
        EMU_ACCEL="-accel off"
    fi

    # Start emulator headless
    emulator -avd vnids-test-avd \
        -no-window \
        -no-audio \
        -no-boot-anim \
        -gpu swiftshader_indirect \
        -no-snapshot \
        -memory 2048 \
        $EMU_ACCEL \
        &

    EMU_PID=$!
    log_info "Emulator started with PID: $EMU_PID"

    # Wait for emulator to boot
    log_info "Waiting for emulator to boot..."
    local timeout=180
    local elapsed=0

    while [ $elapsed -lt $timeout ]; do
        if adb wait-for-device shell getprop sys.boot_completed 2>/dev/null | grep -q "1"; then
            log_info "Emulator booted successfully"
            return 0
        fi
        sleep 5
        elapsed=$((elapsed + 5))
        echo -n "."
    done

    log_error "Emulator failed to boot within ${timeout}s"
    return 1
}

# Deploy VNIDS binaries to emulator
deploy_binaries() {
    log_info "Deploying VNIDS binaries to emulator..."

    # Wait for device
    adb wait-for-device

    # Get root access
    adb root || log_warn "Could not get root access"
    sleep 2

    # Create directories
    adb shell "mkdir -p /data/vnids/bin /data/vnids/etc /data/vnids/rules /data/vnids/var/log /data/vnids/var/run"

    # Push binaries
    if [ -f "$WORKSPACE/out/android-arm64/bin/vnidsd" ]; then
        adb push "$WORKSPACE/out/android-arm64/bin/vnidsd" /data/vnids/bin/
        adb shell chmod 755 /data/vnids/bin/vnidsd
    else
        log_warn "vnidsd binary not found"
    fi

    if [ -f "$WORKSPACE/suricata/out/android-arm64/bin/suricata" ]; then
        adb push "$WORKSPACE/suricata/out/android-arm64/bin/suricata" /data/vnids/bin/
        adb shell chmod 755 /data/vnids/bin/suricata
    else
        log_warn "suricata binary not found"
    fi

    # Push test configuration
    if [ -d "$WORKSPACE/tests/data/config" ]; then
        adb push "$WORKSPACE/tests/data/config/vnids-test.yaml" /data/vnids/etc/vnids.yaml
    fi

    # Push test rules
    if [ -d "$WORKSPACE/tests/data/rules" ]; then
        adb push "$WORKSPACE/tests/data/rules/"*.rules /data/vnids/rules/
    fi

    log_info "Binaries deployed successfully"
}

# Run unit tests (host-side)
run_unit_tests() {
    log_info "Running unit tests..."

    cd "$WORKSPACE/tests"

    if [ -f "requirements.txt" ]; then
        pip3 install -q -r requirements.txt 2>/dev/null || true
    fi

    python3 -m pytest unit/ -v --tb=short --timeout=$TEST_TIMEOUT \
        --junitxml="$WORKSPACE/out/test-results/unit-tests.xml" || {
        log_error "Unit tests failed"
        return 1
    }

    log_info "Unit tests passed"
}

# Run integration tests (device-side)
run_integration_tests() {
    log_info "Running integration tests..."

    cd "$WORKSPACE/tests"

    python3 -m pytest integration/ -v --tb=short --timeout=$TEST_TIMEOUT \
        --junitxml="$WORKSPACE/out/test-results/integration-tests.xml" || {
        log_error "Integration tests failed"
        return 1
    }

    log_info "Integration tests passed"
}

# Main
main() {
    log_info "=== VNIDS Android Integration Test Suite ==="

    # Create output directory
    mkdir -p "$WORKSPACE/out/test-results"

    # Run unit tests first (no emulator needed)
    run_unit_tests

    # Start emulator for integration tests
    start_emulator

    # Deploy binaries
    deploy_binaries

    # Run integration tests
    run_integration_tests

    log_info "=== All tests completed successfully ==="
}

main "$@"
