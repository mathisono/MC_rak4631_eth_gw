# MC_rak4631_eth_gw

Overlay code for adding **RAK10720 / RAK4631 + RAK13800 Ethernet** support to MeshCore.

Goal:

```text
RAK10720 Ethernet
→ RAK13800 W5100S TCP server
→ MeshCore Companion API frames
→ Crow / MeshCore client connects to tcp://DEVICE_IP:4403
```

This repo intentionally starts as an overlay instead of vendoring the full MeshCore tree. Copy the files under `meshcore_overlay/` into a MeshCore checkout, then apply the patch notes in `patches/0001-add-rak4631-ethernet-companion-target.patch`.

## Current scope

Primary deliverable:

```text
RAK_4631_companion_radio_eth
```

This exposes the normal MeshCore Companion API over Ethernet TCP using the same framing as USB/Wi-Fi Companion firmware:

```text
radio -> client:  '>' + uint16_le(length) + payload
client -> radio:  '<' + uint16_le(length) + payload
```

MQTT is **not required** for the app or Crow API access. MQTT bridge code should stay optional and behind `WITH_MQTT_BRIDGE` later.

## Files

```text
meshcore_overlay/src/helpers/nrf52/EthernetSerialInterface.h
meshcore_overlay/src/helpers/nrf52/EthernetSerialInterface.cpp
meshcore_overlay/variants/rak4631_eth_gw/platformio.addon.ini
patches/0001-add-rak4631-ethernet-companion-target.patch
docs/CROW_COMPATIBILITY.md
```

## Target hardware

RAK10720 WisMesh Ethernet MQTT Gateway:

```text
RAK19007 base
RAK4631 nRF52840 + SX1262
RAK13800 W5100S Ethernet module
```

## Build intent

First build only the Ethernet Companion API target. After that works and size/RAM look okay, try a second combined target with MQTT enabled.

```bash
pio run -e RAK_4631_companion_radio_eth
```

Expected TCP endpoint:

```text
DEVICE_IP:4403
```

## Status

Initial overlay: code committed, needs to be copied into MeshCore and compile-tested on hardware.
