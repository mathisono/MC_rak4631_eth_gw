#!/usr/bin/env python3
"""
Prepare an upstream MeshCore checkout to build the RAK4631 / RAK13800
Ethernet companion target.

Primary target now follows MeshCore PR #2679's experimental Ethernet companion
approach:
- RAK4631 + RAK13800/W5100S
- MeshCore companion protocol over TCP
- multi-client SerialEthernetInterface
- W5100S Ethernet hardware init deferred into loop()
"""

from __future__ import annotations

import argparse
import shutil
from pathlib import Path

TARGET_ENV = "RAK_4631_companion_radio_eth_clean"


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


def copy_if_present(src: Path, dst: Path, label: str) -> None:
    if src.exists():
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, dst)
        print(f"copied {label}")


def copy_overlay(overlay: Path, meshcore: Path) -> None:
    helpers_src = overlay / "meshcore_overlay" / "src" / "helpers"
    helpers_dst = meshcore / "src" / "helpers"
    nrf_src = helpers_src / "nrf52"
    nrf_dst = helpers_dst / "nrf52"

    # New upstream-style Ethernet companion transport from MeshCore PR #2679.
    copy_if_present(helpers_src / "SerialEthernetInterface.h", helpers_dst / "SerialEthernetInterface.h", "src/helpers/SerialEthernetInterface.h")
    copy_if_present(helpers_src / "SerialEthernetInterface.cpp", helpers_dst / "SerialEthernetInterface.cpp", "src/helpers/SerialEthernetInterface.cpp")

    # Keep legacy helper files available for older/manual targets, but the clean
    # target no longer depends on EthernetSerialInterface.
    for name in (
        "DualSerialInterface.h",
        "DualSerialInterface.cpp",
    ):
        copy_if_present(helpers_src / name, helpers_dst / name, f"src/helpers/{name}")

    for name in (
        "BLECommandAPI.h",
        "BLECommandAPI.cpp",
        "EthernetCommandAPI.h",
        "EthernetCommandAPI.cpp",
        "EthernetSerialInterface.h",
        "EthernetSerialInterface.cpp",
    ):
        copy_if_present(nrf_src / name, nrf_dst / name, f"src/helpers/nrf52/{name}")


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
  #if defined(WITH_ETHERNET_COMPANION)
    #include <SPI.h>
    #include <RAK13800_W5100S.h>
    #include <helpers/SerialEthernetInterface.h>
    SerialEthernetInterface serial_interface;
    // Dedicated SPI for RAK13800/W5100S on WisBlock IO-slot pins.
    // Keep it separate from LoRa SPI.
    SPIClass eth_spi(NRF_SPIM2, 29, 3, 30);  // MISO=29, SCK=3, MOSI=30
    uint8_t g_eth_mac[6] = {0};
    #ifndef TCP_PORT
      #define TCP_PORT 4403
    #endif
    #ifndef ETH_STATIC_IP
      #define ETH_STATIC_IP 192,168,3,55
    #endif
    #ifndef ETH_GATEWAY
      #define ETH_GATEWAY 192,168,3,3
    #endif
    #ifndef ETH_SUBNET
      #define ETH_SUBNET 255,255,255,0
    #endif
  #elif defined(WITH_DUAL_BLE_ETHERNET_COMPANION_API)
    #include <helpers/nrf52/SerialBLEInterface.h>
    #include <helpers/nrf52/EthernetSerialInterface.h>
    #include <helpers/DualSerialInterface.h>
    SerialBLEInterface ble_interface;
    EthernetSerialInterface eth_interface;
    DualSerialInterface serial_interface(ble_interface, eth_interface);
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

#if defined(WITH_ETHERNET_COMPANION)
  // Match MeshCore PR #2679: compute a stable local MAC here, but defer the
  // disruptive W5100S Ethernet.begin()/PHY reset into loop().
  g_eth_mac[0] = 0x02;
  uint32_t id0 = NRF_FICR->DEVICEID[0];
  uint32_t id1 = NRF_FICR->DEVICEID[1];
  g_eth_mac[1] = (id0 >> 24) & 0xFF;
  g_eth_mac[2] = (id0 >> 16) & 0xFF;
  g_eth_mac[3] = (id0 >> 8) & 0xFF;
  g_eth_mac[4] = id0 & 0xFF;
  g_eth_mac[5] = id1 & 0xFF;

  pinMode(34, OUTPUT); digitalWrite(34, HIGH);  // RAK19007 3V3 peripheral enable
  pinMode(21, OUTPUT); digitalWrite(21, LOW); delay(100); digitalWrite(21, HIGH); delay(100);
  pinMode(26, OUTPUT); digitalWrite(26, HIGH);  // W5100S CS idle high

  eth_spi.begin();
  Ethernet.init(eth_spi, 26);
  Serial.println("Ethernet companion: bring-up deferred to loop()");
