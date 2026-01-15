#!/bin/bash
# Start Android emulator for local testing
# Usage: ./emulator-start.sh [avd-name]

set -e

AVD_NAME="${1:-vnids-test-avd}"
ANDROID_SDK="${ANDROID_SDK_ROOT:-${ANDROID_HOME:-}}"
DEFAULT_SDK_DIR="${HOME}/Android/Sdk"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

log_info() { echo -e "${GREEN}[INFO]${NC} $1"; }
log_warn() { echo -e "${YELLOW}[WARN]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; exit 1; }

install_android_sdk() {
    local sdk_dir="$1"
    local tools_url="https://dl.google.com/android/repository/commandlinetools-linux-11076708_latest.zip"
    local tmp_dir

    if ! command -v unzip &> /dev/null; then
        log_error "unzip not found. Please install it first."
    fi
    if ! command -v curl &> /dev/null && ! command -v wget &> /dev/null; then
        log_error "curl or wget is required to download Android SDK."
    fi
    if ! command -v java &> /dev/null; then
        log_error "Java not found. Please install OpenJDK 11+."
    fi

    log_info "Installing Android SDK to: ${sdk_dir}"
    mkdir -p "${sdk_dir}/cmdline-tools"
    tmp_dir="$(mktemp -d)"
    trap 'rm -rf "${tmp_dir}"' RETURN

    log_info "Downloading Android SDK command-line tools..."
    if command -v curl &> /dev/null; then
        curl -fsSL "${tools_url}" -o "${tmp_dir}/cmdline-tools.zip"
    else
        wget -qO "${tmp_dir}/cmdline-tools.zip" "${tools_url}"
    fi

    unzip -q "${tmp_dir}/cmdline-tools.zip" -d "${tmp_dir}"
    rm -rf "${sdk_dir}/cmdline-tools/latest"
    mv "${tmp_dir}/cmdline-tools" "${sdk_dir}/cmdline-tools/latest"

    export ANDROID_SDK_ROOT="${sdk_dir}"
    export ANDROID_HOME="${sdk_dir}"
    export PATH="${PATH}:${ANDROID_SDK_ROOT}/cmdline-tools/latest/bin:${ANDROID_SDK_ROOT}/platform-tools:${ANDROID_SDK_ROOT}/emulator"

    log_info "Accepting Android SDK licenses..."
    yes | sdkmanager --licenses > /dev/null 2>&1 || true
    sdkmanager --sdk_root="${sdk_dir}" --update
    sdkmanager --sdk_root="${sdk_dir}" \
        "platform-tools" \
        "platforms;android-31" \
        "emulator" \
        "system-images;android-31;google_apis;arm64-v8a"
}

ensure_android_sdk() {
    if [ -z "${ANDROID_SDK}" ]; then
        ANDROID_SDK="${DEFAULT_SDK_DIR}"
    fi

    if [ ! -d "${ANDROID_SDK}" ]; then
        log_warn "Android SDK not found. Installing to ${ANDROID_SDK}."
        install_android_sdk "${ANDROID_SDK}"
        return
    fi

    export ANDROID_SDK_ROOT="${ANDROID_SDK}"
    export ANDROID_HOME="${ANDROID_SDK}"
    export PATH="${PATH}:${ANDROID_SDK}/cmdline-tools/latest/bin:${ANDROID_SDK}/platform-tools:${ANDROID_SDK}/emulator"

    if ! command -v sdkmanager &> /dev/null; then
        log_warn "Android SDK command-line tools missing. Installing..."
        install_android_sdk "${ANDROID_SDK}"
    fi
}

ensure_android_sdk

# Check if emulator command exists
if ! command -v emulator &> /dev/null; then
    export PATH="$PATH:$ANDROID_SDK/emulator:$ANDROID_SDK/platform-tools"
fi
if ! command -v emulator &> /dev/null; then
    log_error "Android emulator not found. Install SDK package: emulator"
fi
if ! command -v adb &> /dev/null; then
    log_error "adb not found. Install SDK package: platform-tools"
fi

# Check if AVD exists
if ! emulator -list-avds 2>/dev/null | grep -q "$AVD_NAME"; then
    log_warn "AVD '$AVD_NAME' not found. Creating..."

    # Check for system image
    SYSTEM_IMAGE="system-images;android-31;google_apis;arm64-v8a"
    SYSTEM_IMAGE_DIR="${ANDROID_SDK}/system-images/android-31/google_apis/arm64-v8a"
    if [ ! -d "${SYSTEM_IMAGE_DIR}" ]; then
        log_info "Installing system image..."
        sdkmanager --sdk_root="${ANDROID_SDK}" "$SYSTEM_IMAGE"
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
