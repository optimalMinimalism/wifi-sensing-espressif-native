#!/usr/bin/env python3
"""Create a C header from the project's local, untracked .env file."""

import json
import sys
from pathlib import Path


def read_credentials(path: Path) -> dict[str, str]:
    values = {}
    for line_number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        if not line.strip() or line.lstrip().startswith("#"):
            continue
        if "=" not in line:
            raise ValueError(f".env line {line_number}: expected KEY=value")
        key, value = line.split("=", 1)
        key = key.strip()
        if key not in {"WIFI_SSID", "WIFI_PASSWORD"}:
            raise ValueError(f".env line {line_number}: unknown key {key!r}")
        if key in values:
            raise ValueError(f".env line {line_number}: duplicate {key}")
        if any(char in value for char in ("\x00", "\r", "\n")):
            raise ValueError(f".env line {line_number}: invalid control character")
        values[key] = value

    if not values.get("WIFI_SSID"):
        raise ValueError(".env: WIFI_SSID is required")
    if "WIFI_PASSWORD" not in values:
        raise ValueError(".env: WIFI_PASSWORD is required (use an empty value for an open AP)")
    if len(values["WIFI_SSID"].encode("utf-8")) > 32:
        raise ValueError(".env: WIFI_SSID exceeds 32 bytes")
    if len(values["WIFI_PASSWORD"].encode("utf-8")) > 64:
        raise ValueError(".env: WIFI_PASSWORD exceeds 64 bytes")
    return values


def main() -> int:
    env_path, output_path = map(Path, sys.argv[1:])
    credentials = read_credentials(env_path)
    content = (
        "#pragma once\n"
        "/* Generated at build time from .env; do not edit. */\n"
        f"#define WIFI_SSID {json.dumps(credentials['WIFI_SSID'], ensure_ascii=False)}\n"
        f"#define WIFI_PASSWORD {json.dumps(credentials['WIFI_PASSWORD'], ensure_ascii=False)}\n"
    )
    output_path.parent.mkdir(parents=True, exist_ok=True)
    if not output_path.exists() or output_path.read_text(encoding="utf-8") != content:
        output_path.write_text(content, encoding="utf-8")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError) as exc:
        print(f"Wi-Fi credentials: {exc}", file=sys.stderr)
        raise SystemExit(1)
