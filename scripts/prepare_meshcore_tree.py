#!/usr/bin/env python3
"""
Prepare an upstream MeshCore checkout to build the RAK4631 Ethernet repeater API target.

This script is intentionally idempotent. It copies this repo's overlay files into
MeshCore and applies small source/config edits needed for the new build env.
"""

from __future__ import annotations

import argparse
import shutil
from pathlib import Path

TARGET_ENV = "RAK_4631_repeater_eth_api"


def read(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def write(path: Path, text: str) -> None:
    path.write_text(text, encoding="utf-8")


def replace_once(text: str, old: str, new: str, label: str) -> str:
    if new in text:
        return text
    if old not in text:
        raise RuntimeError(f"could not find patch anchor for {label}")
    return text.replace(old, new, 1)


def copy_overlay(overlay: Path, meshcore: Path) -> None:
    src_dir = overlay / "meshcore_overlay" / "src" / "helpers" / "nrf52"
    dst_dir = meshcore / "src" / "helpers" / "nrf52"
    dst_dir.mkdir(parents=True, exist_ok=True)

    for name in (
        "EthernetCommandAPI.h",
        "EthernetCommandAPI.cpp",
        "EthernetSerialInterface.h",
        "EthernetSerialInterface.cpp",
    ):
        shutil.copy2(src_dir / name, dst_dir / name)
        print(f"copied src/helpers/nrf52/{name}")


def patch_simple_repeater_main(meshcore: Path) -> None:
    path = meshcore / "examples" / "simple_repeater" / "main.cpp"
    text = read(path)

    include_anchor = """#include <Mesh.h>

#include "MyMesh.h"
"""
    include_replacement = """#include <Mesh.h>

#ifdef WITH_ETHERNET_COMMAND_API
  #include <helpers/nrf52/EthernetCommandAPI.h>
#endif

#include "MyMesh.h"
"""
    text = replace_once(text, include_anchor, include_replacement, "simple_repeater ethernet include")

    global_anchor = """MyMesh the_mesh(board, radio_driver, *new ArduinoMillis(), fast_rng, rtc_clock, tables);

void halt() {
"""
    global_replacement = """MyMesh the_mesh(board, radio_driver, *new ArduinoMillis(), fast_rng, rtc_clock, tables);

#ifdef WITH_ETHERNET_COMMAND_API
EthernetCommandAPI eth_api;
static char eth_command[160];
#endif

void halt() {
"""
    text = replace_once(text, global_anchor, global_replacement, "simple_repeater ethernet globals")

    setup_anchor = """  the_mesh.begin(fs);

#ifdef DISPLAY_CLASS
"""
    setup_replacement = """  the_mesh.begin(fs);

#ifdef WITH_ETHERNET_COMMAND_API
  eth_api.begin(ETH_API_PORT);
#endif

#ifdef DISPLAY_CLASS
"""
    text = replace_once(text, setup_anchor, setup_replacement, "simple_repeater ethernet begin")

    loop_anchor = """  if (len > 0 && command[len - 1] == '\\r') {  // received complete line
    Serial.print('\\n');
    command[len - 1] = 0;  // replace newline with C string null terminator
    char reply[160];
    the_mesh.handleCommand(0, command, reply);  // NOTE: there is no sender_timestamp via serial!
    if (reply[0]) {
      Serial.print("  -> "); Serial.println(reply);
    }

    command[0] = 0;  // reset command buffer
  }

#if defined(PIN_USER_BTN) && defined(_SEEED_SENSECAP_SOLAR_H_)
"""
    loop_replacement = """  if (len > 0 && command[len - 1] == '\\r') {  // received complete line
    Serial.print('\\n');
    command[len - 1] = 0;  // replace newline with C string null terminator
    char reply[160];
    the_mesh.handleCommand(0, command, reply);  // NOTE: there is no sender_timestamp via serial!
    if (reply[0]) {
      Serial.print("  -> "); Serial.println(reply);
    }

    command[0] = 0;  // reset command buffer
  }

#ifdef WITH_ETHERNET_COMMAND_API
  while (eth_api.readCommand(eth_command, sizeof(eth_command))) {
    char reply[160];
    the_mesh.handleCommand(0, eth_command, reply);  // Ethernet API has no mesh sender timestamp.
    eth_api.writeReply(reply);
  }
#endif

#if defined(PIN_USER_BTN) && defined(_SEEED_SENSECAP_SOLAR_H_)
"""
    text = replace_once(text, loop_anchor, loop_replacement, "simple_repeater ethernet command loop")

    write(path, text)
    print("patched examples/simple_repeater/main.cpp")


def patch_companion_main(meshcore: Path) -> None:
    """Keep the older companion Ethernet target buildable, but it is not primary."""
    path = meshcore / "examples" / "companion_radio" / "main.cpp"
    text = read(path)

    old_select = """#elif defined(NRF52_PLATFORM)
  #ifdef BLE_PIN_CODE
    #include <helpers/nrf52/SerialBLEInterface.h>
    SerialBLEInterface serial_interface;
  #else
    #include <helpers/ArduinoSerialInterface.h>
    ArduinoSerialInterface serial_interface;
  #endif
"""
    new_select = """#elif defined(NRF52_PLATFORM)
  #ifdef WITH_ETHERNET_TCP_API
    #include <helpers/nrf52/EthernetSerialInterface.h>
    EthernetSerialInterface serial_interface;
  #elif defined(BLE_PIN_CODE)
    #include <helpers/nrf52/SerialBLEInterface.h>
    SerialBLEInterface serial_interface;
  #else
    #include <helpers/ArduinoSerialInterface.h>
    ArduinoSerialInterface serial_interface;
  #endif
"""
    text = replace_once(text, old_select, new_select, "NRF52 serial interface selection")

    old_begin = """  store.begin();
  the_mesh.begin(
    #ifdef DISPLAY_CLASS
        disp != NULL
    #else
        false
    #endif
  );

#ifdef BLE_PIN_CODE
  serial_interface.begin(BLE_NAME_PREFIX, the_mesh.getNodePrefs()->node_name, the_mesh.getBLEPin());
#else
  serial_interface.begin(Serial);
#endif
  the_mesh.startInterface(serial_interface);
#elif defined(RP2040_PLATFORM)
"""
    new_begin = """  store.begin();
  the_mesh.begin(
    #ifdef DISPLAY_CLASS
        disp != NULL
    #else
        false
    #endif
  );

#ifdef WITH_ETHERNET_TCP_API
  serial_interface.begin(TCP_PORT);
#elif defined(BLE_PIN_CODE)
  serial_interface.begin(BLE_NAME_PREFIX, the_mesh.getNodePrefs()->node_name, the_mesh.getBLEPin());
#else
  serial_interface.begin(Serial);
#endif
  the_mesh.startInterface(serial_interface);
#elif defined(RP2040_PLATFORM)
"""
    text = replace_once(text, old_begin, new_begin, "NRF52 serial interface begin")

    write(path, text)
    print("patched examples/companion_radio/main.cpp")


def patch_rak4631_variant(meshcore: Path) -> None:
    path = meshcore / "variants" / "rak4631" / "variant.h"
    text = read(path)

    old_spi_count = "#define SPI_INTERFACES_COUNT 1"
    new_spi_count = """#if defined(WITH_ETHERNET_TCP_API) || defined(WITH_ETHERNET_COMMAND_API)
// RAK13800/W5100S Ethernet module uses the WisBlock IO-slot SPI bus.
// RAK4631 LoRa uses a separate SPI bus on pins 43/44/45, so Ethernet
// must use SPI1 on WB_SPI pins 3/29/30 with CS on 26.
#define SPI_INTERFACES_COUNT 2
#else
#define SPI_INTERFACES_COUNT 1
#endif"""
    text = replace_once(text, old_spi_count, new_spi_count, "SPI_INTERFACES_COUNT")

    if "PIN_ETHERNET_SS" not in text:
        pin_anchor = """#define PIN_SPI_MISO (29)
#define PIN_SPI_MOSI (30)
#define PIN_SPI_SCK (3)
"""
        eth_defs = """#define PIN_SPI_MISO (29)
#define PIN_SPI_MOSI (30)
#define PIN_SPI_SCK (3)

#if defined(WITH_ETHERNET_TCP_API) || defined(WITH_ETHERNET_COMMAND_API)
#define PIN_SPI1_MISO (29)
#define PIN_SPI1_MOSI (30)
#define PIN_SPI1_SCK  (3)
#define PIN_3V3_EN    (34)
#define PIN_ETHERNET_RESET (21)
#define PIN_ETHERNET_SS    (26)
#define ETH_SPI_PORT  SPI1
#define ETH_SPI_SCK   PIN_SPI1_SCK
#define ETH_SPI_MISO  PIN_SPI1_MISO
#define ETH_SPI_MOSI  PIN_SPI1_MOSI
#endif
"""
        text = replace_once(text, pin_anchor, eth_defs, "RAK13800 ethernet pin defines")

    write(path, text)
    print("patched variants/rak4631/variant.h")


def patch_platformio(meshcore: Path, overlay: Path) -> None:
    path = meshcore / "variants" / "rak4631" / "platformio.ini"
    text = read(path)

    if f"[env:{TARGET_ENV}]" in text:
        print("platformio env already present")
        return

    addon = read(overlay / "meshcore_overlay" / "variants" / "rak4631_eth_gw" / "platformio.addon.ini")
    text = text.rstrip() + "\n\n" + addon.rstrip() + "\n"
    write(path, text)
    print("patched variants/rak4631/platformio.ini")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--meshcore", required=True, type=Path, help="Path to MeshCore checkout")
    parser.add_argument("--overlay", required=True, type=Path, help="Path to this overlay repo")
    args = parser.parse_args()

    meshcore = args.meshcore.resolve()
    overlay = args.overlay.resolve()

    if not (meshcore / "platformio.ini").exists():
        raise SystemExit(f"not a MeshCore checkout: {meshcore}")
    if not (overlay / "meshcore_overlay").exists():
        raise SystemExit(f"not this overlay repo: {overlay}")

    copy_overlay(overlay, meshcore)
    patch_simple_repeater_main(meshcore)
    patch_companion_main(meshcore)
    patch_rak4631_variant(meshcore)
    patch_platformio(meshcore, overlay)
    print("MeshCore tree prepared")


if __name__ == "__main__":
    main()
