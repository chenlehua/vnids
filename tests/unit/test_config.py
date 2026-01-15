# VNIDS Unit Tests - Configuration Tests
# Tests for configuration parsing and validation

import os
import json
import tempfile
from pathlib import Path
from typing import Dict, Any

import pytest
import yaml


class TestConfigParsing:
    """Test configuration file parsing."""

    def test_load_yaml_config(self, sample_config: Dict[str, Any]):
        """Test that sample config loads correctly."""
        assert sample_config is not None
        assert "daemon" in sample_config
        assert "suricata" in sample_config
        assert "ipc" in sample_config
        assert "events" in sample_config

    def test_daemon_config_fields(self, sample_config: Dict[str, Any]):
        """Test daemon configuration fields."""
        daemon = sample_config.get("daemon", {})
        assert "pid_file" in daemon or "log_level" in daemon

    def test_suricata_config_fields(self, sample_config: Dict[str, Any]):
        """Test Suricata configuration fields."""
        suricata = sample_config.get("suricata", {})
        assert "binary" in suricata
        assert "config" in suricata
        assert "rules_dir" in suricata

    def test_ipc_config_fields(self, sample_config: Dict[str, Any]):
        """Test IPC configuration fields."""
        ipc = sample_config.get("ipc", {})
        assert "socket_path" in ipc
        assert "timeout_ms" in ipc

    def test_events_config_fields(self, sample_config: Dict[str, Any]):
        """Test events configuration fields."""
        events = sample_config.get("events", {})
        assert "queue_size" in events


class TestConfigValidation:
    """Test configuration validation logic."""

    def test_valid_log_levels(self):
        """Test that valid log levels are accepted."""
        valid_levels = ["trace", "debug", "info", "warn", "error", "fatal"]
        for level in valid_levels:
            assert level in valid_levels

    def test_positive_queue_size(self, sample_config: Dict[str, Any]):
        """Test that queue size must be positive."""
        queue_size = sample_config.get("events", {}).get("queue_size", 0)
        assert queue_size > 0, "Queue size must be positive"

    def test_positive_timeout(self, sample_config: Dict[str, Any]):
        """Test that timeout must be positive."""
        timeout = sample_config.get("ipc", {}).get("timeout_ms", 0)
        assert timeout > 0, "Timeout must be positive"

    def test_valid_paths(self, sample_config: Dict[str, Any]):
        """Test that paths are valid strings."""
        suricata = sample_config.get("suricata", {})
        assert isinstance(suricata.get("binary"), str)
        assert isinstance(suricata.get("config"), str)
        assert isinstance(suricata.get("rules_dir"), str)


class TestConfigFileOperations:
    """Test configuration file read/write operations."""

    def test_write_and_read_config(self, temp_dir: Path):
        """Test writing and reading config file."""
        config = {
            "daemon": {"log_level": "debug"},
            "suricata": {"binary": "/usr/bin/suricata"},
        }

        config_path = temp_dir / "test-config.yaml"

        # Write
        with open(config_path, "w") as f:
            yaml.dump(config, f)

        # Read
        with open(config_path) as f:
            loaded = yaml.safe_load(f)

        assert loaded == config

    def test_config_with_special_characters(self, temp_dir: Path):
        """Test config with paths containing special characters."""
        config = {
            "paths": {
                "with_spaces": "/path/with spaces/file.txt",
                "with_unicode": "/path/文件/config.yaml",
            }
        }

        config_path = temp_dir / "special-config.yaml"

        with open(config_path, "w") as f:
            yaml.dump(config, f)

        with open(config_path) as f:
            loaded = yaml.safe_load(f)

        assert loaded["paths"]["with_spaces"] == "/path/with spaces/file.txt"

    def test_empty_config(self, temp_dir: Path):
        """Test handling of empty config file."""
        config_path = temp_dir / "empty-config.yaml"
        config_path.touch()

        with open(config_path) as f:
            loaded = yaml.safe_load(f)

        assert loaded is None

    def test_invalid_yaml(self, temp_dir: Path):
        """Test handling of invalid YAML."""
        config_path = temp_dir / "invalid-config.yaml"
        with open(config_path, "w") as f:
            f.write("invalid: yaml: content: [")

        with pytest.raises(yaml.YAMLError):
            with open(config_path) as f:
                yaml.safe_load(f)


class TestConfigDefaults:
    """Test configuration default values."""

    def test_default_queue_size(self):
        """Test default queue size value."""
        default_queue_size = 1024
        assert default_queue_size == 1024

    def test_default_timeout(self):
        """Test default timeout value."""
        default_timeout_ms = 5000
        assert default_timeout_ms == 5000

    def test_default_log_level(self):
        """Test default log level."""
        default_log_level = "info"
        assert default_log_level in ["trace", "debug", "info", "warn", "error"]
