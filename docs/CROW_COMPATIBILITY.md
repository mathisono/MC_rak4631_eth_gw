# Crow compatibility notes

The RAK10720 Ethernet target should expose MeshCore's normal Companion API over TCP.

## Correct transport framing

Crow should connect to the gateway IP on TCP port `4403` by default.

MeshCore Companion API TCP/serial framing is:

```text
radio -> client:  '>' + uint16_le(length) + payload
client -> radio:  '<' + uint16_le(length) + payload
```

The payload itself starts with the MeshCore Companion command/response code. The outer TCP frame is not:

```text
[ 0x3E ][ cmd ][ uint16_be length ][ payload ]
```

That older Crow parser assumption needs to be fixed.

## Message receive flow

MeshCore Companion firmware queues inbound messages and sends a tickle:

```text
PUSH_CODE_MSG_WAITING = 0x83
```

Crow should then request queued messages with:

```text
CMD_SYNC_NEXT_MESSAGE = 10 / 0x0A
```

Possible message responses include:

```text
RESP_CODE_CONTACT_MSG_RECV    = 7  / 0x07
RESP_CODE_CHANNEL_MSG_RECV    = 8  / 0x08
RESP_CODE_CONTACT_MSG_RECV_V3 = 16 / 0x10
RESP_CODE_CHANNEL_MSG_RECV_V3 = 17 / 0x11
RESP_CODE_NO_MORE_MESSAGES    = 10 / 0x0A
```

## Channel discovery

The discovery plan in Crow is mostly right:

```text
CMD_GET_CHANNEL        = 31 / 0x1F
RESP_CODE_CHANNEL_INFO = 18 / 0x12
```

Response payload layout:

```text
byte 0      response code 0x12
byte 1      channel index 0-7
bytes 2-33  channel name, 32 bytes, null-padded
bytes 34-49 16-byte channel secret
```

Crow still needs a real `sendCommand(cmd, payload)` helper to send framed commands and wait for matching responses.

## MQTT

MQTT is not required for the app or Crow API access.

Keep MQTT as a future optional bridge target only after:

```text
RAK_4631_companion_radio_eth
```

works and firmware size/RAM/socket count are acceptable.
