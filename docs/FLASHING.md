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

Expected files may include one or more of:

```text
firmware.zip
firmware.hex
firmware.uf2
firmware.elf
firmware.map
BUILD_INFO.txt
```

Use whatever the build actually emits. For RAK4631 / nRF52 serial DFU, `firmware.zip` is usually the first file to try.

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

## Flash UF2 if emitted

If the build emits a `.uf2` file and the RAK4631 appears as a USB mass-storage bootloader drive, copy the `.uf2` to that drive.

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
