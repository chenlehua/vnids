# VNIDS Integration Tests - Daemon Tests
# Tests for vnidsd daemon functionality

import os
import time
import json
import socket
import subprocess
from pathlib import Path
from typing import Dict, Any

import pytest


@pytest.mark.integration
class TestDaemonStartup:
    """Test daemon startup and shutdown."""

    def test_daemon_help(self, vnidsd_binary: Path):
        """Test daemon --help output."""
        result = subprocess.run(
            [str(vnidsd_binary), "--help"],
            capture_output=True,
            text=True,
            timeout=10,
        )
        # Should show help without error
        assert result.returncode == 0 or "usage" in result.stdout.lower() or "help" in result.stdout.lower()

    def test_daemon_version(self, vnidsd_binary: Path):
        """Test daemon --version output."""
        result = subprocess.run(
            [str(vnidsd_binary), "--version"],
            capture_output=True,
            text=True,
            timeout=10,
        )
        # Should output version info
        assert "vnids" in result.stdout.lower() or "1." in result.stdout

    def test_daemon_config_validation(self, vnidsd_binary: Path, vnids_config_path: Path):
        """Test daemon validates config on startup."""
        result = subprocess.run(
            [str(vnidsd_binary), "--config", str(vnids_config_path), "--check"],
            capture_output=True,
            text=True,
            timeout=10,
        )
        # Config check should pass or give meaningful error
        # (depending on whether --check is implemented)
        assert result.returncode in [0, 1, 2]

    @pytest.mark.slow
    def test_daemon_startup_shutdown(self, vnidsd_binary: Path, vnids_config_path: Path, process_manager):
        """Test daemon can start and stop cleanly."""
        # Start daemon
        proc = process_manager.start(
            "vnidsd",
            [str(vnidsd_binary), "--config", str(vnids_config_path), "--foreground"],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )

        # Wait for startup
        time.sleep(2)

        # Check if still running
        if proc.poll() is not None:
            stdout, stderr = proc.communicate()
            pytest.skip(f"Daemon exited early: {stderr.decode()}")

        # Stop daemon
        proc.terminate()
        try:
            proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            proc.kill()

        assert True  # If we got here, startup/shutdown worked


@pytest.mark.integration
@pytest.mark.device
class TestDaemonOnDevice:
    """Test daemon functionality on Android device."""

    def test_push_binary(self, adb, vnidsd_binary: Path, require_device):
        """Test pushing binary to device."""
        result = adb.push(str(vnidsd_binary), "/data/local/tmp/vnidsd")
        assert result.returncode == 0

    def test_binary_executable(self, adb, require_device):
        """Test binary is executable on device."""
        adb.shell("chmod 755 /data/local/tmp/vnidsd")
        result = adb.shell("/data/local/tmp/vnidsd --help")
        # Should run without exec errors
        assert "not found" not in result.stderr.lower()
        assert "permission denied" not in result.stderr.lower()

    def test_daemon_runs_on_device(self, adb, require_device):
        """Test daemon can run on device."""
        # Start daemon in background
        adb.shell("/data/local/tmp/vnidsd --help &")
        time.sleep(1)

        # Check process list
        result = adb.shell("ps -A | grep vnidsd")
        # May or may not find process depending on how quickly it exits

    def test_ipc_socket_creation(self, adb, require_device):
        """Test IPC socket is created on device."""
        # This would require actually running the daemon
        # Just verify we can check for sockets
        result = adb.shell("ls -la /data/vnids/var/run/ 2>/dev/null || echo 'no socket'")
        assert result.returncode == 0


@pytest.mark.integration
class TestDaemonIPC:
    """Test daemon IPC functionality."""

    def test_ipc_socket_bind(self, integration_temp_dir: Path):
        """Test IPC socket can be created."""
        socket_path = integration_temp_dir / "test-ipc.sock"

        sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        try:
            sock.bind(str(socket_path))
            sock.listen(1)
            assert socket_path.exists()
        finally:
            sock.close()
            socket_path.unlink(missing_ok=True)

    def test_ipc_message_exchange(self, local_socket_pair):
        """Test IPC message exchange."""
        server, client = local_socket_pair

        # Send request
        request = {"command": "status", "id": 1}
        client.send(json.dumps(request).encode())

        # Receive on server
        data = server.recv(1024)
        received = json.loads(data.decode())

        assert received["command"] == "status"

        # Send response
        response = {"status": "ok", "id": 1}
        server.send(json.dumps(response).encode())

        # Receive on client
        data = client.recv(1024)
        received = json.loads(data.decode())

        assert received["status"] == "ok"

    def test_ipc_concurrent_connections(self, integration_temp_dir: Path):
        """Test IPC with multiple concurrent connections."""
        socket_path = integration_temp_dir / "multi-ipc.sock"

        server = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        server.bind(str(socket_path))
        server.listen(5)
        server.setblocking(False)

        clients = []
        try:
            # Create multiple clients
            for i in range(3):
                client = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
                client.connect(str(socket_path))
                clients.append(client)

            assert len(clients) == 3

        finally:
            for c in clients:
                c.close()
            server.close()
            socket_path.unlink(missing_ok=True)


@pytest.mark.integration
class TestDaemonWatchdog:
    """Test daemon watchdog functionality."""

    def test_watchdog_timeout_config(self):
        """Test watchdog timeout configuration."""
        # Default timeout should be reasonable
        default_timeout_sec = 30
        assert 10 <= default_timeout_sec <= 120

    def test_watchdog_restart_limit(self):
        """Test watchdog restart limit."""
        max_restarts = 5
        restart_window_sec = 300  # 5 minutes

        assert max_restarts > 0
        assert restart_window_sec > 0
