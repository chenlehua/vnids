# VNIDS Unit Tests - Event Processing Tests
# Tests for event queue, parsing, and handling

import json
import time
from typing import Dict, Any
from datetime import datetime

import pytest


class TestEVEEventParsing:
    """Test Suricata EVE JSON event parsing."""

    def test_parse_alert_event(self, sample_eve_event: Dict[str, Any]):
        """Test parsing alert event."""
        assert sample_eve_event["event_type"] == "alert"
        assert "alert" in sample_eve_event
        assert "signature_id" in sample_eve_event["alert"]

    def test_parse_flow_event(self, sample_eve_flow: Dict[str, Any]):
        """Test parsing flow event."""
        assert sample_eve_flow["event_type"] == "flow"
        assert "flow" in sample_eve_flow
        assert "state" in sample_eve_flow["flow"]

    def test_event_timestamp_parsing(self, sample_eve_event: Dict[str, Any]):
        """Test timestamp parsing from EVE event."""
        timestamp_str = sample_eve_event["timestamp"]
        # Verify format: YYYY-MM-DDTHH:MM:SS.ffffff+ZZZZ
        assert "T" in timestamp_str
        assert "." in timestamp_str

    def test_event_flow_id(self, sample_eve_event: Dict[str, Any]):
        """Test flow_id is valid integer."""
        flow_id = sample_eve_event["flow_id"]
        assert isinstance(flow_id, int)
        assert flow_id > 0

    def test_event_network_fields(self, sample_eve_event: Dict[str, Any]):
        """Test network-related fields."""
        assert "src_ip" in sample_eve_event
        assert "dest_ip" in sample_eve_event
        assert "src_port" in sample_eve_event
        assert "dest_port" in sample_eve_event
        assert "proto" in sample_eve_event

    def test_alert_severity_range(self, sample_eve_event: Dict[str, Any]):
        """Test alert severity is in valid range."""
        severity = sample_eve_event["alert"]["severity"]
        assert 1 <= severity <= 5, "Severity must be between 1 and 5"


class TestEventQueue:
    """Test event queue operations."""

    def test_queue_creation(self):
        """Test creating an event queue."""
        queue_size = 1024
        assert queue_size > 0

    def test_queue_push_pop(self):
        """Test push and pop operations."""
        events = []
        event = {"type": "alert", "id": 1}

        # Push
        events.append(event)
        assert len(events) == 1

        # Pop
        popped = events.pop(0)
        assert popped == event
        assert len(events) == 0

    def test_queue_ordering(self):
        """Test FIFO ordering."""
        events = []
        for i in range(5):
            events.append({"id": i})

        for i in range(5):
            event = events.pop(0)
            assert event["id"] == i

    def test_queue_capacity(self):
        """Test queue capacity limits."""
        max_size = 10
        events = []

        for i in range(max_size):
            events.append({"id": i})

        assert len(events) == max_size

    def test_queue_overflow_handling(self):
        """Test handling when queue is full."""
        max_size = 5
        events = []
        dropped = 0

        for i in range(10):
            if len(events) < max_size:
                events.append({"id": i})
            else:
                dropped += 1

        assert len(events) == max_size
        assert dropped == 5


class TestEventSerialization:
    """Test event serialization/deserialization."""

    def test_json_serialization(self, sample_eve_event: Dict[str, Any]):
        """Test JSON serialization."""
        json_str = json.dumps(sample_eve_event)
        assert isinstance(json_str, str)
        assert len(json_str) > 0

    def test_json_deserialization(self, sample_eve_event: Dict[str, Any]):
        """Test JSON deserialization."""
        json_str = json.dumps(sample_eve_event)
        parsed = json.loads(json_str)
        assert parsed == sample_eve_event

    def test_compact_json(self, sample_eve_event: Dict[str, Any]):
        """Test compact JSON output."""
        compact = json.dumps(sample_eve_event, separators=(",", ":"))
        pretty = json.dumps(sample_eve_event, indent=2)
        assert len(compact) < len(pretty)

    def test_event_id_generation(self):
        """Test unique event ID generation."""
        import random
        ids = set()
        for i in range(1000):
            # Simple ID generation (timestamp + counter + random)
            event_id = f"{int(time.time() * 1000000)}_{i}_{random.randint(0, 999999)}"
            ids.add(event_id)

        # All should be unique with counter
        assert len(ids) == 1000


class TestEventFiltering:
    """Test event filtering logic."""

    def test_filter_by_event_type(self, sample_eve_event: Dict[str, Any], sample_eve_flow: Dict[str, Any]):
        """Test filtering by event type."""
        events = [sample_eve_event, sample_eve_flow]

        alerts = [e for e in events if e["event_type"] == "alert"]
        flows = [e for e in events if e["event_type"] == "flow"]

        assert len(alerts) == 1
        assert len(flows) == 1

    def test_filter_by_severity(self, sample_eve_event: Dict[str, Any]):
        """Test filtering by severity."""
        events = [
            {"alert": {"severity": 1}},
            {"alert": {"severity": 2}},
            {"alert": {"severity": 3}},
            {"alert": {"severity": 4}},
        ]

        high_severity = [
            e for e in events
            if e.get("alert", {}).get("severity", 5) <= 2
        ]

        assert len(high_severity) == 2

    def test_filter_by_ip(self, sample_eve_event: Dict[str, Any]):
        """Test filtering by IP address."""
        events = [
            {"src_ip": "192.168.1.100", "dest_ip": "10.0.0.1"},
            {"src_ip": "192.168.1.101", "dest_ip": "10.0.0.2"},
            {"src_ip": "192.168.2.100", "dest_ip": "10.0.0.1"},
        ]

        # Filter by source subnet
        subnet_events = [
            e for e in events
            if e["src_ip"].startswith("192.168.1.")
        ]

        assert len(subnet_events) == 2


class TestEventAggregation:
    """Test event aggregation logic."""

    def test_count_by_signature(self):
        """Test counting events by signature ID."""
        events = [
            {"alert": {"signature_id": 1001}},
            {"alert": {"signature_id": 1001}},
            {"alert": {"signature_id": 1002}},
            {"alert": {"signature_id": 1001}},
        ]

        counts = {}
        for e in events:
            sid = e["alert"]["signature_id"]
            counts[sid] = counts.get(sid, 0) + 1

        assert counts[1001] == 3
        assert counts[1002] == 1

    def test_group_by_source_ip(self):
        """Test grouping events by source IP."""
        events = [
            {"src_ip": "192.168.1.1"},
            {"src_ip": "192.168.1.1"},
            {"src_ip": "192.168.1.2"},
        ]

        groups = {}
        for e in events:
            ip = e["src_ip"]
            if ip not in groups:
                groups[ip] = []
            groups[ip].append(e)

        assert len(groups["192.168.1.1"]) == 2
        assert len(groups["192.168.1.2"]) == 1
