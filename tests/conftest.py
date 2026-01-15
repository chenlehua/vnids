# VNIDS Unit Tests - Pytest Configuration
# Shared fixtures and configuration for all tests

import os
import sys
import json
import platform
import tempfile
import subprocess
from pathlib import Path
from typing import Generator, Dict, Any, Optional

import pytest
import yaml

# Add project root to path
PROJECT_ROOT = Path(__file__).parent.parent
sys.path.insert(0, str(PROJECT_ROOT))


# =============================================================================
# Architecture Detection Helpers
# =============================================================================

def get_host_arch() -> str:
    """Get the host machine architecture."""
    machine = platform.machine().lower()
    if machine in ('x86_64', 'amd64'):
        return 'x86_64'
    elif machine in ('aarch64', 'arm64'):
        return 'aarch64'
    elif machine.startswith('arm'):
        return 'arm'
    return machine


def get_binary_arch(binary_path: Path) -> Optional[str]:
    """Get the architecture of an ELF binary using file command."""
    if not binary_path.exists():
        return None

    try:
        result = subprocess.run(
            ['file', str(binary_path)],
            capture_output=True,
            text=True,
            timeout=5,
        )
        output = result.stdout.lower()

        if 'aarch64' in output or 'arm64' in output or 'arm aarch64' in output:
            return 'aarch64'
        elif 'x86-64' in output or 'x86_64' in output or 'amd64' in output:
            return 'x86_64'
        elif 'arm' in output:
            return 'arm'
        elif '386' in output or 'i386' in output or 'i686' in output:
            return 'x86'

        return None
    except (subprocess.TimeoutExpired, FileNotFoundError):
        return None


def is_android_binary(binary_path: Path) -> bool:
    """Check if an ELF binary is built for Android (uses Bionic linker)."""
    if not binary_path.exists():
        return False

    try:
        # Use readelf to check the interpreter (dynamic linker)
        result = subprocess.run(
            ['readelf', '-l', str(binary_path)],
            capture_output=True,
            text=True,
            timeout=5,
        )
        output = result.stdout

        # Android binaries use /system/bin/linker64 or /system/bin/linker
        if '/system/bin/linker' in output:
            return True

        # Also check file output for Android indicators
        result = subprocess.run(
            ['file', str(binary_path)],
            capture_output=True,
            text=True,
            timeout=5,
        )
        # Android binaries often have "Android" in file output
        if 'android' in result.stdout.lower():
            return True

        return False
    except (subprocess.TimeoutExpired, FileNotFoundError):
        return False


def can_run_binary(binary_path: Path) -> bool:
    """Check if a binary can run on the current host."""
    if not binary_path.exists():
        return False

    # Android binaries require /system/bin/linker64 which only exists on Android
    if is_android_binary(binary_path):
        # Check if we're on actual Android (has the linker)
        if Path('/system/bin/linker64').exists() or Path('/system/bin/linker').exists():
            return True
        return False

    host_arch = get_host_arch()
    binary_arch = get_binary_arch(binary_path)

    if binary_arch is None:
        return False

    # Same architecture can run
    if host_arch == binary_arch:
        return True

    # x86_64 can run x86 binaries (32-bit compatibility)
    if host_arch == 'x86_64' and binary_arch == 'x86':
        return True

    # Check for qemu-user emulation (only for non-Android Linux binaries)
    if binary_arch == 'aarch64':
        qemu_path = Path('/usr/bin/qemu-aarch64-static')
        if qemu_path.exists():
            return True

    return False


# =============================================================================
# Configuration Fixtures
# =============================================================================

@pytest.fixture(scope="session")
def project_root() -> Path:
    """Return the project root directory."""
    return PROJECT_ROOT


@pytest.fixture(scope="session")
def test_data_dir() -> Path:
    """Return the test data directory."""
    return PROJECT_ROOT / "tests" / "data"


@pytest.fixture(scope="session")
def host_arch() -> str:
    """Return the host architecture."""
    return get_host_arch()


