#!/usr/bin/env python3
"""
Create a RAK4631 / nRF52840 bootloader-friendly DFU zip from PlatformIO output.

Why this exists:
- `pio run` often leaves raw files such as firmware.hex / firmware.bin.
- RAK4631's serial DFU path usually wants a Nordic/Adafruit DFU package zip.
- Copying raw `.bin`, `.elf`, or an unpackaged `.hex` to the device will usually fail.

The defaults match the RAK4631 board definition used by MeshCore's PlatformIO tree:
- SoftDevice S140 v6.1.1 sd_fwid: 0x00B6
- Adafruit nRF52 dev type: 0x0052

Both can be overridden with environment variables if hardware reports a mismatch.
"""

from __future__ import annotations

import argparse
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path


def read_board_sd_req(meshcore_dir: Path, default: str = "0x00B6") -> str:
    board_file = meshcore_dir / "boards" / "rak4631.json"
    try:
        data = json.loads(board_file.read_text(encoding="utf-8"))
        return data["build"]["softdevice"]["sd_fwid"]
    except Exception as exc:  # noqa: BLE001 - this is a best-effort fallback
        print(f"warning: could not read {board_file}: {exc}; using {default}", file=sys.stderr)
        return default


def find_hex(build_dir: Path) -> Path:
    preferred = build_dir / "firmware.hex"
    if preferred.exists():
        return preferred

    matches = sorted(build_dir.glob("*.hex"))
    if matches:
        return matches[0]

    raise FileNotFoundError(f"no .hex firmware found in {build_dir}")


def require_tool(name: str) -> str:
    path = shutil.which(name)
    if not path:
        raise SystemExit(
            f"required tool not found: {name}\n"
            "Install it with: python3 -m pip install --user adafruit-nrfutil"
        )
    return path


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--meshcore", required=True, type=Path)
    parser.add_argument("--build-dir", required=True, type=Path)
    parser.add_argument("--out", required=True, type=Path)
    parser.add_argument("--target-env", default="RAK_4631_companion_radio_eth")
    args = parser.parse_args()

    meshcore_dir = args.meshcore.resolve()
    build_dir = args.build_dir.resolve()
    out_zip = args.out.resolve()
    out_zip.parent.mkdir(parents=True, exist_ok=True)

    nrfutil = require_tool("adafruit-nrfutil")
    hex_file = find_hex(build_dir)

    sd_req = os.environ.get("NRF_DFU_SD_REQ") or read_board_sd_req(meshcore_dir)
    dev_type = os.environ.get("NRF_DFU_DEV_TYPE", "0x0052")
    app_version = os.environ.get("NRF_DFU_APP_VERSION", "1")

    cmd = [
        nrfutil,
        "dfu",
        "genpkg",
        "--dev-type",
        dev_type,
        "--application-version",
        app_version,
        "--sd-req",
        sd_req,
        "--application",
        str(hex_file),
        str(out_zip),
    ]

    print("creating nRF52 DFU package")
    print(f"  target_env: {args.target_env}")
    print(f"  input_hex:  {hex_file}")
    print(f"  output_zip: {out_zip}")
    print(f"  dev_type:   {dev_type}")
    print(f"  sd_req:     {sd_req}")
    print(f"  app_ver:    {app_version}")
    subprocess.run(cmd, check=True)

    if not out_zip.exists() or out_zip.stat().st_size == 0:
        raise SystemExit(f"DFU package was not created: {out_zip}")

    print(f"created {out_zip} ({out_zip.stat().st_size} bytes)")


if __name__ == "__main__":
    main()
