#!/bin/bash
# Start Android emulator for local testing
# Usage: ./emulator-start.sh [avd-name]

set -e

AVD_NAME="${1:-vnids-test-avd}"
ANDROID_SDK="${ANDROID_SDK_ROOT:-$ANDROID_HOME}"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

log_info() { echo -e "${GREEN}[INFO]${NC} $1"; }
log_warn() { echo -e "${YELLOW}[WARN]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; exit 1; }

# Check if Android SDK is available
if [ -z "$ANDROID_SDK" ] || [ ! -d "$ANDROID_SDK" ]; then
    log_error "Android SDK not found. Set ANDROID_SDK_ROOT or ANDROID_HOME"
fi

# Check if emulator command exists
if ! command -v emulator &> /dev/null; then
    export PATH="$PATH:$ANDROID_SDK/emulator:$ANDROID_SDK/platform-tools"
fi

# Check if AVD exists
if ! emulator -list-avds 2>/dev/null | grep -q "$AVD_NAME"; then
    log_warn "AVD '$AVD_NAME' not found. Creating..."

    # Check for system image
    SYSTEM_IMAGE="system-images;android-31;google_apis;arm64-v8a"
    if ! sdkmanager --list_installed 2>/dev/null | grep -q "arm64-v8a"; then
        log_info "Installing system image..."
        sdkmanager "$SYSTEM_IMAGE"
    fi

    # Create AVD
    echo "no" | avdmanager create avd \
        --name "$AVD_NAME" \
        --package "$SYSTEM_IMAGE" \
        --device "pixel_4" \
        --force
fi

# Check KVM availability
if [ -e /dev/kvm ] && [ -r /dev/kvm ] && [ -w /dev/kvm ]; then
    log_info "KVM acceleration available"
    ACCEL_OPTS="-accel on"
else
    log_warn "KVM not available or not accessible. Emulator will be slow."
    log_warn "Try: sudo usermod -aG kvm $USER && newgrp kvm"
    ACCEL_OPTS="-accel off"
fi

# Check if emulator is already running
if adb devices 2>/dev/null | grep -q "emulator"; then
    log_warn "Emulator already running"
    adb devices
    exit 0
fi

log_info "Starting emulator: $AVD_NAME"

# Start emulator
emulator -avd "$AVD_NAME" \
    -no-snapshot-save \
    -no-boot-anim \
    -gpu auto \
    -memory 2048 \
    $ACCEL_OPTS \
    &

EMU_PID=$!
log_info "Emulator PID: $EMU_PID"

# Wait for boot
log_info "Waiting for emulator to boot..."
timeout=120
elapsed=0

while [ $elapsed -lt $timeout ]; do
    if adb wait-for-device shell getprop sys.boot_completed 2>/dev/null | grep -q "1"; then
        log_info "Emulator booted successfully!"
        adb devices
        exit 0
    fi
    sleep 2
    elapsed=$((elapsed + 2))
    echo -n "."
done

log_error "Emulator failed to boot within ${timeout}s"
