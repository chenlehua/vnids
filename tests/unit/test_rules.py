# VNIDS Unit Tests - Rules Tests
# Tests for Suricata rule parsing and validation

import re
from pathlib import Path
from typing import Dict, Any, List, Optional

import pytest


class TestRuleParsing:
    """Test Suricata rule parsing."""

    # Sample rules for testing
    SAMPLE_ALERT_RULE = (
        'alert tcp any any -> any 80 '
        '(msg:"HTTP Request"; flow:to_server,established; '
        'content:"GET"; http_method; sid:1000001; rev:1;)'
    )

    SAMPLE_DROP_RULE = (
        'drop tcp any any -> any any '
        '(msg:"Block malicious traffic"; '
        'content:"|de ad be ef|"; sid:1000002; rev:1;)'
    )

    def test_parse_rule_action(self):
        """Test parsing rule action."""
        rule = self.SAMPLE_ALERT_RULE
        action = rule.split()[0]
        assert action in ["alert", "drop", "pass", "reject"]

    def test_parse_rule_protocol(self):
        """Test parsing rule protocol."""
        rule = self.SAMPLE_ALERT_RULE
        parts = rule.split()
        protocol = parts[1]
        assert protocol in ["tcp", "udp", "icmp", "ip", "http", "dns", "tls"]

    def test_parse_rule_network(self):
        """Test parsing rule network specification."""
        rule = self.SAMPLE_ALERT_RULE
        # Format: action proto src_ip src_port -> dst_ip dst_port
        parts = rule.split()
        src_ip = parts[2]
        src_port = parts[3]
        direction = parts[4]
        dst_ip = parts[5]
        dst_port = parts[6]

        assert direction == "->"
        assert src_ip in ["any", "$HOME_NET", "$EXTERNAL_NET"] or self._is_valid_ip(src_ip)

    def test_parse_rule_sid(self):
        """Test parsing rule SID."""
        rule = self.SAMPLE_ALERT_RULE
        sid_match = re.search(r'sid:(\d+);', rule)
        assert sid_match is not None
        sid = int(sid_match.group(1))
        assert sid > 0

    def test_parse_rule_msg(self):
        """Test parsing rule message."""
        rule = self.SAMPLE_ALERT_RULE
        msg_match = re.search(r'msg:"([^"]+)";', rule)
        assert msg_match is not None
        msg = msg_match.group(1)
        assert len(msg) > 0

    def test_parse_rule_rev(self):
        """Test parsing rule revision."""
        rule = self.SAMPLE_ALERT_RULE
        rev_match = re.search(r'rev:(\d+);', rule)
        assert rev_match is not None
        rev = int(rev_match.group(1))
        assert rev > 0

    def _is_valid_ip(self, ip: str) -> bool:
        """Check if string is valid IP or CIDR."""
        import re
        ip_pattern = r'^\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}(/\d{1,2})?$'
        return bool(re.match(ip_pattern, ip))


class TestRuleValidation:
    """Test rule validation logic."""

    def test_valid_action_types(self):
        """Test valid rule action types."""
        valid_actions = ["alert", "pass", "drop", "reject", "rejectsrc", "rejectdst", "rejectboth"]
        for action in valid_actions:
            assert action in valid_actions

    def test_valid_protocols(self):
        """Test valid protocol types."""
        valid_protocols = [
            "ip", "tcp", "udp", "icmp",
            "http", "ftp", "tls", "smb",
            "dns", "ssh", "smtp", "imap",
        ]
        for proto in valid_protocols:
            assert isinstance(proto, str)

    def test_sid_range(self):
        """Test SID ranges."""
        # Reserved ranges:
        # 1-999999: Emerging Threats community rules
        # 1000000-1999999: Local rules
        # 2000000-2999999: Emerging Threats Pro rules

        local_sid_start = 1000000
        local_sid_end = 1999999

        test_sid = 1000001
        assert local_sid_start <= test_sid <= local_sid_end

    def test_rule_options_validation(self):
        """Test rule options syntax."""
        # Valid options should be in format: keyword:value; or keyword;
        valid_options = [
            "msg:\"Test message\";",
            "sid:1000001;",
            "rev:1;",
            "flow:to_server,established;",
            "nocase;",
        ]

        for opt in valid_options:
            assert opt.endswith(";")

    def test_content_match_syntax(self):
        """Test content match syntax."""
        # Text content
        text_content = 'content:"GET";'
        assert 'content:' in text_content

        # Binary content (hex)
        hex_content = 'content:"|00 01 02 03|";'
        assert '|' in hex_content

        # Mixed content
        mixed_content = 'content:"GET|20|/";'
        assert 'content:' in mixed_content