@pytest.fixture(scope="session")
def sample_config(test_data_dir: Path) -> Dict[str, Any]:
    """Load sample VNIDS configuration."""
    config_path = test_data_dir / "config" / "vnids-test.yaml"
    if config_path.exists():
        with open(config_path) as f:
            return yaml.safe_load(f)
    # Return default test config
    return {
        "daemon": {
            "pid_file": "/tmp/vnids-test.pid",
            "log_level": "debug",
        },
        "suricata": {
            "binary": "/data/vnids/bin/suricata",
            "config": "/data/vnids/etc/suricata.yaml",
            "rules_dir": "/data/vnids/rules",
        },
        "ipc": {
            "socket_path": "/tmp/vnids-test.sock",
            "timeout_ms": 5000,
        },
        "events": {
            "queue_size": 1024,
            "storage_path": "/tmp/vnids-events.db",
        },
    }


@pytest.fixture
def temp_dir() -> Generator[Path, None, None]:
    """Create a temporary directory for tests."""
    with tempfile.TemporaryDirectory(prefix="vnids-test-") as tmpdir:
        yield Path(tmpdir)


@pytest.fixture
def temp_config(temp_dir: Path, sample_config: Dict[str, Any]) -> Path:
    """Create a temporary config file."""
    config_path = temp_dir / "vnids.yaml"
    with open(config_path, "w") as f:
        yaml.dump(sample_config, f)
    return config_path


# =============================================================================
# Binary Fixtures
# =============================================================================

@pytest.fixture(scope="session")
def vnidsd_binary(project_root: Path) -> Path:
    """Return path to vnidsd binary if it exists and can run on host."""
    candidates = [
        # Host-native builds first
        project_root / "build" / "vnidsd" / "vnidsd",
        Path("/usr/local/bin/vnidsd"),
        Path("/usr/bin/vnidsd"),
        # Android builds (may not run on host)
        project_root / "out" / "android-arm64" / "bin" / "vnidsd",
        project_root / "build-android" / "vnidsd" / "vnidsd",
    ]

    for path in candidates:
        if path.exists():
            if can_run_binary(path):
                return path
            else:
                # Found but can't run on this host
                if is_android_binary(path):
                    pytest.skip(
                        f"vnidsd binary found ({path}) is Android binary - "
                        f"requires /system/bin/linker64 (not available in Linux container)"
                    )
                else:
                    binary_arch = get_binary_arch(path)
                    host_arch = get_host_arch()
                    pytest.skip(
                        f"vnidsd binary found ({path}) but architecture mismatch: "
                        f"binary={binary_arch}, host={host_arch}"
                    )

    pytest.skip("vnidsd binary not found")


@pytest.fixture(scope="session")
def suricata_binary(project_root: Path) -> Path:
    """Return path to suricata binary if it exists and can run on host."""
    candidates = [
        # System-installed suricata first (most likely to run)
        Path("/usr/bin/suricata"),
        Path("/usr/local/bin/suricata"),
        # Project builds (may not run on host)
        project_root / "suricata" / "out" / "android-arm64" / "bin" / "suricata",
    ]

    for path in candidates:
        if path.exists():
            if can_run_binary(path):
                return path
            else:
                # Found but can't run on this host
                binary_arch = get_binary_arch(path)
                host_arch = get_host_arch()
                pytest.skip(
                    f"suricata binary found ({path}) but architecture mismatch: "
                    f"binary={binary_arch}, host={host_arch}"
                )

    pytest.skip("suricata binary not found (install with: apt install suricata)")


# =============================================================================
# Network Fixtures
# =============================================================================

@pytest.fixture(scope="session")
def pcap_dir(test_data_dir: Path) -> Path:
    """Return the PCAP test data directory."""
    return test_data_dir / "pcaps"


@pytest.fixture(scope="session")
def sample_pcaps(pcap_dir: Path) -> Dict[str, Path]:
    """Return dictionary of available sample PCAP files."""
    pcaps = {}
    if pcap_dir.exists():
        for pcap_file in pcap_dir.glob("*.pcap"):
            pcaps[pcap_file.stem] = pcap_file
        for pcap_file in pcap_dir.glob("*.pcapng"):
            pcaps[pcap_file.stem] = pcap_file
    return pcaps