#elif defined(WITH_DUAL_BLE_ETHERNET_COMPANION_API)
  ble_interface.begin(BLE_NAME_PREFIX, the_mesh.getNodePrefs()->node_name, the_mesh.getBLEPin());
  eth_interface.begin(TCP_PORT);
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

    if "Ethernet up (deferred):" not in text:
        loop_insert = r'''

#if defined(WITH_ETHERNET_COMPANION)
  static bool _eth_up = false;
  if (!_eth_up && millis() > 6000) {
#if defined(ETH_STATIC_ONLY)
    IPAddress sip(ETH_STATIC_IP), sgw(ETH_GATEWAY), ssn(ETH_SUBNET);
    Ethernet.begin(g_eth_mac, sip, sgw, sgw, ssn);
    serial_interface.begin(TCP_PORT);
#else
    Serial.println("Ethernet: trying DHCP (deferred)...");
    int dhcp_ok = Ethernet.begin(g_eth_mac, 12000, 4000);
    if (!dhcp_ok) {
      IPAddress sip(ETH_STATIC_IP), sgw(ETH_GATEWAY), ssn(ETH_SUBNET);
      Ethernet.begin(g_eth_mac, sip, sgw, sgw, ssn);
      Serial.println("Ethernet: DHCP failed -> static IP fallback");
    }
    serial_interface.begin(TCP_PORT);
#endif
    _eth_up = true;
    IPAddress ip = Ethernet.localIP();
    Serial.print("Ethernet up (deferred): ");
    Serial.print(ip[0]); Serial.print('.'); Serial.print(ip[1]); Serial.print('.');
    Serial.print(ip[2]); Serial.print('.'); Serial.print(ip[3]);
    Serial.print(":"); Serial.println(TCP_PORT);
  }
#if !defined(ETH_STATIC_ONLY)
  else if (_eth_up) {
    Ethernet.maintain();
  }
#endif
#endif
'''
        idx = text.rfind("\n}")
        if idx < 0:
            raise RuntimeError("could not find loop() closing brace")
        text = text[:idx] + loop_insert + text[idx:]

    write(path, text)
    print("patched examples/companion_radio/main.cpp for upstream Ethernet companion path")


def patch_companion_mymesh(meshcore: Path) -> None:
    path = meshcore / "examples" / "companion_radio" / "MyMesh.cpp"
    text = read(path)

    anchor = """  _prefs.gps_interval = constrain(_prefs.gps_interval, 0, 86400);  // Max 24 hours

#ifdef BLE_PIN_CODE // 123456 by default
"""
    replacement = """  _prefs.gps_interval = constrain(_prefs.gps_interval, 0, 86400);  // Max 24 hours

#ifdef FORCE_CLIENT_REPEAT
  _prefs.client_repeat = FORCE_CLIENT_REPEAT;
#endif

#ifdef BLE_PIN_CODE // 123456 by default
"""
    text = replace_once(text, anchor, replacement, "FORCE_CLIENT_REPEAT")

    write(path, text)
    print("patched examples/companion_radio/MyMesh.cpp")


def patch_rak4631_variant(meshcore: Path) -> None:
    path = meshcore / "variants" / "rak4631" / "variant.h"
    text = read(path)

    old_spi_count = "#define SPI_INTERFACES_COUNT 1"
    new_spi_count = """#if defined(WITH_ETHERNET_COMPANION) || defined(WITH_ETHERNET_TCP_API) || defined(WITH_ETHERNET_COMMAND_API) || defined(WITH_DUAL_BLE_ETHERNET_COMPANION_API)
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

#if defined(WITH_ETHERNET_COMPANION) || defined(WITH_ETHERNET_TCP_API) || defined(WITH_ETHERNET_COMMAND_API) || defined(WITH_DUAL_BLE_ETHERNET_COMPANION_API)
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

    base = overlay / "meshcore_overlay" / "variants" / "rak4631_eth_gw"
    app_api = read(base / "app_api.addon.ini")
    text = text.rstrip() + "\n\n" + app_api.rstrip() + "\n"
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
    patch_companion_main(meshcore)
    patch_companion_mymesh(meshcore)
    patch_rak4631_variant(meshcore)
    patch_platformio(meshcore, overlay)
    print("MeshCore tree prepared")


if __name__ == "__main__":
    main()
