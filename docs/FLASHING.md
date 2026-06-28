# Flashing the RAK4631 Ethernet Companion build

This repo builds a MeshCore nRF52/RAK4631 firmware target:

```text
RAK_4631_companion_radio_eth
```

The expected hardware is a RAK10720 / RAK19007 + RAK4631 + RAK13800 Ethernet module.

## Build outputs

After a successful build, artifacts are collected under:

```text
dist/RAK_4631_companion_radio_eth/
```

Expected files include:

```text
firmware.zip
RAK_4631_companion_radio_eth-dfu.zip
firmware.hex
firmware.bin
firmware.elf
firmware.map
BUILD_INFO.txt
```

## Use the DFU zip first

For RAK4631 / nRF52 serial DFU, use this file first:

```text
dist/RAK_4631_companion_radio_eth/firmware.zip
```

`firmware.zip` is a bootloader-friendly DFU package generated from `firmware.hex` using `adafruit-nrfutil dfu genpkg`.

Do **not** try to flash these first:

```text
firmware.elf
firmware.map
raw firmware.bin
raw firmware.hex
```

Those are useful for debugging or other tools, but the RAK4631 serial DFU bootloader normally wants the DFU `.zip` package.

## DFU package defaults

The package script defaults to:

```text
NRF_DFU_SD_REQ=auto from MeshCore boards/rak4631.json, usually 0x00B6
NRF_DFU_DEV_TYPE=0x0052
NRF_DFU_APP_VERSION=1
```

If the device rejects the package with a SoftDevice or device-type mismatch, rebuild with overrides:

```bash
NRF_DFU_SD_REQ=0x00B6 \
NRF_DFU_DEV_TYPE=0x0052 \
NRF_DFU_APP_VERSION=1 \
bash scripts/build_firmware.sh
```

## Flash with adafruit-nrfutil

Install the tool:

```bash
python3 -m pip install --user adafruit-nrfutil
export PATH="$HOME/.local/bin:$PATH"
```

Put the board into bootloader mode. Usually this is done by double-tapping reset, or by letting `adafruit-nrfutil` touch the port at 1200 baud.

Then flash:

```bash
adafruit-nrfutil --verbose dfu serial \
  --package dist/RAK_4631_companion_radio_eth/firmware.zip \
  -p /dev/ttyACM0 \
  -b 115200 \
  --singlebank \
  --touch 1200
```

On Windows, replace `/dev/ttyACM0` with the COM port, for example:

```powershell
adafruit-nrfutil --verbose dfu serial --package firmware.zip -p COM7 -b 115200 --singlebank --touch 1200
```

## If the device still rejects the firmware

Capture the exact error text and compare it against this list:

```text
Rejects before transfer starts:
- wrong package type
- wrong DFU dev type
- wrong SoftDevice requirement
- board not actually in bootloader mode

Rejects after transfer starts:
- image too large
- bad init packet
- bootloader/application layout mismatch

Flashes but does not boot:
- app crash early in setup
- bad SPI/Ethernet init blocking boot
- memory/layout problem
```

Fast recovery path:

```text
1. Double-tap reset into bootloader.
2. Flash a known-good RAK4631 MeshCore ZIP or UF2.
3. Confirm the board accepts known-good firmware.
4. Rebuild this repo and retry firmware.zip only.
```

## Flash UF2 if emitted

If the build emits a `.uf2` file and the RAK4631 appears as a USB mass-storage bootloader drive, copy the `.uf2` to that drive.

At the moment, the repo's primary supported artifact is the DFU `.zip`.

## First boot test

After flashing:

```text
1. Connect Ethernet to the RAK13800.
2. Power-cycle the RAK10720.
3. Check your router DHCP leases for the new device.
4. Test TCP port 4403 from a computer on the same LAN.
```

Example:

```bash
nc -vz DEVICE_IP 4403
```

A successful TCP connection only proves the Ethernet Companion API server is listening. The next test is a real MeshCore Companion API `DEVICE_QUERY` frame from Crow or a small test client.

## Recovery notes

If the board stops appearing normally:

```text
1. Double-tap reset to enter bootloader.
2. Flash a known-good RAK4631 MeshCore or Meshtastic build.
3. If needed, erase filesystem / settings from a known-good serial console.
```

Keep a known-good RAK4631 UF2/ZIP nearby before testing new Ethernet builds.
