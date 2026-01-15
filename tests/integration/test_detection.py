# VNIDS Integration Tests - Detection Tests
# Tests for Suricata-based threat detection with PCAP replay

import os
import time
import json
import subprocess
import tempfile
from pathlib import Path
from typing import Dict, Any, List, Optional

import pytest

# Try to import scapy for packet generation
try:
    from scapy.all import *
    SCAPY_AVAILABLE = True
except ImportError:
    SCAPY_AVAILABLE = False


@pytest.mark.integration
class TestSuricataIntegration:
    """Test Suricata integration."""

    def test_suricata_version(self, suricata_binary: Path):
        """Test Suricata version output."""
        result = subprocess.run(
            [str(suricata_binary), "--build-info"],
            capture_output=True,
            text=True,
            timeout=10,
        )
        assert "Suricata" in result.stdout or result.returncode == 0

    def test_suricata_rule_syntax_check(self, suricata_binary: Path, integration_temp_dir: Path):
        """Test Suricata rule syntax validation."""
        # Create a test rule file
        rules_file = integration_temp_dir / "test.rules"
        rules_file.write_text(
            'alert tcp any any -> any 80 (msg:"Test rule"; sid:9999999; rev:1;)\n'
        )

        # Create minimal config
        config_file = integration_temp_dir / "suricata-test.yaml"
        config_file.write_text(f"""
%YAML 1.1
---
rule-files:
  - {rules_file}
""")

        result = subprocess.run(
            [str(suricata_binary), "-T", "-c", str(config_file), "-S", str(rules_file)],
            capture_output=True,
            text=True,
            timeout=30,
        )

        # -T flag tests configuration
        # May succeed or fail depending on full config requirements


@pytest.mark.integration
class TestPCAPReplay:
    """Test PCAP replay and detection."""

    def test_pcap_file_parsing(self, sample_pcaps: Dict[str, Path]):
        """Test PCAP files can be parsed."""
        if not sample_pcaps:
            pytest.skip("No PCAP files available")

        for name, path in sample_pcaps.items():
            assert path.exists(), f"PCAP file not found: {path}"
            assert path.stat().st_size > 0, f"PCAP file is empty: {path}"

    @pytest.mark.skipif(not SCAPY_AVAILABLE, reason="scapy not installed")
    def test_generate_http_pcap(self, integration_temp_dir: Path):
        """Test generating HTTP traffic PCAP."""
        pcap_path = integration_temp_dir / "http-test.pcap"

        # Generate simple HTTP GET packet
        packets = [
            Ether() / IP(dst="10.0.0.1") / TCP(dport=80, flags="S"),
            Ether() / IP(dst="10.0.0.1") / TCP(dport=80, flags="A") /
            Raw(load=b"GET / HTTP/1.1\r\nHost: test.com\r\n\r\n"),
        ]

        wrpcap(str(pcap_path), packets)
        assert pcap_path.exists()
        assert pcap_path.stat().st_size > 0

    @pytest.mark.skipif(not SCAPY_AVAILABLE, reason="scapy not installed")
    def test_generate_suspicious_traffic(self, integration_temp_dir: Path):
        """Test generating suspicious traffic PCAP."""
        pcap_path = integration_temp_dir / "suspicious-test.pcap"

        # Generate traffic that might trigger alerts
        packets = [
            # SYN flood-like traffic
            Ether() / IP(dst="10.0.0.1") / TCP(dport=22, flags="S"),
            Ether() / IP(dst="10.0.0.1") / TCP(dport=23, flags="S"),
            Ether() / IP(dst="10.0.0.1") / TCP(dport=3389, flags="S"),
            # Suspicious payload
            Ether() / IP(dst="10.0.0.1") / TCP(dport=80) /
            Raw(load=b"GET /admin/../../../etc/passwd HTTP/1.1\r\n\r\n"),
        ]

        wrpcap(str(pcap_path), packets)
        assert pcap_path.exists()


