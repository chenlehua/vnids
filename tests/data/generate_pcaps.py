#!/usr/bin/env python3
"""
VNIDS Test Data Generator
Generates sample PCAP files for testing Suricata detection rules.
"""

import os
import sys
import struct
import argparse
from pathlib import Path
from datetime import datetime

# Check for scapy
try:
    from scapy.all import *
    SCAPY_AVAILABLE = True
except ImportError:
    SCAPY_AVAILABLE = False
    print("Warning: scapy not installed. Using raw packet generation.")


def generate_http_traffic(output_path: Path):
    """Generate HTTP traffic PCAP."""
    if not SCAPY_AVAILABLE:
        print(f"Skipping {output_path} - scapy not available")
        return

    packets = []

    # HTTP GET request
    packets.append(
        Ether(dst="ff:ff:ff:ff:ff:ff", src="00:11:22:33:44:55") /
        IP(src="192.168.1.100", dst="10.0.0.1") /
        TCP(sport=54321, dport=80, flags="PA", seq=1000, ack=1) /
        Raw(load=b"GET /api/v1/users HTTP/1.1\r\nHost: test.example.com\r\nUser-Agent: curl/7.68.0\r\n\r\n")
    )

    # HTTP response
    packets.append(
        Ether(dst="00:11:22:33:44:55", src="ff:ff:ff:ff:ff:ff") /
        IP(src="10.0.0.1", dst="192.168.1.100") /
        TCP(sport=80, dport=54321, flags="PA", seq=1, ack=1100) /
        Raw(load=b"HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: 50\r\n\r\n{\"status\": \"ok\", \"users\": []}")
    )

    # Directory traversal attempt
    packets.append(
        Ether(dst="ff:ff:ff:ff:ff:ff", src="00:11:22:33:44:66") /
        IP(src="192.168.1.101", dst="10.0.0.1") /
        TCP(sport=54322, dport=80, flags="PA", seq=1000, ack=1) /
        Raw(load=b"GET /files/../../../etc/passwd HTTP/1.1\r\nHost: test.example.com\r\n\r\n")
    )

    # SQL injection attempt
    packets.append(
        Ether(dst="ff:ff:ff:ff:ff:ff", src="00:11:22:33:44:77") /
        IP(src="192.168.1.102", dst="10.0.0.1") /
        TCP(sport=54323, dport=80, flags="PA", seq=1000, ack=1) /
        Raw(load=b"GET /search?q=test' UNION SELECT * FROM users-- HTTP/1.1\r\nHost: test.example.com\r\n\r\n")
    )

    wrpcap(str(output_path), packets)
    print(f"Generated: {output_path} ({len(packets)} packets)")


def generate_someip_traffic(output_path: Path):
    """Generate SOME/IP traffic PCAP."""
    if not SCAPY_AVAILABLE:
        print(f"Skipping {output_path} - scapy not available")
        return

    packets = []

    # SOME/IP service discovery (SD)
    # Service ID: 0xFFFF, Method ID: 0x8100
    someip_sd = struct.pack(">HH I I BBBB",
        0xFFFF,          # Service ID (SD)
        0x8100,          # Method ID (SD)
        0x00000018,      # Length
        0x00000001,      # Request ID
        0x01,            # Protocol version
        0x01,            # Interface version
        0x02,            # Message type (NOTIFICATION)
        0x00,            # Return code
    ) + b"\x00" * 16     # Payload

    packets.append(
        Ether() /
        IP(src="192.168.1.10", dst="224.224.224.245") /
        UDP(sport=30490, dport=30490) /
        Raw(load=someip_sd)
    )

    # SOME/IP method request
    someip_req = struct.pack(">HH I I BBBB",
        0x1234,          # Service ID
        0x0001,          # Method ID
        0x00000010,      # Length
        0x00000002,      # Request ID
        0x01,            # Protocol version
        0x01,            # Interface version
        0x00,            # Message type (REQUEST)
        0x00,            # Return code
    ) + b"\x01\x02\x03\x04\x05\x06\x07\x08"  # Payload

    packets.append(
        Ether() /
        IP(src="192.168.1.20", dst="192.168.1.30") /
        UDP(sport=30491, dport=30490) /
        Raw(load=someip_req)
    )

    # SOME/IP method response
    someip_resp = struct.pack(">HH I I BBBB",
        0x1234,          # Service ID
        0x0001,          # Method ID
        0x00000010,      # Length
        0x00000002,      # Request ID
        0x01,            # Protocol version
        0x01,            # Interface version
        0x80,            # Message type (RESPONSE)
        0x00,            # Return code (OK)
    ) + b"\x00\x00\x00\x00\x00\x00\x00\x00"

    packets.append(
        Ether() /
        IP(src="192.168.1.30", dst="192.168.1.20") /
        UDP(sport=30490, dport=30491) /
        Raw(load=someip_resp)
    )

    wrpcap(str(output_path), packets)
    print(f"Generated: {output_path} ({len(packets)} packets)")


