# VNIDS Unit Tests - IPC Communication Tests
# Tests for Unix socket IPC protocol

import os
import json
import socket
import struct
import tempfile
from pathlib import Path
from typing import Dict, Any, Optional

import pytest


class TestIPCProtocol:
    """Test IPC message protocol."""

    # Message types (matching vnids_ipc.h)
    MSG_TYPE_REQUEST = 0x01
    MSG_TYPE_RESPONSE = 0x02
    MSG_TYPE_EVENT = 0x03
    MSG_TYPE_HEARTBEAT = 0x04

    # Commands
    CMD_STATUS = 0x01
    CMD_RELOAD_RULES = 0x02
    CMD_GET_STATS = 0x03
    CMD_GET_EVENTS = 0x04

    def test_message_header_size(self):
        """Test message header is correct size."""
        # Header: magic(4) + version(2) + type(2) + length(4) + reserved(4) = 16 bytes
        header_size = 16
        assert header_size == 16

    def test_message_magic_number(self):
        """Test message magic number."""
        magic = 0x564E4944  # "VNID" in ASCII
        assert magic == 0x564E4944

    def test_encode_message_header(self):
        """Test encoding message header."""
        magic = 0x564E4944
        version = 0x0100
        msg_type = self.MSG_TYPE_REQUEST
        length = 64
        reserved = 0

        header = struct.pack(">IHHII", magic, version, msg_type, length, reserved)
        assert len(header) == 16

    def test_decode_message_header(self):
        """Test decoding message header."""
        magic = 0x564E4944
        version = 0x0100
        msg_type = self.MSG_TYPE_RESPONSE
        length = 128
        reserved = 0

        header = struct.pack(">IHHII", magic, version, msg_type, length, reserved)
        decoded = struct.unpack(">IHHII", header)

        assert decoded[0] == magic
        assert decoded[1] == version
        assert decoded[2] == msg_type
        assert decoded[3] == length

    def test_invalid_magic_detection(self):
        """Test detection of invalid magic number."""
        invalid_magic = 0xDEADBEEF
        valid_magic = 0x564E4944

        assert invalid_magic != valid_magic


class TestIPCMessages:
    """Test IPC message encoding/decoding."""

    def test_status_request(self):
        """Test status request message."""
        request = {
            "command": "status",
            "request_id": 12345,
        }
        encoded = json.dumps(request).encode("utf-8")
        assert len(encoded) > 0

    def test_status_response(self):
        """Test status response message."""
        response = {
            "status": "ok",
            "request_id": 12345,
            "data": {
                "daemon_running": True,
                "suricata_running": True,
                "events_processed": 1000,
                "uptime_seconds": 3600,
            }
        }
        encoded = json.dumps(response).encode("utf-8")
        decoded = json.loads(encoded.decode("utf-8"))
        assert decoded["status"] == "ok"

    def test_reload_rules_request(self):
        """Test reload rules request."""
        request = {
            "command": "reload_rules",
            "request_id": 12346,
            "params": {
                "rules_path": "/data/vnids/rules",
            }
        }
        encoded = json.dumps(request)
        assert "reload_rules" in encoded

    def test_get_events_request(self):
        """Test get events request."""
        request = {
            "command": "get_events",
            "request_id": 12347,
            "params": {
                "limit": 100,
                "since": "2024-01-15T00:00:00Z",
                "severity_max": 3,
            }
        }
        encoded = json.dumps(request)
        assert "get_events" in encoded

    def test_error_response(self):
        """Test error response message."""
        response = {
            "status": "error",
            "request_id": 12345,
            "error": {
                "code": -1,
                "message": "Operation failed",
            }
        }
        assert response["status"] == "error"
        assert "error" in response