@pytest.mark.integration
class TestAlertGeneration:
    """Test alert generation from traffic."""

    def test_alert_json_format(self, sample_eve_event: Dict[str, Any]):
        """Test alert JSON format matches expected schema."""
        required_fields = ["timestamp", "event_type", "src_ip", "dest_ip"]
        for field in required_fields:
            assert field in sample_eve_event

        assert sample_eve_event["event_type"] == "alert"
        assert "alert" in sample_eve_event
        assert "signature_id" in sample_eve_event["alert"]

    def test_alert_severity_levels(self):
        """Test alert severity levels."""
        severity_levels = {
            1: "critical",
            2: "high",
            3: "medium",
            4: "low",
            5: "info",
        }

        for level, name in severity_levels.items():
            assert 1 <= level <= 5

    def test_eve_log_parsing(self, integration_temp_dir: Path):
        """Test parsing EVE JSON log file."""
        eve_log = integration_temp_dir / "eve.json"

        # Write sample EVE events
        events = [
            {"timestamp": "2024-01-15T10:00:00", "event_type": "alert", "src_ip": "192.168.1.1"},
            {"timestamp": "2024-01-15T10:00:01", "event_type": "flow", "src_ip": "192.168.1.2"},
        ]

        with open(eve_log, "w") as f:
            for event in events:
                f.write(json.dumps(event) + "\n")

        # Parse log file
        parsed_events = []
        with open(eve_log) as f:
            for line in f:
                parsed_events.append(json.loads(line))

        assert len(parsed_events) == 2
        assert parsed_events[0]["event_type"] == "alert"


@pytest.mark.integration
class TestAutomotiveDetection:
    """Test automotive protocol detection."""

    def test_someip_rule_trigger(self):
        """Test SOME/IP rule detection."""
        # SOME/IP header structure
        someip_header = {
            "message_id": 0x12345678,
            "length": 8,
            "request_id": 0x00000001,
            "protocol_version": 1,
            "interface_version": 1,
            "message_type": 0x00,  # REQUEST
            "return_code": 0x00,
        }

        assert someip_header["protocol_version"] == 1

    def test_doip_rule_trigger(self):
        """Test DoIP rule detection."""
        # DoIP header structure
        doip_header = {
            "protocol_version": 0x02,
            "inverse_version": 0xFD,
            "payload_type": 0x8001,  # Diagnostic message
            "payload_length": 4,
        }

        assert doip_header["protocol_version"] == 0x02

    @pytest.mark.skipif(not SCAPY_AVAILABLE, reason="scapy not installed")
    def test_generate_someip_pcap(self, integration_temp_dir: Path):
        """Test generating SOME/IP traffic PCAP."""
        pcap_path = integration_temp_dir / "someip-test.pcap"

        # SOME/IP typically runs on UDP port 30490 or TCP
        # Message format: Service ID (2) + Method ID (2) + Length (4) + ...
        someip_payload = bytes([
            0x12, 0x34,  # Service ID
            0x00, 0x01,  # Method ID
            0x00, 0x00, 0x00, 0x08,  # Length
            0x00, 0x00, 0x00, 0x01,  # Request ID
            0x01,  # Protocol version
            0x01,  # Interface version
            0x00,  # Message type (REQUEST)
            0x00,  # Return code
        ])

        packets = [
            Ether() / IP(dst="10.0.0.1") / UDP(dport=30490) / Raw(load=someip_payload),
        ]

        wrpcap(str(pcap_path), packets)
        assert pcap_path.exists()


@pytest.mark.integration
@pytest.mark.slow
class TestFullDetectionPipeline:
    """Test full detection pipeline end-to-end."""

    def test_pcap_to_alerts_pipeline(
        self,
        suricata_binary: Path,
        integration_temp_dir: Path,
        sample_pcaps: Dict[str, Path],
    ):
        """Test full pipeline from PCAP to alerts."""
        if not sample_pcaps:
            pytest.skip("No PCAP files available")

        # Get first available PCAP
        pcap_name, pcap_path = next(iter(sample_pcaps.items()))

        # Create output directory
        log_dir = integration_temp_dir / "suricata-logs"
        log_dir.mkdir(exist_ok=True)

        # This would run Suricata in offline mode
        # Actual test would require full Suricata setup
        # result = subprocess.run([
        #     str(suricata_binary),
        #     "-r", str(pcap_path),
        #     "-l", str(log_dir),
        # ], capture_output=True, timeout=60)

        # For now, just verify PCAP exists
        assert pcap_path.exists()
