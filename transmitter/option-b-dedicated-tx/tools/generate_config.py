#!/usr/bin/env python3
"""Generate validated radio settings for the option-B transmitter."""

import re
import sys
from pathlib import Path


EXPECTED_KEYS = {"TX_CHANNEL", "TX_FREQUENCY_HZ", "TX_MAC"}


def read_settings(path: Path) -> dict[str, str]:
    values: dict[str, str] = {}
    for line_number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        if not line.strip() or line.lstrip().startswith("#"):
            continue
        if "=" not in line:
            raise ValueError(f".env line {line_number}: expected KEY=<value>")
        key, value = line.split("=", 1)
        key = key.strip()
        if key not in EXPECTED_KEYS:
            raise ValueError(f".env line {line_number}: unknown key {key!r}")
        if key in values:
            raise ValueError(f".env line {line_number}: duplicate {key}")
        value = value.strip()
        if not (value.startswith("<") and value.endswith(">")):
            raise ValueError(f".env line {line_number}: {key} must use <value> delimiters")
        values[key] = value[1:-1].strip()

    missing = EXPECTED_KEYS - values.keys()
    if missing:
        raise ValueError(f".env: missing {', '.join(sorted(missing))}")
    return values


def parse_mac(value: str) -> list[int]:
    if not re.fullmatch(r"[0-9A-Fa-f]{2}(?::[0-9A-Fa-f]{2}){5}", value):
        raise ValueError(".env: TX_MAC must have the form 1A:00:00:00:00:01")
    octets = [int(part, 16) for part in value.split(":")]
    if octets[0] & 0x01:
        raise ValueError(".env: TX_MAC must be a unicast address")
    if not octets[0] & 0x02:
        raise ValueError(".env: TX_MAC must be locally administered (bit 1 of first byte set)")
    return octets


def render(settings: dict[str, str]) -> str:
    try:
        channel = int(settings["TX_CHANNEL"])
        frequency = int(settings["TX_FREQUENCY_HZ"])
    except ValueError as exc:
        raise ValueError(".env: TX_CHANNEL and TX_FREQUENCY_HZ must be integers") from exc
    if not 1 <= channel <= 13:
        raise ValueError(".env: TX_CHANNEL must be between 1 and 13")
    if not 1 <= frequency <= 500:
        raise ValueError(".env: TX_FREQUENCY_HZ must be between 1 and 500")
    mac = parse_mac(settings["TX_MAC"])
    mac_initializer = ", ".join(f"0x{octet:02x}" for octet in mac)
    return (
        "#pragma once\n"
        "/* Generated at build time from .env; do not edit. */\n"
        f"#define TX_CHANNEL {channel}\n"
        f"#define TX_FREQUENCY_HZ {frequency}\n"
        f"#define TX_MAC_BYTES {{{mac_initializer}}}\n"
        f'#define TX_MAC_STRING "{settings["TX_MAC"].upper()}"\n'
    )


def main() -> int:
    env_path, output_path = map(Path, sys.argv[1:])
    content = render(read_settings(env_path))
    output_path.parent.mkdir(parents=True, exist_ok=True)
    if not output_path.exists() or output_path.read_text(encoding="utf-8") != content:
        output_path.write_text(content, encoding="utf-8")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError) as exc:
        print(f"dedicated transmitter settings: {exc}", file=sys.stderr)
        raise SystemExit(1)