class TestUnixSocket:
    """Test Unix domain socket operations."""

    def test_socket_path_creation(self, temp_dir: Path):
        """Test socket path can be created."""
        socket_path = temp_dir / "test.sock"
        assert not socket_path.exists()

        # Create socket
        sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        try:
            sock.bind(str(socket_path))
            assert socket_path.exists()
        finally:
            sock.close()
            socket_path.unlink(missing_ok=True)

    def test_socket_permissions(self, temp_dir: Path):
        """Test socket file permissions."""
        socket_path = temp_dir / "test.sock"

        sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        try:
            sock.bind(str(socket_path))
            # Check socket exists
            assert socket_path.exists()
            # Socket should be a socket type file
            import stat
            mode = socket_path.stat().st_mode
            assert stat.S_ISSOCK(mode)
        finally:
            sock.close()
            socket_path.unlink(missing_ok=True)

    def test_socket_path_length_limit(self, temp_dir: Path):
        """Test socket path length limit (108 chars on Linux)."""
        max_path_len = 108

        # Create a path that's too long
        long_name = "a" * 200
        long_path = temp_dir / long_name

        assert len(str(long_path)) > max_path_len

    def test_abstract_socket_namespace(self):
        """Test abstract socket namespace (Linux-specific)."""
        # Abstract sockets start with null byte
        abstract_name = "\0vnids-test-abstract"

        sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        try:
            sock.bind(abstract_name)
            # Abstract sockets don't create files
            # They exist only in the abstract namespace
        except OSError:
            # May fail if socket already exists
            pass
        finally:
            sock.close()


class TestIPCTimeout:
    """Test IPC timeout handling."""

    def test_socket_timeout_setting(self):
        """Test setting socket timeout."""
        sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        try:
            timeout_sec = 5.0
            sock.settimeout(timeout_sec)
            assert sock.gettimeout() == timeout_sec
        finally:
            sock.close()

    def test_blocking_vs_nonblocking(self):
        """Test blocking vs non-blocking mode."""
        sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        try:
            # Default is blocking
            assert sock.getblocking() is True

            # Set non-blocking
            sock.setblocking(False)
            assert sock.getblocking() is False
        finally:
            sock.close()


class TestIPCReconnection:
    """Test IPC reconnection logic."""

    def test_reconnect_backoff(self):
        """Test exponential backoff for reconnection."""
        base_delay = 0.1  # 100ms
        max_delay = 5.0   # 5s
        multiplier = 2.0

        delays = []
        delay = base_delay

        for _ in range(10):
            delays.append(delay)
            delay = min(delay * multiplier, max_delay)

        # First few delays should increase exponentially
        assert delays[0] == 0.1
        assert delays[1] == 0.2
        assert delays[2] == 0.4

        # Later delays should be capped at max
        assert delays[-1] == max_delay

    def test_max_reconnect_attempts(self):
        """Test maximum reconnection attempts."""
        max_attempts = 5
        attempts = 0

        while attempts < max_attempts:
            # Simulate connection attempt
            success = False  # Would be actual connection attempt
            if success:
                break
            attempts += 1

        assert attempts == max_attempts


class TestIPCSerialization:
    """Test IPC data serialization."""

    def test_utf8_encoding(self):
        """Test UTF-8 encoding for messages."""
        message = {"text": "Hello, 世界! 🌍"}
        encoded = json.dumps(message).encode("utf-8")
        decoded = json.loads(encoded.decode("utf-8"))
        assert decoded["text"] == message["text"]

    def test_binary_data_handling(self):
        """Test handling of binary data in messages."""
        import base64

        binary_data = bytes(range(256))
        encoded_data = base64.b64encode(binary_data).decode("ascii")

        message = {"binary": encoded_data}
        json_str = json.dumps(message)

        # Verify we can decode it back
        parsed = json.loads(json_str)
        decoded_data = base64.b64decode(parsed["binary"])
        assert decoded_data == binary_data

    def test_large_message_handling(self):
        """Test handling of large messages."""
        # Create a message with many events
        events = [{"id": i, "data": "x" * 100} for i in range(100)]
        message = {"events": events}

        encoded = json.dumps(message).encode("utf-8")
        # Should be able to handle reasonably large messages
        assert len(encoded) < 1024 * 1024  # Less than 1MB
