#!/bin/bash
# Build Suricata for Linux ARM64 (for testing in Docker/QEMU)
# This builds a native Linux version, not Android

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SURICATA_DIR="$SCRIPT_DIR/.."
BUILD_DIR="$SURICATA_DIR/build-linux-arm64"
DEPS_DIR="$BUILD_DIR/deps"
SURICATA_VERSION="7.0.8"
SURICATA_SRC="$BUILD_DIR/suricata-${SURICATA_VERSION}"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

log_info() { echo -e "${GREEN}[INFO]${NC} $1"; }
log_warn() { echo -e "${YELLOW}[WARN]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; exit 1; }

# Detect if we're running natively on ARM64 or need cross-compilation
NATIVE_BUILD=false
if [ "$(uname -m)" = "aarch64" ]; then
    NATIVE_BUILD=true
    log_info "Native ARM64 build detected"
fi

# Set up cross-compilation if needed
setup_cross_compile() {
    if [ "$NATIVE_BUILD" = true ]; then
        export CC=gcc
        export CXX=g++
        export AR=ar
        export RANLIB=ranlib
        export STRIP=strip
        HOST_FLAG=""
    else
        # Check for cross compiler
        if ! command -v aarch64-linux-gnu-gcc &> /dev/null; then
            log_error "Cross compiler not found. Install with: apt install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu"
        fi
        export CC=aarch64-linux-gnu-gcc
        export CXX=aarch64-linux-gnu-g++
        export AR=aarch64-linux-gnu-ar
        export RANLIB=aarch64-linux-gnu-ranlib
        export STRIP=aarch64-linux-gnu-strip
        HOST_FLAG="--host=aarch64-linux-gnu"
    fi

    export CFLAGS="-O2 -fPIC"
    export CXXFLAGS="$CFLAGS"
    export LDFLAGS="-L$DEPS_DIR/lib"
    export PKG_CONFIG_PATH="$DEPS_DIR/lib/pkgconfig"
    export PKG_CONFIG_LIBDIR="$DEPS_DIR/lib/pkgconfig"
}

# Download Suricata
download_suricata() {
    if [ -d "$SURICATA_SRC" ]; then
        log_info "Suricata source already exists"
        return
    fi

    log_info "Downloading Suricata ${SURICATA_VERSION}..."
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"

    wget -q --show-progress "https://www.openinfosecfoundation.org/download/suricata-${SURICATA_VERSION}.tar.gz" \
        -O "suricata-${SURICATA_VERSION}.tar.gz"
    tar -xzf "suricata-${SURICATA_VERSION}.tar.gz"
    rm "suricata-${SURICATA_VERSION}.tar.gz"
}

# Build dependencies
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
        ./configure $HOST_FLAG --prefix="$DEPS_DIR" \
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
        ./configure $HOST_FLAG --prefix="$DEPS_DIR" \
            --disable-shared --enable-static
        make -j$(nproc) && make install
        cd "$BUILD_DIR"
        rm -rf "yaml-${YAML_VERSION}" "yaml-${YAML_VERSION}.tar.gz"
    fi

    # jansson
    if [ ! -f "$DEPS_DIR/lib/libjansson.a" ]; then
        log_info "Building jansson..."
        JANSSON_VERSION="2.14"
        wget -q "https://github.com/akheron/jansson/releases/download/v${JANSSON_VERSION}/jansson-${JANSSON_VERSION}.tar.gz"
        tar -xzf "jansson-${JANSSON_VERSION}.tar.gz"
        cd "jansson-${JANSSON_VERSION}"
        ./configure $HOST_FLAG --prefix="$DEPS_DIR" \
            --disable-shared --enable-static
        make -j$(nproc) && make install
        cd "$BUILD_DIR"
        rm -rf "jansson-${JANSSON_VERSION}" "jansson-${JANSSON_VERSION}.tar.gz"
    fi

    # libpcap
    if [ ! -f "$DEPS_DIR/lib/libpcap.a" ]; then
        log_info "Building libpcap..."
        LIBPCAP_VERSION="1.10.4"
        wget -q "https://www.tcpdump.org/release/libpcap-${LIBPCAP_VERSION}.tar.gz"
        tar -xzf "libpcap-${LIBPCAP_VERSION}.tar.gz"
        cd "libpcap-${LIBPCAP_VERSION}"
        ./configure $HOST_FLAG --prefix="$DEPS_DIR" \
            --disable-shared --enable-static \
            --without-libnl --disable-dbus
        make -j$(nproc) && make install
        cd "$BUILD_DIR"
        rm -rf "libpcap-${LIBPCAP_VERSION}" "libpcap-${LIBPCAP_VERSION}.tar.gz"
    fi

    log_info "Dependencies built"
}

# Configure Suricata
configure_suricata() {
    log_info "Configuring Suricata..."
    cd "$SURICATA_SRC"

    export CFLAGS="$CFLAGS -I$DEPS_DIR/include"
    export LDFLAGS="$LDFLAGS -L$DEPS_DIR/lib"

    # Cache variables for cross-compilation
    if [ "$NATIVE_BUILD" = false ]; then
        export ac_cv_func_malloc_0_nonnull=yes
        export ac_cv_func_realloc_0_nonnull=yes
    fi

    ./configure \
        $HOST_FLAG \
        --prefix=/usr/local \
        --sysconfdir=/etc/suricata \
        --localstatedir=/var \
        --disable-shared \
        --enable-static \
        --disable-gccmarch-native \
        --enable-rust \
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

# Install
install_suricata() {
    log_info "Installing Suricata..."

    OUTPUT_DIR="$SURICATA_DIR/out/linux-arm64"
    mkdir -p "$OUTPUT_DIR/bin" "$OUTPUT_DIR/etc" "$OUTPUT_DIR/rules"

    cd "$SURICATA_SRC"
    cp src/suricata "$OUTPUT_DIR/bin/"
    $STRIP "$OUTPUT_DIR/bin/suricata" 2>/dev/null || true

    # Copy config
    if [ -d "$SURICATA_DIR/config" ]; then
        cp -r "$SURICATA_DIR/config/"* "$OUTPUT_DIR/etc/" 2>/dev/null || true
    fi
    if [ -d "$SURICATA_DIR/rules" ]; then
        cp -r "$SURICATA_DIR/rules/"* "$OUTPUT_DIR/rules/" 2>/dev/null || true
    fi

    log_info "Installed to: $OUTPUT_DIR"
    log_info "Binary size: $(du -h "$OUTPUT_DIR/bin/suricata" | cut -f1)"
}

# Main
main() {
    log_info "=== Building Suricata for Linux ARM64 ==="

    setup_cross_compile
    download_suricata
    build_dependencies
    configure_suricata
    build_suricata
    install_suricata

    log_info "=== Build complete ==="
}

main "$@"
