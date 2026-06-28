# BLE + Ethernet repeater API mode

Yes: the repeater firmware can expose the same repeater command/API path over both BLE and Ethernet.

Primary target:

```text
RAK_4631_repeater_eth_ble_api
```

## Architecture

```text
MeshCore simple_repeater
        |
        v
 the_mesh.handleCommand(...)
        ^
        |
+----------------------+----------------------+
|                                             |
EthernetCommandAPI                           BLECommandAPI
TCP port 4403                                BLE UART
line-oriented text commands                  line-oriented text commands
```

Both transports feed the same MeshCore repeater command handler.

This is not MQTT.

This is also not the MeshCore Companion binary app API. It is the repeater/CommonCLI command API exposed over two transports.

## Ethernet usage

Connect to the device IP on TCP port 4403 and send text commands ending in `\r` or `\n`.

Example:

```bash
printf 'get name\r' | nc DEVICE_IP 4403
```

Expected first line on connection:

```text
MeshCore repeater Ethernet API ready
```

## BLE usage

The BLE side uses MeshCore's encrypted BLE UART helper. It advertises with this prefix by default:

```text
MC-RPT-
```

Default BLE pairing PIN:

```text
123456
```

A BLE UART client can send the same line-oriented commands:

```text
get name
get freq
get tx
advert
```

## Build target defaults

The local build script and GitHub Actions now default to:

```text
RAK_4631_repeater_eth_ble_api
```

Expected flash artifact:

```text
dist/RAK_4631_repeater_eth_ble_api/firmware.zip
```

## Related targets

```text
RAK_4631_repeater_eth_ble_api     primary: repeater + Ethernet API + BLE API, no MQTT
RAK_4631_repeater_eth_api         Ethernet-only fallback, no MQTT
RAK_4631_repeater_eth_ble_api_mqtt optional future combined MQTT target
RAK_4631_companion_radio_eth      legacy/experimental Companion binary API over Ethernet
```

## Notes

BLE and Ethernet can run at the same time because they are independent transports. They should not both own the mesh stack; they should only pass commands into the existing repeater handler.

If flash or RAM becomes tight, fall back to the Ethernet-only target first:

```bash
TARGET_ENV=RAK_4631_repeater_eth_api bash scripts/build_firmware.sh
```
