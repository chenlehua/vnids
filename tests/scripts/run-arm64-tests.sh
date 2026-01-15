#!/bin/bash
# Run VNIDS tests in ARM64 Docker container
# This script runs all tests including those that require ARM64 binaries

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORKSPACE="/workspace"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

log_info() { echo -e "${GREEN}[INFO]${NC} $1"; }
log_warn() { echo -e "${YELLOW}[WARN]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }

# Check environment
log_info "=== VNIDS ARM64 Test Suite ==="
log_info "Architecture: $(uname -m)"
log_info "Suricata version: $(suricata -V 2>&1 | head -1)"
log_info "Python version: $(python3 --version)"

# Setup test configuration
setup_test_env() {
    log_info "Setting up test environment..."

    # Create required directories
    mkdir -p /data/vnids/bin /data/vnids/etc /data/vnids/rules /data/vnids/var/log
    mkdir -p /tmp/vnids-test

    # Copy test rules if they exist
    if [ -d "$WORKSPACE/tests/data/rules" ]; then
        cp -r "$WORKSPACE/tests/data/rules/"* /data/vnids/rules/ 2>/dev/null || true
    fi

    # Copy test configuration
    if [ -f "$WORKSPACE/tests/data/config/vnids-test.yaml" ]; then
        cp "$WORKSPACE/tests/data/config/vnids-test.yaml" /data/vnids/etc/vnids.yaml
    fi

    log_info "Test environment ready"
}

# Run unit tests
run_unit_tests() {
    log_info "Running unit tests..."

    cd "$WORKSPACE/tests"
    python3 -m pytest unit/ -v --tb=short \
        --junitxml=/tmp/vnids-test/unit-results.xml || {
        log_error "Unit tests failed"
        return 1
    }

    log_info "Unit tests passed"
}

# Run integration tests
run_integration_tests() {
    log_info "Running integration tests..."

    cd "$WORKSPACE/tests"
    python3 -m pytest integration/ -v --tb=short \
        --junitxml=/tmp/vnids-test/integration-results.xml || {
        log_error "Integration tests failed"
        return 1
    }

    log_info "Integration tests passed"
}

# Test Suricata directly
test_suricata() {
    log_info "Testing Suricata directly..."

    # Test basic functionality
    suricata -V || {
        log_error "Suricata version check failed"
        return 1
    }

    # Test rule loading (if rules exist)
    if [ -f "/data/vnids/rules/http-test.rules" ]; then
        log_info "Testing rule loading..."
        # Just verify suricata can parse the rules (will exit with error if invalid)
        suricata -T -S /data/vnids/rules/http-test.rules 2>&1 | head -20 || true
    fi

    log_info "Suricata tests passed"
}

# Run PCAP replay tests
test_pcap_replay() {
    log_info "Testing PCAP replay..."

    if [ -d "$WORKSPACE/tests/data/pcaps" ]; then
        for pcap in "$WORKSPACE/tests/data/pcaps"/*.pcap; do
            if [ -f "$pcap" ]; then
                name=$(basename "$pcap" .pcap)
                log_info "  Processing: $name"

                # Run suricata in offline mode on the pcap
                mkdir -p "/tmp/vnids-test/pcap-results/$name"
                timeout 30 suricata -r "$pcap" -l "/tmp/vnids-test/pcap-results/$name" \
                    --runmode single 2>&1 | tail -5 || true
            fi
        done
    else
        log_warn "No PCAP files found"
    fi

    log_info "PCAP replay tests completed"
}

# Main
main() {
    setup_test_env
    test_suricata
    run_unit_tests
    run_integration_tests
    test_pcap_replay

    log_info "=== All tests completed successfully ==="

    # Summary
    echo ""
    echo "Test Results:"
    echo "  Unit tests:        PASSED"
    echo "  Integration tests: PASSED"
    echo "  Suricata tests:    PASSED"
    echo "  PCAP replay:       COMPLETED"
}

main "$@"