def generate_doip_traffic(output_path: Path):
    """Generate DoIP traffic PCAP."""
    if not SCAPY_AVAILABLE:
        print(f"Skipping {output_path} - scapy not available")
        return

    packets = []

    # DoIP vehicle identification request
    doip_veh_id = struct.pack(">BB HI",
        0x02,           # Protocol version
        0xFD,           # Inverse protocol version
        0x0001,         # Payload type: Vehicle identification request
        0x00000000,     # Payload length
    )

    packets.append(
        Ether() /
        IP(src="192.168.1.50", dst="192.168.1.1") /
        TCP(sport=13401, dport=13400, flags="PA") /
        Raw(load=doip_veh_id)
    )

    # DoIP vehicle identification response
    doip_veh_resp = struct.pack(">BB HI",
        0x02,
        0xFD,
        0x0004,         # Payload type: Vehicle identification response
        0x00000021,     # Payload length (33 bytes)
    ) + b"VIN12345678901234" + bytes([0x00, 0x01]) + bytes(6) + bytes(6) + b"\x00"

    packets.append(
        Ether() /
        IP(src="192.168.1.1", dst="192.168.1.50") /
        TCP(sport=13400, dport=13401, flags="PA") /
        Raw(load=doip_veh_resp)
    )

    # DoIP routing activation request
    doip_route_act = struct.pack(">BB HI HBxxxxxx",
        0x02,
        0xFD,
        0x0005,         # Payload type: Routing activation request
        0x00000007,     # Payload length
        0x0E80,         # Source address
        0x00,           # Activation type
    )

    packets.append(
        Ether() /
        IP(src="192.168.1.50", dst="192.168.1.1") /
        TCP(sport=13401, dport=13400, flags="PA") /
        Raw(load=doip_route_act)
    )

    # DoIP diagnostic message (UDS request)
    uds_request = bytes([0x22, 0xF1, 0x90])  # Read DID 0xF190 (VIN)
    doip_diag = struct.pack(">BB HI HH",
        0x02,
        0xFD,
        0x8001,         # Payload type: Diagnostic message
        len(uds_request) + 4,
        0x0E80,         # Source address
        0x0001,         # Target address (ECU)
    ) + uds_request

    packets.append(
        Ether() /
        IP(src="192.168.1.50", dst="192.168.1.1") /
        TCP(sport=13401, dport=13400, flags="PA") /
        Raw(load=doip_diag)
    )

    wrpcap(str(output_path), packets)
    print(f"Generated: {output_path} ({len(packets)} packets)")


def generate_suspicious_traffic(output_path: Path):
    """Generate suspicious/malicious traffic PCAP."""
    if not SCAPY_AVAILABLE:
        print(f"Skipping {output_path} - scapy not available")
        return

    packets = []

    # Port scan (SYN packets to multiple ports)
    for port in [22, 23, 80, 443, 3389, 8080, 8443]:
        packets.append(
            Ether() /
            IP(src="10.0.0.100", dst="192.168.1.1") /
            TCP(sport=12345, dport=port, flags="S")
        )

    # Connection to Metasploit default port
    packets.append(
        Ether() /
        IP(src="192.168.1.200", dst="10.0.0.50") /
        TCP(sport=54321, dport=4444, flags="S")
    )

    # Large ICMP packet (possible covert channel)
    packets.append(
        Ether() /
        IP(src="192.168.1.201", dst="8.8.8.8") /
        ICMP(type=8, code=0) /
        Raw(load=b"A" * 1500)
    )

    # DNS TXT query (possible C2)
    packets.append(
        Ether() /
        IP(src="192.168.1.202", dst="8.8.8.8") /
        UDP(sport=54322, dport=53) /
        DNS(rd=1, qd=DNSQR(qname="malicious.example.com", qtype="TXT"))
    )

    wrpcap(str(output_path), packets)
    print(f"Generated: {output_path} ({len(packets)} packets)")


def main():
    parser = argparse.ArgumentParser(description="Generate test PCAP files")
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=Path(__file__).parent / "pcaps",
        help="Output directory for PCAP files"
    )
    parser.add_argument(
        "--type",
        choices=["all", "http", "someip", "doip", "suspicious"],
        default="all",
        help="Type of traffic to generate"
    )

    args = parser.parse_args()

    # Create output directory
    args.output_dir.mkdir(parents=True, exist_ok=True)

    if not SCAPY_AVAILABLE:
        print("ERROR: scapy is required for PCAP generation")
        print("Install with: pip install scapy")
        sys.exit(1)

    generators = {
        "http": (generate_http_traffic, "http-test.pcap"),
        "someip": (generate_someip_traffic, "someip-test.pcap"),
        "doip": (generate_doip_traffic, "doip-test.pcap"),
        "suspicious": (generate_suspicious_traffic, "suspicious-test.pcap"),
    }

    if args.type == "all":
        for name, (func, filename) in generators.items():
            func(args.output_dir / filename)
    else:
        func, filename = generators[args.type]
        func(args.output_dir / filename)

    print(f"\nPCAP files generated in: {args.output_dir}")


if __name__ == "__main__":
    main()
