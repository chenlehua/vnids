#!/bin/bash
# Build Suricata for Yocto/Buildroot Linux ARM64
# This script downloads and cross-compiles Suricata 7.x for embedded Linux

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SURICATA_DIR="$SCRIPT_DIR/.."
BUILD_DIR="$SURICATA_DIR/build-yocto"
DEPS_DIR="$BUILD_DIR/deps"
SURICATA_VERSION="7.0.8"
SURICATA_SRC="$BUILD_DIR/suricata-${SURICATA_VERSION}"

# Cross-compilation settings
CROSS_COMPILE="${CROSS_COMPILE:-aarch64-linux-gnu-}"
SYSROOT="${SYSROOT:-}"
TARGET_ARCH="${TARGET_ARCH:-aarch64-linux-gnu}"

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

    # Check for cross-compiler
    if ! command -v ${CROSS_COMPILE}gcc &> /dev/null; then
        log_error "Cross-compiler ${CROSS_COMPILE}gcc not found. Install aarch64-linux-gnu toolchain or set CROSS_COMPILE"
    fi

    # Check for required tools
    for cmd in wget tar make pkg-config; do
        if ! command -v $cmd &> /dev/null; then
            log_error "$cmd is required but not installed"
        fi
    done

    log_info "Prerequisites OK"
    log_info "Using cross-compiler: ${CROSS_COMPILE}gcc"
}

# Set up cross-compilation environment
setup_toolchain() {
    log_info "Setting up cross-compilation environment..."

    export CC="${CROSS_COMPILE}gcc"
    export CXX="${CROSS_COMPILE}g++"
    export AR="${CROSS_COMPILE}ar"
    export RANLIB="${CROSS_COMPILE}ranlib"
    export STRIP="${CROSS_COMPILE}strip"
    export LD="${CROSS_COMPILE}ld"

    export CFLAGS="-O2 -fPIC"
    export CXXFLAGS="$CFLAGS"
    export LDFLAGS="-L$DEPS_DIR/lib"
    export PKG_CONFIG_PATH="$DEPS_DIR/lib/pkgconfig"
    export PKG_CONFIG_LIBDIR="$DEPS_DIR/lib/pkgconfig"

    if [ -n "$SYSROOT" ]; then
        export CFLAGS="$CFLAGS --sysroot=$SYSROOT"
        export LDFLAGS="$LDFLAGS --sysroot=$SYSROOT"
        log_info "Using sysroot: $SYSROOT"
    fi

    log_info "Toolchain configured"
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
        ./configure --host=$TARGET_ARCH --prefix="$DEPS_DIR" \
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
        ./configure --host=$TARGET_ARCH --prefix="$DEPS_DIR" \
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
        ./configure --host=$TARGET_ARCH --prefix="$DEPS_DIR" \
            --disable-shared --enable-static
        make -j$(nproc) && make install
        cd "$BUILD_DIR"
        rm -rf "jansson-${JANSSON_VERSION}" "jansson-${JANSSON_VERSION}.tar.gz"
    fi

    # libpcap (for packet capture)
    if [ ! -f "$DEPS_DIR/lib/libpcap.a" ]; then
        log_info "Building libpcap..."
        LIBPCAP_VERSION="1.10.4"
        wget -q "https://www.tcpdump.org/release/libpcap-${LIBPCAP_VERSION}.tar.gz"
        tar -xzf "libpcap-${LIBPCAP_VERSION}.tar.gz"
        cd "libpcap-${LIBPCAP_VERSION}"
        ./configure --host=$TARGET_ARCH --prefix="$DEPS_DIR" \
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
    log_info "Configuring Suricata for Yocto Linux..."
    cd "$SURICATA_SRC"

    # Add dependency include/lib paths
    export CFLAGS="$CFLAGS -I$DEPS_DIR/include"
    export LDFLAGS="$LDFLAGS -L$DEPS_DIR/lib"

    ./configure \
        --host=$TARGET_ARCH \
        --prefix=/usr \
        --sysconfdir=/etc/vnids \
        --localstatedir=/var \
        --disable-shared \
        --enable-static \
        --disable-gccmarch-native \
        --disable-rust \
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

    OUTPUT_DIR="$SURICATA_DIR/out/yocto-arm64"
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
    log_info "=== Building Suricata for Yocto Linux ARM64 ==="

    check_prerequisites
    setup_toolchain
    download_suricata
    build_dependencies
    configure_suricata
    build_suricata
    install_suricata

    log_info "=== Build complete ==="
}

main "$@"
