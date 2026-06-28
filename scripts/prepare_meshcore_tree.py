#!/usr/bin/env python3
"""
Prepare an upstream MeshCore checkout to build the RAK4631 companion BLE + Ethernet target.

This script is intentionally idempotent. It copies this repo's overlay files into
MeshCore and applies small source/config edits needed for the new build env.
"""

from __future__ import annotations

import argparse
import shutil
from pathlib import Path

TARGET_ENV = "RAK_4631_companion_repeater_eth_ble"


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
        "EthernetSerialInterface.h",
        "EthernetSerialInterface.cpp",
        "CompanionDualSerialInterface.h",
        "CompanionDualSerialInterface.cpp",
    ):
        shutil.copy2(src_dir / name, dst_dir / name)
        print(f"copied src/helpers/nrf52/{name}")


def patch_ethernet_serial_interface(meshcore: Path) -> None:
    path = meshcore / "src" / "helpers" / "nrf52" / "EthernetSerialInterface.cpp"
    text = read(path)

    text = replace_once(
        text,
        """    : deviceConnected(false), ethernetReady(false), _isEnabled(false), _port(TCP_PORT),
      lastDhcpAttempt(0), lastMaintain(0), server(nullptr), client(EthernetClient()) {
""",
        """    : deviceConnected(false), ethernetReady(false), ethernetStarted(false), _isEnabled(false), _port(TCP_PORT),
      beginMillis(0), lastDhcpAttempt(0), lastMaintain(0), server(nullptr), client(EthernetClient()) {
""",
        "ethernet delayed-start constructor",
    )

    text = replace_once(
        text,
        """bool EthernetSerialInterface::startEthernet() {
  powerUpEthernet();
""",
        """bool EthernetSerialInterface::startEthernet() {
  ethernetStarted = true;
  powerUpEthernet();
""",
        "ethernetStarted marker",
    )

    text = replace_once(
        text,
        """void EthernetSerialInterface::begin(int port) {
  _port = port;

  if (!startEthernet()) {
    // Leave server unset until DHCP succeeds during serviceEthernet().
    return;
  }

  if (server) {
    delete server;
  }
  server = new EthernetServer(_port);
  server->begin();
  ETH_DEBUG_PRINTLN("TCP companion server listening on %d", _port);
}
""",
        """void EthernetSerialInterface::begin(int port) {
  _port = port;
  beginMillis = millis();
  lastDhcpAttempt = beginMillis;

#if ETH_START_DELAY_MS == 0
  if (!startEthernet()) {
    return;
  }

  if (server) {
    delete server;
  }
  server = new EthernetServer(_port);
  server->begin();
  ETH_DEBUG_PRINTLN("TCP companion server listening on %d", _port);
#else
  ETH_DEBUG_PRINTLN("Ethernet startup delayed by %lu ms", (unsigned long)ETH_START_DELAY_MS);
#endif
}
""",
        "delayed Ethernet begin",
    )

    text = replace_once(
        text,
        """  if (!ethernetReady) {
    if (now - lastDhcpAttempt >= ETH_DHCP_RETRY_MS) {
      if (startEthernet()) {
""",
        """  if (!ethernetReady) {
#if ETH_START_DELAY_MS > 0
    if (!ethernetStarted && now - beginMillis < ETH_START_DELAY_MS) {
      return;
    }
#endif
    if (!ethernetStarted || now - lastDhcpAttempt >= ETH_DHCP_RETRY_MS) {
      if (startEthernet()) {
""",
        "delayed Ethernet service retry",
    )

    write(path, text)
    print("patched src/helpers/nrf52/EthernetSerialInterface.cpp for delayed Ethernet startup")


def patch_companion_main(meshcore: Path) -> None:
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
  #if defined(WITH_ETHERNET_TCP_API) && defined(BLE_PIN_CODE)
    #include <helpers/nrf52/CompanionDualSerialInterface.h>
    CompanionDualSerialInterface serial_interface;
  #elif defined(WITH_ETHERNET_TCP_API)
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

#if defined(WITH_ETHERNET_TCP_API) && defined(BLE_PIN_CODE)
  serial_interface.begin(BLE_NAME_PREFIX, the_mesh.getNodePrefs()->node_name, the_mesh.getBLEPin(), TCP_PORT);
#elif defined(WITH_ETHERNET_TCP_API)
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


def patch_companion_mymesh(meshcore: Path) -> None:
    path = meshcore / "examples" / "companion_radio" / "MyMesh.cpp"
    text = read(path)

    old_defaults = """  _prefs.tx_power_dbm = constrain(_prefs.tx_power_dbm, -9, MAX_LORA_TX_POWER);
  _prefs.gps_enabled = constrain(_prefs.gps_enabled, 0, 1);  // Ensure boolean 0 or 1
  _prefs.gps_interval = constrain(_prefs.gps_interval, 0, 86400);  // Max 24 hours

#ifdef BLE_PIN_CODE // 123456 by default
"""
    new_defaults = """  _prefs.tx_power_dbm = constrain(_prefs.tx_power_dbm, -9, MAX_LORA_TX_POWER);
  _prefs.gps_enabled = constrain(_prefs.gps_enabled, 0, 1);  // Ensure boolean 0 or 1
  _prefs.gps_interval = constrain(_prefs.gps_interval, 0, 86400);  // Max 24 hours
#ifdef FORCE_CLIENT_REPEAT
  _prefs.client_repeat = 1;
#endif

#ifdef BLE_PIN_CODE // 123456 by default
"""
    text = replace_once(text, old_defaults, new_defaults, "client repeat default")

    old_repeat = """    uint8_t repeat = 0;  // default - false
    if (len > i) {
      repeat = cmd_frame[i++];   // FIRMWARE_VER_CODE  9+
    }

    if (repeat && !isValidClientRepeatFreq(freq)) {
"""
    new_repeat = """    uint8_t repeat = 0;  // default - false
    if (len > i) {
      repeat = cmd_frame[i++];   // FIRMWARE_VER_CODE  9+
    }

#ifdef FORCE_CLIENT_REPEAT
    repeat = 1;
#endif

    if (repeat && !isValidClientRepeatFreq(freq)) {
"""
    text = replace_once(text, old_repeat, new_repeat, "forced client repeat")

    write(path, text)
    print("patched examples/companion_radio/MyMesh.cpp")


def patch_rak4631_variant(meshcore: Path) -> None:
    path = meshcore / "variants" / "rak4631" / "variant.h"
    text = read(path)

    old_spi_count = "#define SPI_INTERFACES_COUNT 1"
    new_spi_count = """#if defined(WITH_ETHERNET_TCP_API) || defined(WITH_DUAL_BLE_ETHERNET_COMPANION_API)
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

#if defined(WITH_ETHERNET_TCP_API) || defined(WITH_DUAL_BLE_ETHERNET_COMPANION_API)
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

    base = overlay / "meshcore_overlay" / "variants" / "rak4631_eth_gw"
    for addon in sorted(base.glob("*.addon.ini")):
        addon_text = read(addon).rstrip()
        if addon_text not in text:
            text = text.rstrip() + "\n\n" + addon_text + "\n"

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
    patch_ethernet_serial_interface(meshcore)
    patch_companion_main(meshcore)
    patch_companion_mymesh(meshcore)
    patch_rak4631_variant(meshcore)
    patch_platformio(meshcore, overlay)
    print("MeshCore tree prepared")


if __name__ == "__main__":
    main()