# =============================================================================
# Rules Fixtures
# =============================================================================

@pytest.fixture(scope="session")
def rules_dir(test_data_dir: Path) -> Path:
    """Return the rules test data directory."""
    return test_data_dir / "rules"


@pytest.fixture(scope="session")
def sample_rules(rules_dir: Path) -> Dict[str, Path]:
    """Return dictionary of available sample rule files."""
    rules = {}
    if rules_dir.exists():
        for rule_file in rules_dir.glob("*.rules"):
            rules[rule_file.stem] = rule_file
    return rules


# =============================================================================
# Event Fixtures
# =============================================================================

@pytest.fixture
def sample_eve_event() -> Dict[str, Any]:
    """Return a sample Suricata EVE JSON event."""
    return {
        "timestamp": "2024-01-15T10:30:00.123456+0000",
        "flow_id": 1234567890123456,
        "in_iface": "eth0",
        "event_type": "alert",
        "src_ip": "192.168.1.100",
        "src_port": 54321,
        "dest_ip": "10.0.0.1",
        "dest_port": 80,
        "proto": "TCP",
        "alert": {
            "action": "allowed",
            "gid": 1,
            "signature_id": 2100498,
            "rev": 7,
            "signature": "GPL ATTACK_RESPONSE id check returned root",
            "category": "Potentially Bad Traffic",
            "severity": 2,
        },
        "flow": {
            "pkts_toserver": 10,
            "pkts_toclient": 8,
            "bytes_toserver": 1500,
            "bytes_toclient": 12000,
            "start": "2024-01-15T10:29:55.000000+0000",
        },
    }


@pytest.fixture
def sample_eve_flow() -> Dict[str, Any]:
    """Return a sample Suricata EVE flow event."""
    return {
        "timestamp": "2024-01-15T10:30:05.654321+0000",
        "flow_id": 1234567890123456,
        "in_iface": "eth0",
        "event_type": "flow",
        "src_ip": "192.168.1.100",
        "src_port": 54321,
        "dest_ip": "10.0.0.1",
        "dest_port": 80,
        "proto": "TCP",
        "flow": {
            "pkts_toserver": 15,
            "pkts_toclient": 12,
            "bytes_toserver": 2500,
            "bytes_toclient": 18000,
            "start": "2024-01-15T10:29:55.000000+0000",
            "end": "2024-01-15T10:30:05.000000+0000",
            "age": 10,
            "state": "closed",
            "reason": "timeout",
        },
        "tcp": {
            "tcp_flags": "1f",
            "tcp_flags_ts": "1e",
            "tcp_flags_tc": "1b",
            "syn": True,
            "fin": True,
            "rst": False,
            "psh": True,
            "ack": True,
        },
    }


# =============================================================================
# ADB Fixtures (for integration tests)
# =============================================================================

@pytest.fixture(scope="session")
def adb_available() -> bool:
    """Check if adb is available and a device is connected."""
    try:
        result = subprocess.run(
            ["adb", "devices"],
            capture_output=True,
            text=True,
            timeout=5,
        )
        lines = result.stdout.strip().split("\n")
        # Check if any device is connected (more than just header line)
        return len(lines) > 1 and any("device" in line for line in lines[1:])
    except (subprocess.TimeoutExpired, FileNotFoundError):
        return False


@pytest.fixture
def skip_without_device(adb_available: bool):
    """Skip test if no Android device/emulator is available."""
    if not adb_available:
        pytest.skip("No Android device/emulator available")


# =============================================================================
# Markers
# =============================================================================

def pytest_configure(config):
    """Register custom markers."""
    config.addinivalue_line(
        "markers", "slow: marks tests as slow (deselect with '-m \"not slow\"')"
    )
    config.addinivalue_line(
        "markers", "integration: marks tests as integration tests"
    )
    config.addinivalue_line(
        "markers", "requires_device: marks tests that require an Android device"
    )
    config.addinivalue_line(
        "markers", "requires_root: marks tests that require root access"
    )
    config.addinivalue_line(
        "markers", "requires_native_binary: marks tests that require native binaries"
    )
