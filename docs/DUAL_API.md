# BLE + Ethernet Companion app API mode

The MeshCore phone app needs the Companion app API, not the line-oriented repeater command API.

Primary target:

```text
RAK_4631_companion_repeater_eth_ble
```

## Architecture

```text
MeshCore companion_radio
        |
        v
 Companion app API handler
        ^
        |
+-------------------------+-------------------------+
|                                                   |
SerialBLEInterface                                 EthernetSerialInterface
MeshCore app over BLE                              MeshCore app over TCP/Ethernet
```

`DualSerialInterface` multiplexes the two transports into the single `BaseSerialInterface` pointer that `companion_radio` expects.

This is not MQTT.

This is the app-compatible Companion API path.

## Repeater behavior

This target uses `companion_radio`, not `simple_repeater`, because the app API already lives in `companion_radio`.

Repeater-like behavior is enabled by forcing:

```text
FORCE_CLIENT_REPEAT=1
```

That sets the companion firmware's `client_repeat` preference on boot.

## BLE usage

Use the normal MeshCore app BLE workflow. The BLE side uses MeshCore's existing BLE UART transport through `SerialBLEInterface`.

## Ethernet usage

Ethernet exposes the same Companion app frame transport over TCP port 4403.

```text
DEVICE_IP:4403
```

A client must speak the MeshCore Companion frame protocol. Plain `nc` text commands are not valid for this target.

## Build target defaults

The local build script and GitHub Actions now default to:

```text
RAK_4631_companion_repeater_eth_ble
```

Expected flash artifact:

```text
dist/RAK_4631_companion_repeater_eth_ble/firmware.zip
```

## Related targets

```text
RAK_4631_companion_repeater_eth_ble  primary: app API over BLE + Ethernet, repeat forced on, no MQTT
RAK_4631_companion_repeater_eth      Ethernet-only app API, repeat forced on, no MQTT
RAK_4631_repeater_eth_api            experimental text command server, not app-compatible
```

## Important note

The earlier `BLECommandAPI` / `EthernetCommandAPI` path is for text commands only. It is not compatible with the MeshCore phone app. Keep it as a diagnostic fallback, but do not use it as the main firmware path.
