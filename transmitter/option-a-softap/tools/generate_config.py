#!/usr/bin/env python3
"""Generate private SoftAP settings from transmitter/option-a-softap/.env at build time."""

import json
import sys
from pathlib import Path

KEYS = {"AP_SSID", "AP_PASSWORD", "CONTROL_TOKEN", "AP_CHANNEL"}


def read_env(path: Path) -> dict[str, str]:
    values: dict[str, str] = {}
    for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        if not line.strip() or line.lstrip().startswith("#"):
            continue
        if "=" not in line:
            raise ValueError(f"line {number}: expected KEY=<value>")
        key, raw = line.split("=", 1)
        key, raw = key.strip(), raw.strip()
        if key not in KEYS or key in values:
            raise ValueError(f"line {number}: unknown or duplicate key {key!r}")
        if not (raw.startswith("<") and raw.endswith(">")):
            raise ValueError(f"line {number}: {key} must use <value> delimiters")
        values[key] = raw[1:-1]
    if set(values) != KEYS:
        raise ValueError(f"missing: {', '.join(sorted(KEYS - set(values)))}")
    ssid = values["AP_SSID"].encode("utf-8")
    password = values["AP_PASSWORD"].encode("utf-8")
    token = values["CONTROL_TOKEN"].encode("utf-8")
    if not 1 <= len(ssid) <= 32:
        raise ValueError("AP_SSID must be 1-32 bytes")
    if not 8 <= len(password) <= 63:
        raise ValueError("AP_PASSWORD must be 8-63 bytes for WPA2")
    if not 16 <= len(token) <= 64:
        raise ValueError("CONTROL_TOKEN must be 16-64 bytes")
    if any(ord(char) < 32 or ord(char) > 126 for char in values["CONTROL_TOKEN"]):
        raise ValueError("CONTROL_TOKEN must contain printable ASCII")
    if not values["AP_CHANNEL"].isdigit() or not 1 <= int(values["AP_CHANNEL"]) <= 11:
        raise ValueError("AP_CHANNEL must be 1-11")
    return values


def main() -> None:
    env_file, output_file = map(Path, sys.argv[1:])
    values = read_env(env_file)
    output = (
        "#pragma once\n"
        "/* Generated from local .env at build time. */\n"
        f"#define AP_SSID {json.dumps(values['AP_SSID'], ensure_ascii=False)}\n"
        f"#define AP_PASSWORD {json.dumps(values['AP_PASSWORD'], ensure_ascii=False)}\n"
        f"#define CONTROL_TOKEN {json.dumps(values['CONTROL_TOKEN'])}\n"
        f"#define AP_CHANNEL {int(values['AP_CHANNEL'])}\n"
    )
    output_file.parent.mkdir(parents=True, exist_ok=True)
    if not output_file.exists() or output_file.read_text(encoding="utf-8") != output:
        output_file.write_text(output, encoding="utf-8")


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError) as error:
        print(f"SoftAP configuration: {error}", file=sys.stderr)
        raise SystemExit(1)
