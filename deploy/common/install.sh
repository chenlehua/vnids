#!/bin/bash
# VNIDS Installation Script
# Installs VNIDS on a Linux system
#
# Copyright (c) 2026 VNIDS Authors
# SPDX-License-Identifier: Apache-2.0

set -e

# Configuration
INSTALL_PREFIX="${INSTALL_PREFIX:-/usr/local}"
CONFIG_DIR="${CONFIG_DIR:-/etc/vnids}"
DATA_DIR="${DATA_DIR:-/var/lib/vnids}"
LOG_DIR="${LOG_DIR:-/var/log/vnids}"
RUN_DIR="${RUN_DIR:-/var/run/vnids}"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Check if running as root
check_root() {
    if [ "$EUID" -ne 0 ]; then
        log_error "This script must be run as root"
        exit 1
    fi
}

# Create directories
create_directories() {
    log_info "Creating directories..."

    mkdir -p "$CONFIG_DIR"
    mkdir -p "$CONFIG_DIR/suricata"
    mkdir -p "$CONFIG_DIR/rules"
    mkdir -p "$DATA_DIR"
    mkdir -p "$LOG_DIR"
    mkdir -p "$LOG_DIR/suricata"
    mkdir -p "$RUN_DIR"

    chmod 750 "$CONFIG_DIR"
    chmod 750 "$DATA_DIR"
    chmod 750 "$LOG_DIR"
    chmod 755 "$RUN_DIR"
}

# Install binaries
install_binaries() {
    log_info "Installing binaries..."

    if [ -f "build/vnidsd/vnidsd" ]; then
        install -m 755 "build/vnidsd/vnidsd" "$INSTALL_PREFIX/bin/"
    else
        log_warn "vnidsd binary not found in build directory"
    fi

    if [ -f "build/vnids-cli/vnids-cli" ]; then
        install -m 755 "build/vnids-cli/vnids-cli" "$INSTALL_PREFIX/bin/"
    else
        log_warn "vnids-cli binary not found in build directory"
    fi
}

# Install configuration files
install_config() {
    log_info "Installing configuration files..."

    # Main config
    if [ ! -f "$CONFIG_DIR/vnidsd.conf" ]; then
        install -m 640 "deploy/common/vnidsd.conf.example" "$CONFIG_DIR/vnidsd.conf"
    else
        log_warn "vnidsd.conf already exists, not overwriting"
        install -m 640 "deploy/common/vnidsd.conf.example" "$CONFIG_DIR/vnidsd.conf.new"
    fi

    # Suricata config
    if [ ! -f "$CONFIG_DIR/suricata/suricata.yaml" ]; then
        install -m 640 "suricata/config/suricata.yaml" "$CONFIG_DIR/suricata/"
    fi

    # Rules
    install -m 644 suricata/rules/*.rules "$CONFIG_DIR/rules/" 2>/dev/null || true
    install -m 644 suricata/rules/*.config "$CONFIG_DIR/rules/" 2>/dev/null || true
}

# Install systemd service
install_systemd() {
    log_info "Installing systemd service..."

    if [ -d "/etc/systemd/system" ]; then
        install -m 644 "deploy/common/vnidsd.service" "/etc/systemd/system/"
        systemctl daemon-reload
        log_info "Systemd service installed. Enable with: systemctl enable vnidsd"
    else
        log_warn "Systemd not found, skipping service installation"
    fi
}

# Create vnids user
create_user() {
    log_info "Creating vnids user..."

    if ! id -u vnids >/dev/null 2>&1; then
        useradd -r -s /sbin/nologin -d "$DATA_DIR" vnids
        log_info "Created user 'vnids'"
    else
        log_info "User 'vnids' already exists"
    fi

    # Set ownership
    chown -R vnids:vnids "$DATA_DIR"
    chown -R vnids:vnids "$LOG_DIR"
    chown -R root:vnids "$CONFIG_DIR"
}

# Main installation
main() {
    log_info "VNIDS Installation Script"
    log_info "========================="

    check_root
    create_directories
    install_binaries
    install_config
    create_user
    install_systemd

    log_info ""
    log_info "Installation complete!"
    log_info ""
    log_info "Next steps:"
    log_info "1. Edit configuration: $CONFIG_DIR/vnidsd.conf"
    log_info "2. Ensure Suricata is installed"
    log_info "3. Start the service: systemctl start vnidsd"
    log_info "4. Check status: vnids-cli status"
}

main "$@"
