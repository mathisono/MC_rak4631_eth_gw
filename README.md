# MC_rak4631_eth_gw

Firmware overlay and project tracker for adding **RAK10720 / RAK4631 + RAK13800 Ethernet** support to MeshCore.

## Project goal

Build a MeshCore **repeater** firmware target for the RAK10720 that also exposes the repeater command/API interface over wired Ethernet.

```text
RAK10720 Ethernet
→ RAK13800 W5100S TCP server
→ MeshCore repeater CommonCLI command/API
→ TCP client connects to DEVICE_IP:4403
```

The first successful firmware should behave as a normal MeshCore repeater and respond to command/API requests over Ethernet without BLE, USB, Wi-Fi, or MQTT.

## Non-goals for the first working build

MQTT is **not required**. Do not block the Ethernet repeater API work on MQTT.

For now, keep MQTT bridge work parked behind `WITH_MQTT_BRIDGE`; do not remove it, but do not make it part of the required first build.

The legacy Companion binary API target is kept for reference, but it is not the primary hardware target.

## Current primary deliverable

```text
RAK_4631_repeater_eth_api
```

This target should run MeshCore's `simple_repeater` role and expose the existing repeater command handler over Ethernet TCP.

Expected TCP endpoint:

```text
DEVICE_IP:4403
```

Expected command style:

```text
get name
get freq
get tx
set name RAK4631-Repeater
advert
```

The Ethernet command API is line-oriented text. Send commands with `\r` or `\n`; the firmware responds with a text line.

## Flashable artifact

The file to load first on RAK4631 hardware is:

```text
dist/RAK_4631_repeater_eth_api/firmware.zip
```

The build script generates this as a bootloader-friendly nRF52 DFU package from PlatformIO's `firmware.hex`.

The raw `.bin`, `.hex`, `.elf`, and `.map` files are collected for debugging, but they are not the primary RAK4631 serial DFU artifact.

## Optional future deliverable

```text
RAK_4631_repeater_eth_api_mqtt
```

This should only be enabled after the plain repeater Ethernet API build works and firmware size, RAM use, and W5100S socket use are known to be safe.

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

The build script clones MeshCore, copies the overlay, patches the upstream tree, builds the PlatformIO target, and packages a flashable DFU zip.

## Files

```text
meshcore_overlay/src/helpers/nrf52/EthernetCommandAPI.h
meshcore_overlay/src/helpers/nrf52/EthernetCommandAPI.cpp
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

That now defaults to:

```text
RAK_4631_repeater_eth_api
```

Manual build inside a prepared MeshCore checkout:

```bash
pio run -e RAK_4631_repeater_eth_api
```

## Bring-up test plan

Minimum success path:

```text
1. Firmware compiles for RAK_4631_repeater_eth_api.
2. Build emits dist/RAK_4631_repeater_eth_api/firmware.zip.
3. firmware.zip flashes to RAK10720 / RAK4631 with adafruit-nrfutil.
4. RAK13800 powers up and obtains DHCP lease.
5. Device listens on TCP port 4403.
6. A TCP client connects to DEVICE_IP:4403.
7. Client sees: MeshCore repeater Ethernet API ready.
8. Client sends: get name
9. Firmware returns a text reply.
10. Repeater continues normal mesh forwarding behavior.
```

Example TCP test:

```bash
printf 'get name\r' | nc DEVICE_IP 4403
```

## Open risks / things to verify

- Whether the RAK13800 library exposes exactly the same `Ethernet.init(ETH_SPI_PORT, cs)` overload expected by the current transport code.
- Whether the MeshCore RAK4631 variant needs a dedicated `rak4631_eth_gw` variant instead of conditional edits in `variants/rak4631/variant.h`.
- Final flash size under the nRF52840 SoftDevice layout.
- Runtime RAM use with repeater role plus Ethernet stack.
- W5100S socket availability if MQTT is later enabled at the same time as the TCP command API.
- Whether the RAK4631 bootloader needs a DFU dev type other than `0x0052`; override with `NRF_DFU_DEV_TYPE` if hardware reports a mismatch.
- Whether the line-oriented command API is enough for Crow, or whether Crow still needs the Companion binary API later.

## Current status

```text
Status: primary target changed to repeater role with Ethernet command/API; not hardware-verified yet.
Primary next step: build, flash dist/RAK_4631_repeater_eth_api/firmware.zip, and test TCP command response on port 4403.
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

- Ethernet transport defaults `ETH_DEBUG_LOGGING` to `0` to reduce firmware size and serial noise.
- SPI setup avoids non-portable `setSCK` / `setMISO` / `setMOSI` calls by default. The transport expects SPI1 pins to be supplied through MeshCore variant definitions.

Known incomplete:

- Not compile-tested.
- Not flash-tested on RAK10720.
- MQTT bridge is intentionally not implemented as a required feature yet.

### 2026-06-28

Flashability hardening.

Added:

- `scripts/package_nrf52_dfu.py` to create a bootloader-friendly RAK4631/nRF52 DFU package from PlatformIO `firmware.hex`.
- Build output includes target-specific `*-dfu.zip` and generic `firmware.zip`.
- `docs/FLASHING.md` explains which file to flash and how to diagnose rejection.

Changed:

- `scripts/build_firmware.sh` installs/checks `adafruit-nrfutil`, builds, packages DFU zip, and records DFU metadata in `BUILD_INFO.txt`.
- GitHub Actions pins Python to `3.11` and installs both `platformio` and `adafruit-nrfutil`.

Reason:

- RAK4631 serial DFU generally will not accept raw `.bin`, `.elf`, or unpackaged `.hex`; the bootloader path needs a DFU `.zip` package.

### 2026-06-28 — repeater API pivot

Changed:

- Primary target is now `RAK_4631_repeater_eth_api`.
- Default local and GitHub Actions builds now use `RAK_4631_repeater_eth_api`.
- Added `EthernetCommandAPI.h/cpp`, a line-oriented TCP command server for the repeater role.
- `prepare_meshcore_tree.py` now patches `examples/simple_repeater/main.cpp` to call `the_mesh.handleCommand(...)` for commands received over Ethernet.
- MQTT remains disabled in the primary target.

Reason:

- Hardware should run as a repeater first, with Ethernet API enabled, and should respond over Ethernet without requiring MQTT.

## Decision log

### MQTT is optional

MQTT is useful for broker-based packet bridging later, but it is not needed for the first repeater Ethernet API build.

### Repeater firmware is the base

The primary Ethernet build is now based on MeshCore `simple_repeater`, not `companion_radio`, because the hardware needs to act as a repeater while exposing a command/API path over Ethernet.

### Overlay first, upstream-style patch later

This repo begins as an overlay to move quickly. After the first successful build and hardware test, convert the overlay into a cleaner branch or pull request against MeshCore.