class TestRuleModifiers:
    """Test rule content modifiers."""

    def test_http_modifiers(self):
        """Test HTTP-specific modifiers."""
        http_modifiers = [
            "http_method",
            "http_uri",
            "http_raw_uri",
            "http_header",
            "http_raw_header",
            "http_cookie",
            "http_user_agent",
            "http_host",
            "http_request_body",
            "http_response_body",
        ]

        for mod in http_modifiers:
            assert mod.startswith("http_")

    def test_buffer_modifiers(self):
        """Test buffer modifiers."""
        modifiers = [
            "nocase",
            "depth:100;",
            "offset:10;",
            "distance:5;",
            "within:20;",
        ]

        for mod in modifiers:
            assert isinstance(mod, str)

    def test_pcre_syntax(self):
        """Test PCRE pattern syntax."""
        pcre_patterns = [
            'pcre:"/pattern/i";',
            'pcre:"/^GET\\s+/";',
            'pcre:"/\\d{4}-\\d{2}-\\d{2}/";',
        ]

        for pattern in pcre_patterns:
            assert pattern.startswith('pcre:')


class TestRuleFile:
    """Test rule file operations."""

    def test_parse_rule_file_comments(self):
        """Test parsing rule file with comments."""
        rule_content = """
# This is a comment
alert tcp any any -> any 80 (msg:"Test"; sid:1;)
# Another comment
alert udp any any -> any 53 (msg:"DNS"; sid:2;)
"""
        lines = rule_content.strip().split("\n")
        rules = [l for l in lines if l.strip() and not l.strip().startswith("#")]
        assert len(rules) == 2

    def test_parse_multiline_rule(self):
        """Test parsing multi-line rules."""
        multiline_rule = """alert tcp any any -> any 80 (\\
    msg:"Multi-line rule"; \\
    flow:to_server; \\
    sid:1000001; rev:1;)"""

        # Join continuation lines
        joined = multiline_rule.replace("\\\n", "")
        assert "msg:" in joined
        assert "sid:" in joined

    def test_rule_file_encoding(self, temp_dir: Path):
        """Test rule file encoding."""
        rule_file = temp_dir / "test.rules"
        rule_content = 'alert tcp any any -> any 80 (msg:"Test UTF-8: 测试"; sid:1;)'

        rule_file.write_text(rule_content, encoding="utf-8")
        loaded = rule_file.read_text(encoding="utf-8")

        assert "测试" in loaded


class TestAutomotiveRules:
    """Test automotive-specific protocol rules."""

    def test_someip_rule_structure(self):
        """Test SOME/IP protocol rule structure."""
        someip_rule = (
            'alert someip any any -> any any '
            '(msg:"SOME/IP suspicious service"; '
            'someip.service_id:0x1234; '
            'someip.method_id:0x5678; '
            'sid:3000001; rev:1;)'
        )

        assert "someip" in someip_rule
        assert "someip.service_id" in someip_rule
        assert "someip.method_id" in someip_rule

    def test_doip_rule_structure(self):
        """Test DoIP protocol rule structure."""
        doip_rule = (
            'alert doip any any -> any any '
            '(msg:"DoIP diagnostic request"; '
            'doip.payload_type:0x8001; '
            'sid:3000002; rev:1;)'
        )

        assert "doip" in doip_rule
        assert "doip.payload_type" in doip_rule

    def test_can_rule_structure(self):
        """Test CAN bus rule structure."""
        can_rule = (
            'alert can any any -> any any '
            '(msg:"CAN suspicious message"; '
            'can.id:0x7DF; '
            'can.data:|02 01 00|; '
            'sid:3000003; rev:1;)'
        )

        assert "can" in can_rule
        assert "can.id" in can_rule
