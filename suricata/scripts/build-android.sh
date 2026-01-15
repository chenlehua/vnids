#!/bin/bash
# Build Suricata for Android ARM64
# This script downloads and cross-compiles Suricata 7.x for Android

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SURICATA_DIR="$SCRIPT_DIR/.."
BUILD_DIR="$SURICATA_DIR/build-android"
DEPS_DIR="$BUILD_DIR/deps"
SURICATA_VERSION="7.0.8"
SURICATA_SRC="$BUILD_DIR/suricata-${SURICATA_VERSION}"

# Android NDK settings
ANDROID_NDK="${ANDROID_NDK:-$ANDROID_NDK_HOME}"
ANDROID_API="${ANDROID_API:-31}"
ANDROID_ABI="${ANDROID_ABI:-arm64-v8a}"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

log_info() { echo -e "${GREEN}[INFO]${NC} $1"; }
log_warn() { echo -e "${YELLOW}[WARN]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; exit 1; }

# Check prerequisites
check_prerequisites() {
    log_info "Checking prerequisites..."

    if [ -z "$ANDROID_NDK" ]; then
        log_error "ANDROID_NDK or ANDROID_NDK_HOME environment variable not set"
    fi

    if [ ! -d "$ANDROID_NDK" ]; then
        log_error "Android NDK not found at: $ANDROID_NDK"
    fi

    # Check for required tools
    for cmd in wget tar make pkg-config; do
        if ! command -v $cmd &> /dev/null; then
            log_error "$cmd is required but not installed"
        fi
    done

    log_info "Prerequisites OK"
}

# Set up Android NDK toolchain
setup_toolchain() {
    log_info "Setting up Android NDK toolchain..."

    export TOOLCHAIN="$ANDROID_NDK/toolchains/llvm/prebuilt/linux-x86_64"
    export TARGET="aarch64-linux-android"
    export API=$ANDROID_API

    export AR="$TOOLCHAIN/bin/llvm-ar"
    export CC="$TOOLCHAIN/bin/${TARGET}${API}-clang"
    export CXX="$TOOLCHAIN/bin/${TARGET}${API}-clang++"
    export LD="$TOOLCHAIN/bin/ld"
    export RANLIB="$TOOLCHAIN/bin/llvm-ranlib"
    export STRIP="$TOOLCHAIN/bin/llvm-strip"

    export CFLAGS="-O2 -fPIC -DANDROID -D__ANDROID_API__=$API"
    export CXXFLAGS="$CFLAGS"
    export LDFLAGS="-L$DEPS_DIR/lib"
    export PKG_CONFIG_PATH="$DEPS_DIR/lib/pkgconfig"
    export PKG_CONFIG_LIBDIR="$DEPS_DIR/lib/pkgconfig"

    if [ ! -f "$CC" ]; then
        log_error "Clang compiler not found at: $CC"
    fi

    log_info "Toolchain configured: $CC"
}

# Set up Rust for Android cross-compilation
setup_rust() {
    log_info "Setting up Rust for Android cross-compilation..."

    RUST_TARGET="aarch64-linux-android"

    # Add Android target if not present
    if ! rustup target list --installed | grep -q "$RUST_TARGET"; then
        log_info "Adding Rust target: $RUST_TARGET"
        rustup target add "$RUST_TARGET"
    fi

    # Configure Cargo to use Android NDK linker
    mkdir -p "$HOME/.cargo"
    CARGO_CONFIG="$HOME/.cargo/config.toml"

    # Check if Android linker config already exists
    if ! grep -q "\[target.aarch64-linux-android\]" "$CARGO_CONFIG" 2>/dev/null; then
        log_info "Configuring Cargo for Android cross-compilation..."
        cat >> "$CARGO_CONFIG" << EOF

[target.aarch64-linux-android]
linker = "$TOOLCHAIN/bin/${TARGET}${API}-clang"
ar = "$TOOLCHAIN/bin/llvm-ar"
EOF
    fi

    # Set environment for Rust
    export CARGO_TARGET_AARCH64_LINUX_ANDROID_LINKER="$TOOLCHAIN/bin/${TARGET}${API}-clang"
    export CARGO_TARGET_AARCH64_LINUX_ANDROID_AR="$TOOLCHAIN/bin/llvm-ar"

    log_info "Rust configured for $RUST_TARGET"
}

# Download and extract Suricata
download_suricata() {
    if [ -d "$SURICATA_SRC" ]; then
        log_info "Suricata source already exists, skipping download"
        return
    fi

    log_info "Downloading Suricata ${SURICATA_VERSION}..."
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"

    SURICATA_URL="https://www.openinfosecfoundation.org/download/suricata-${SURICATA_VERSION}.tar.gz"
    wget -q --show-progress "$SURICATA_URL" -O "suricata-${SURICATA_VERSION}.tar.gz"

    log_info "Extracting Suricata..."
    tar -xzf "suricata-${SURICATA_VERSION}.tar.gz"
    rm "suricata-${SURICATA_VERSION}.tar.gz"
}

# Build dependencies (libpcre2, libyaml, jansson, libpcap)
build_dependencies() {
    log_info "Building dependencies..."
    mkdir -p "$DEPS_DIR"
    cd "$BUILD_DIR"

    # libpcre2
    if [ ! -f "$DEPS_DIR/lib/libpcre2-8.a" ]; then
        log_info "Building libpcre2..."
        PCRE2_VERSION="10.42"
        wget -q "https://github.com/PCRE2Project/pcre2/releases/download/pcre2-${PCRE2_VERSION}/pcre2-${PCRE2_VERSION}.tar.gz"
        tar -xzf "pcre2-${PCRE2_VERSION}.tar.gz"
        cd "pcre2-${PCRE2_VERSION}"
        ./configure --host=$TARGET --prefix="$DEPS_DIR" \
            --disable-shared --enable-static \
            --disable-pcre2grep-libz --disable-pcre2grep-libbz2
        make -j$(nproc) && make install
        cd "$BUILD_DIR"
        rm -rf "pcre2-${PCRE2_VERSION}" "pcre2-${PCRE2_VERSION}.tar.gz"
    fi

    # libyaml
    if [ ! -f "$DEPS_DIR/lib/libyaml.a" ]; then
        log_info "Building libyaml..."
        YAML_VERSION="0.2.5"
        wget -q "https://github.com/yaml/libyaml/releases/download/${YAML_VERSION}/yaml-${YAML_VERSION}.tar.gz"
        tar -xzf "yaml-${YAML_VERSION}.tar.gz"
        cd "yaml-${YAML_VERSION}"
        ./configure --host=$TARGET --prefix="$DEPS_DIR" \
            --disable-shared --enable-static
        make -j$(nproc) && make install
        cd "$BUILD_DIR"
        rm -rf "yaml-${YAML_VERSION}" "yaml-${YAML_VERSION}.tar.gz"
    fi

    # jansson (JSON library)
    if [ ! -f "$DEPS_DIR/lib/libjansson.a" ]; then
        log_info "Building jansson..."
        JANSSON_VERSION="2.14"
        wget -q "https://github.com/akheron/jansson/releases/download/v${JANSSON_VERSION}/jansson-${JANSSON_VERSION}.tar.gz"
        tar -xzf "jansson-${JANSSON_VERSION}.tar.gz"
        cd "jansson-${JANSSON_VERSION}"
        ./configure --host=$TARGET --prefix="$DEPS_DIR" \
            --disable-shared --enable-static
        make -j$(nproc) && make install
        cd "$BUILD_DIR"
        rm -rf "jansson-${JANSSON_VERSION}" "jansson-${JANSSON_VERSION}.tar.gz"
    fi

    # libpcap (minimal, for packet capture)
    if [ ! -f "$DEPS_DIR/lib/libpcap.a" ]; then
        log_info "Building libpcap..."
        LIBPCAP_VERSION="1.10.4"
        wget -q "https://www.tcpdump.org/release/libpcap-${LIBPCAP_VERSION}.tar.gz"
        tar -xzf "libpcap-${LIBPCAP_VERSION}.tar.gz"
        cd "libpcap-${LIBPCAP_VERSION}"
        ./configure --host=$TARGET --prefix="$DEPS_DIR" \
            --disable-shared --enable-static \
            --without-libnl --disable-dbus
        make -j$(nproc) && make install
        cd "$BUILD_DIR"
        rm -rf "libpcap-${LIBPCAP_VERSION}" "libpcap-${LIBPCAP_VERSION}.tar.gz"
    fi

    log_info "Dependencies built successfully"
}

# Configure Suricata
configure_suricata() {
    log_info "Configuring Suricata for Android..."
    cd "$SURICATA_SRC"

    # Add dependency include/lib paths
    export CFLAGS="$CFLAGS -I$DEPS_DIR/include"
    export LDFLAGS="$LDFLAGS -L$DEPS_DIR/lib"

    # Android Bionic libc has pthreads built-in (no separate libpthread)
    # Tell configure that pthread functions are available without -lpthread
    export ac_cv_lib_pthread_pthread_create=yes
    export ac_cv_search_pthread_create="none required"
    export PTHREAD_LIBS=""
    export PTHREAD_CFLAGS=""

    # Cache variables for cross-compiled static libraries
    # Configure's link tests fail when cross-compiling, so we bypass them
    export ac_cv_lib_jansson_json_dump_callback=yes
    export ac_cv_lib_yaml_yaml_parser_initialize=yes
    export ac_cv_lib_pcre2_8_pcre2_compile_8=yes
    export ac_cv_lib_pcap_pcap_open_dead=yes
    export ac_cv_lib_pcap_pcap_open_live=yes

    # Tell configure that malloc/realloc work correctly on Android
    # Without this, autoconf uses rpl_malloc/rpl_realloc replacements that don't exist
    export ac_cv_func_malloc_0_nonnull=yes
    export ac_cv_func_realloc_0_nonnull=yes

    # Ensure static libraries are linked correctly
    export LIBS="-L$DEPS_DIR/lib -ljansson -lyaml -lpcre2-8 -lpcap"

    # Set Rust target for Suricata's build system
    # Note: Android uses "aarch64-linux-android", not "aarch64-unknown-linux-android"
    export RUST_TARGET="aarch64-linux-android"

    ./configure \
        --host=$TARGET \
        --prefix=/data/vnids/suricata \
        --sysconfdir=/data/vnids/etc \
        --localstatedir=/data/vnids/var \
        --disable-shared \
        --enable-static \
        --disable-gccmarch-native \
        --enable-rust \
        --with-rust-target=aarch64-linux-android \
        --disable-python \
        --disable-suricata-update \
        --disable-geoip \
        --disable-libmagic \
        --disable-nfqueue \
        --disable-nflog \
        --disable-lua \
        --disable-luajit \
        --disable-hiredis \
        --enable-unix-socket \
        --enable-af-packet \
        --with-libpcre2-includes="$DEPS_DIR/include" \
        --with-libpcre2-libraries="$DEPS_DIR/lib" \
        --with-libyaml-includes="$DEPS_DIR/include" \
        --with-libyaml-libraries="$DEPS_DIR/lib" \
        --with-libjansson-includes="$DEPS_DIR/include" \
        --with-libjansson-libraries="$DEPS_DIR/lib" \
        --with-libpcap-includes="$DEPS_DIR/include" \
        --with-libpcap-libraries="$DEPS_DIR/lib"

    # Fix Rust target in generated Makefiles
    # Suricata incorrectly uses "aarch64-unknown-linux-android" but Android uses "aarch64-linux-android"
    log_info "Patching Makefiles to fix Rust target..."
    find . -name Makefile -exec sed -i 's/aarch64-unknown-linux-android/aarch64-linux-android/g' {} \;

    # Remove -lrt and -lpthread from Makefiles
    # Android Bionic has these built into libc, no separate libraries exist
    log_info "Patching Makefiles to remove glibc-specific libraries..."
    find . -name Makefile -exec sed -i 's/-lrt//g; s/-lpthread//g' {} \;

    log_info "Configuration complete"
}

# Build Suricata
build_suricata() {
    log_info "Building Suricata..."
    cd "$SURICATA_SRC"

    make -j$(nproc)

    log_info "Build complete"
}

# Install to output directory
install_suricata() {
    log_info "Installing Suricata to output directory..."

    OUTPUT_DIR="$SURICATA_DIR/out/android-arm64"
    mkdir -p "$OUTPUT_DIR/bin" "$OUTPUT_DIR/etc" "$OUTPUT_DIR/rules"

    cd "$SURICATA_SRC"

    # Copy binary
    cp src/suricata "$OUTPUT_DIR/bin/"
    $STRIP "$OUTPUT_DIR/bin/suricata"

    # Copy configuration
    cp -r "$SURICATA_DIR/config/"* "$OUTPUT_DIR/etc/"
    cp -r "$SURICATA_DIR/rules/"* "$OUTPUT_DIR/rules/"

    log_info "Suricata installed to: $OUTPUT_DIR"
    log_info "Binary size: $(du -h "$OUTPUT_DIR/bin/suricata" | cut -f1)"
}

# Main
main() {
    log_info "=== Building Suricata for Android ARM64 ==="

    check_prerequisites
    setup_toolchain
    setup_rust
    download_suricata
    build_dependencies
    configure_suricata
    build_suricata
    install_suricata

    log_info "=== Build complete ==="
}

main "$@"
