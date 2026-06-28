# MC_rak4631_eth_gw

Firmware overlay and project tracker for adding **RAK10720 / RAK4631 + RAK13800 Ethernet** support to MeshCore.

## Project goal

Build a MeshCore firmware target for the RAK10720 that exposes the normal MeshCore Companion API over wired Ethernet.

```text
RAK10720 Ethernet
→ RAK13800 W5100S TCP server
→ MeshCore Companion API frames
→ Crow / MeshCore client connects to tcp://DEVICE_IP:4403
```

The first successful firmware should let Crow or a MeshCore TCP client connect to the gateway IP address and use the normal Companion API without BLE, USB, Wi-Fi, or MQTT.

## Non-goals for the first working build

MQTT is **not required** for app or Crow access. Do not block the Ethernet Companion API work on MQTT.

For now, keep MQTT bridge work parked behind `WITH_MQTT_BRIDGE`; do not remove it, but do not make it part of the required first build.

## Current primary deliverable

```text
RAK_4631_companion_radio_eth
```

This target should expose the normal MeshCore Companion API over Ethernet TCP using the same framing as MeshCore USB/Wi-Fi Companion firmware:

```text
radio -> client:  '>' + uint16_le(length) + payload
client -> radio:  '<' + uint16_le(length) + payload
```

Expected TCP endpoint:

```text
DEVICE_IP:4403
```

## Flashable artifact

The file to load first on RAK4631 hardware is:

```text
dist/RAK_4631_companion_radio_eth/firmware.zip
```

The build script now generates this as a bootloader-friendly nRF52 DFU package from PlatformIO's `firmware.hex`.

The raw `.bin`, `.hex`, `.elf`, and `.map` files are collected for debugging, but they are not the primary RAK4631 serial DFU artifact.

## Optional future deliverable

```text
RAK_4631_companion_radio_eth_mqtt
```

This should only be enabled after the plain Ethernet API build works and firmware size, RAM use, and W5100S socket use are known to be safe.

## Target hardware

RAK10720 WisMesh Ethernet MQTT Gateway:

```text
RAK19007 base
RAK4631 nRF52840 + SX1262
RAK13800 W5100S Ethernet module
```

Key Ethernet assumptions:

```text
W5100S / RAK13800 on WisBlock IO-slot SPI
Ethernet CS: GPIO 26
Ethernet reset: GPIO 21
3V3 peripheral enable: GPIO 34
TCP API port: 4403
DHCP first; static IP later
```

## Repository strategy

This repo intentionally starts as an overlay instead of vendoring the full MeshCore tree.

Copy the files under `meshcore_overlay/` into a MeshCore checkout, then apply the patch notes in:

```text
patches/0001-add-rak4631-ethernet-companion-target.patch
```

## Files

```text
meshcore_overlay/src/helpers/nrf52/EthernetSerialInterface.h
meshcore_overlay/src/helpers/nrf52/EthernetSerialInterface.cpp
meshcore_overlay/variants/rak4631_eth_gw/platformio.addon.ini
patches/0001-add-rak4631-ethernet-companion-target.patch
docs/CROW_COMPATIBILITY.md
docs/FLASHING.md
scripts/apply_overlay.sh
scripts/build_firmware.sh
scripts/package_nrf52_dfu.py
scripts/prepare_meshcore_tree.py
```

## Build command

Preferred full build command from this repo:

```bash
bash scripts/build_firmware.sh
```

That clones MeshCore, applies the overlay, builds the PlatformIO target, and packages a flashable DFU zip.

Manual build inside a prepared MeshCore checkout:

```bash
pio run -e RAK_4631_companion_radio_eth
```

## Bring-up test plan

Minimum success path:

```text
1. Firmware compiles for RAK_4631_companion_radio_eth.
2. Build emits dist/RAK_4631_companion_radio_eth/firmware.zip.
3. firmware.zip flashes to RAK10720 / RAK4631 with adafruit-nrfutil.
4. RAK13800 powers up and obtains DHCP lease.
5. Device listens on TCP port 4403.
6. A TCP client connects to DEVICE_IP:4403.
7. Client sends MeshCore Companion DEVICE_QUERY / APP_START frames.
8. Firmware returns DEVICE_INFO / SELF_INFO frames.
9. Crow can query channel slots with CMD_GET_CHANNEL.
10. Crow can process queued inbound messages via PUSH_CODE_MSG_WAITING + CMD_SYNC_NEXT_MESSAGE.
```

