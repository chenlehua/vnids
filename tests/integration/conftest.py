# VNIDS Integration Tests - Pytest Configuration
# Shared fixtures for integration testing

import os
import sys
import time
import json
import socket
import subprocess
import tempfile
from pathlib import Path
from typing import Generator, Dict, Any, Optional

import pytest

# Import shared fixtures
sys.path.insert(0, str(Path(__file__).parent.parent))
from conftest import *


# =============================================================================
# Integration Test Fixtures
# =============================================================================

@pytest.fixture(scope="module")
def integration_temp_dir() -> Generator[Path, None, None]:
    """Create a temporary directory for integration tests."""
    with tempfile.TemporaryDirectory(prefix="vnids-integration-") as tmpdir:
        yield Path(tmpdir)


@pytest.fixture(scope="module")
def vnids_socket_path(integration_temp_dir: Path) -> Path:
    """Return path for VNIDS IPC socket."""
    return integration_temp_dir / "vnids.sock"


@pytest.fixture(scope="module")
def vnids_config_path(integration_temp_dir: Path) -> Path:
    """Create VNIDS configuration for integration tests."""
    config = {
        "daemon": {
            "pid_file": str(integration_temp_dir / "vnids.pid"),
            "log_level": "debug",
            "log_file": str(integration_temp_dir / "vnids.log"),
        },
        "suricata": {
            "binary": "/data/vnids/bin/suricata",
            "config": str(integration_temp_dir / "suricata.yaml"),
            "rules_dir": str(integration_temp_dir / "rules"),
            "log_dir": str(integration_temp_dir / "suricata-logs"),
        },
        "ipc": {
            "socket_path": str(integration_temp_dir / "vnids.sock"),
            "timeout_ms": 10000,
        },
        "events": {
            "queue_size": 4096,
            "storage_path": str(integration_temp_dir / "events.db"),
            "max_events": 100000,
        },
    }

    import yaml
    config_path = integration_temp_dir / "vnids.yaml"
    with open(config_path, "w") as f:
        yaml.dump(config, f)

    # Create required directories
    (integration_temp_dir / "rules").mkdir(exist_ok=True)
    (integration_temp_dir / "suricata-logs").mkdir(exist_ok=True)

    return config_path


# =============================================================================
# ADB Fixtures
# =============================================================================

class ADBHelper:
    """Helper class for ADB operations."""

    def __init__(self):
        self._available = None

    @property
    def available(self) -> bool:
        """Check if ADB is available."""
        if self._available is None:
            self._available = self._check_adb()
        return self._available

    def _check_adb(self) -> bool:
        """Check ADB availability."""
        try:
            result = subprocess.run(
                ["adb", "devices"],
                capture_output=True,
                text=True,
                timeout=5,
            )
            lines = result.stdout.strip().split("\n")
            return len(lines) > 1 and any("device" in l for l in lines[1:])
        except (subprocess.TimeoutExpired, FileNotFoundError):
            return False

    def shell(self, cmd: str, timeout: int = 30) -> subprocess.CompletedProcess:
        """Run shell command via ADB."""
        return subprocess.run(
            ["adb", "shell", cmd],
            capture_output=True,
            text=True,
            timeout=timeout,
        )

    def push(self, local: str, remote: str) -> subprocess.CompletedProcess:
        """Push file to device."""
        return subprocess.run(
            ["adb", "push", local, remote],
            capture_output=True,
            text=True,
            timeout=60,
        )

    def pull(self, remote: str, local: str) -> subprocess.CompletedProcess:
        """Pull file from device."""
        return subprocess.run(
            ["adb", "pull", remote, local],
            capture_output=True,
            text=True,
            timeout=60,
        )

    def forward(self, local: str, remote: str) -> subprocess.CompletedProcess:
        """Forward port/socket."""
        return subprocess.run(
            ["adb", "forward", local, remote],
            capture_output=True,
            text=True,
            timeout=10,
        )


@pytest.fixture(scope="session")
def adb() -> ADBHelper:
    """Return ADB helper instance."""
    return ADBHelper()


@pytest.fixture
def require_device(adb: ADBHelper):
    """Skip test if no device available."""
    if not adb.available:
        pytest.skip("No Android device/emulator connected")


# =============================================================================
# Process Management Fixtures
# =============================================================================

class ProcessManager:
    """Manage test processes."""

    def __init__(self):
        self.processes: Dict[str, subprocess.Popen] = {}

    def start(self, name: str, cmd: list, **kwargs) -> subprocess.Popen:
        """Start a process."""
        proc = subprocess.Popen(cmd, **kwargs)
        self.processes[name] = proc
        return proc

    def stop(self, name: str, timeout: int = 5) -> Optional[int]:
        """Stop a process."""
        if name not in self.processes:
            return None

        proc = self.processes[name]
        proc.terminate()

        try:
            return proc.wait(timeout=timeout)
        except subprocess.TimeoutExpired:
            proc.kill()
            return proc.wait()

    def stop_all(self, timeout: int = 5):
        """Stop all processes."""
        for name in list(self.processes.keys()):
            self.stop(name, timeout)

    def is_running(self, name: str) -> bool:
        """Check if process is running."""
        if name not in self.processes:
            return False
        return self.processes[name].poll() is None


@pytest.fixture
def process_manager() -> Generator[ProcessManager, None, None]:
    """Return process manager and cleanup after test."""
    manager = ProcessManager()
    yield manager
    manager.stop_all()


# =============================================================================
# Network Fixtures
# =============================================================================

@pytest.fixture
def local_socket_pair() -> Generator[tuple, None, None]:
    """Create a pair of connected Unix domain sockets for testing."""
    server_sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    client_sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)

    with tempfile.NamedTemporaryFile(delete=False, suffix=".sock") as f:
        socket_path = f.name

    os.unlink(socket_path)

    try:
        server_sock.bind(socket_path)
        server_sock.listen(1)
        server_sock.setblocking(False)

        client_sock.connect(socket_path)
        conn, _ = server_sock.accept()

        yield (conn, client_sock)

    finally:
        server_sock.close()
        client_sock.close()
        if os.path.exists(socket_path):
            os.unlink(socket_path)


# =============================================================================
# Markers
# =============================================================================

def pytest_configure(config):
    """Register custom markers."""
    config.addinivalue_line(
        "markers", "device: marks tests that require an Android device"
    )
    config.addinivalue_line(
        "markers", "slow: marks tests as slow running"
    )
    config.addinivalue_line(
        "markers", "suricata: marks tests that require Suricata"
    )