## Open risks / things to verify

- Whether the RAK13800 library exposes exactly the same `Ethernet.init(ETH_SPI_PORT, cs)` overload expected by the current transport code.
- Whether the MeshCore RAK4631 variant needs a dedicated `rak4631_eth_gw` variant instead of conditional edits in `variants/rak4631/variant.h`.
- Final flash size under the nRF52840 SoftDevice layout.
- Runtime RAM use with Companion API queues plus Ethernet stack.
- W5100S socket availability if MQTT is later enabled at the same time as the TCP Companion API.
- Whether Crow's current TCP parser has been updated from the older `0x3E + cmd + len_be` assumption to MeshCore's real `<` / `>` little-endian frame transport.
- Whether the RAK4631 bootloader needs a DFU dev type other than `0x0052`; override with `NRF_DFU_DEV_TYPE` if hardware reports a mismatch.

## Current status

```text
Status: build repo now generates explicit nRF52 DFU zip; not hardware-verified yet.
Primary next step: build, flash dist/RAK_4631_companion_radio_eth/firmware.zip, and capture exact bootloader error if rejected.
```

## Change log

### 2026-06-27

Initial project setup.

Added:

- Root README with project purpose and first-build target.
- `EthernetSerialInterface.h` and `EthernetSerialInterface.cpp` overlay for MeshCore Companion API over RAK13800 Ethernet.
- `platformio.addon.ini` with primary `RAK_4631_companion_radio_eth` target.
- Parked optional `RAK_4631_companion_radio_eth_mqtt` target behind `WITH_MQTT_BRIDGE`.
- Patch notes for MeshCore integration edits.
- Crow compatibility notes documenting the required MeshCore TCP framing and message sync behavior.
- Overlay apply helper script.

Changed:

- Ethernet transport now defaults `ETH_DEBUG_LOGGING` to `0` to reduce firmware size and serial noise.
- SPI setup avoids non-portable `setSCK` / `setMISO` / `setMOSI` calls by default. The transport expects SPI1 pins to be supplied through MeshCore variant definitions.

Known incomplete:

- Not compile-tested.
- Not flash-tested on RAK10720.
- MQTT bridge is intentionally not implemented as a required feature yet.
- Crow still needs TCP send/response work if it has not already been updated.

### 2026-06-28

Flashability hardening.

Added:

- `scripts/package_nrf52_dfu.py` to create a bootloader-friendly RAK4631/nRF52 DFU package from PlatformIO `firmware.hex`.
- Build output now includes `RAK_4631_companion_radio_eth-dfu.zip` and generic `firmware.zip`.
- `docs/FLASHING.md` now explains which file to flash and how to diagnose rejection.

Changed:

- `scripts/build_firmware.sh` now installs/checks `adafruit-nrfutil`, builds, packages DFU zip, and records DFU metadata in `BUILD_INFO.txt`.
- GitHub Actions now pins Python to `3.11` and installs both `platformio` and `adafruit-nrfutil`.

Reason:

- RAK4631 serial DFU generally will not accept raw `.bin`, `.elf`, or unpackaged `.hex`; the bootloader path needs a DFU `.zip` package.

## Decision log

### MQTT is optional

MQTT is useful for broker-based packet bridging later, but it is not needed for the app or Crow API access. The required feature is Ethernet TCP Companion API.

### Companion firmware is the base

The Ethernet API build should be based on MeshCore `companion_radio`, not `simple_repeater`, because Crow needs Companion API commands such as device query, channel query, message sync, send text, and group/channel handling.

### Overlay first, upstream-style patch later

This repo begins as an overlay to move quickly. After the first successful build and hardware test, convert the overlay into a cleaner branch or pull request against MeshCore.
