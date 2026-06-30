#include "SerialEthernetInterface.h"

void SerialEthernetInterface::begin(int port) {
  server = new EthernetServer(port);
  server->begin();
  ETH_DEBUG_PRINTLN("TCP companion server listening on %d", port);
}

void SerialEthernetInterface::enable() {
  if (_isEnabled) return;
  _isEnabled = true;
  send_queue_len = 0;
}

void SerialEthernetInterface::disable() {
  _isEnabled = false;
}

size_t SerialEthernetInterface::writeFrame(const uint8_t src[], size_t len) {
  if (len > MAX_FRAME_SIZE) {
    ETH_DEBUG_PRINTLN("writeFrame frame too big len=%d", (int)len);
    return 0;
  }
  if (!_connected || len == 0) return 0;

  if (send_queue_len >= ETH_FRAME_QUEUE_SIZE) {
    ETH_DEBUG_PRINTLN("writeFrame send queue full code=0x%02x", src[0]);
    return 0;
  }

  int8_t target = (src[0] >= 0x80) ? -1 : (int8_t)_last_rx;

  send_queue[send_queue_len].target = target;
  send_queue[send_queue_len].len = (uint8_t)len;
  memcpy(send_queue[send_queue_len].buf, src, len);
  send_queue_len++;
  return len;
}

size_t SerialEthernetInterface::checkRecvFrame(uint8_t dest[]) {
  if (server == nullptr) return 0;

  EthernetClient nc = server->accept();
  if (nc) {
    int slot = -1;
    for (int i = 0; i < MAX_ETH_CLIENTS; i++) {
      if (!clients[i].connected()) {
        slot = i;
        break;
      }
    }

    if (slot >= 0) {
      clients[slot].stop();
      clients[slot] = nc;
      rx_header[slot].type = 0;
      rx_header[slot].length = 0;
      ETH_DEBUG_PRINTLN("TCP client accepted slot=%d", slot);
    } else {
      nc.stop();
      ETH_DEBUG_PRINTLN("TCP client rejected all slots busy");
    }
  }

  bool any = false;
  for (int i = 0; i < MAX_ETH_CLIENTS; i++) {
    if (clients[i].connected()) {
      any = true;
    } else if (rx_header[i].type || rx_header[i].length) {
      rx_header[i].type = 0;
      rx_header[i].length = 0;
      clients[i].stop();
      ETH_DEBUG_PRINTLN("TCP client disconnected slot=%d", i);
    }
  }
  _connected = any;

  while (send_queue_len > 0) {
    Frame &f = send_queue[0];
    uint8_t pkt[3 + MAX_FRAME_SIZE];
    pkt[0] = '>';
    pkt[1] = f.len & 0xFF;
    pkt[2] = f.len >> 8;
    memcpy(&pkt[3], f.buf, f.len);

    if (f.target < 0) {
      for (int i = 0; i < MAX_ETH_CLIENTS; i++) {
        if (clients[i].connected()) clients[i].write(pkt, 3 + f.len);
      }
      ETH_DEBUG_PRINTLN("TCP tx broadcast len=%u hdr=%u", f.len, f.buf[0]);
    } else if (f.target < MAX_ETH_CLIENTS && clients[f.target].connected()) {
      clients[f.target].write(pkt, 3 + f.len);
      ETH_DEBUG_PRINTLN("TCP tx slot=%d len=%u hdr=%u", f.target, f.len, f.buf[0]);
    }

    send_queue_len--;
    for (int i = 0; i < send_queue_len; i++) {
      send_queue[i] = send_queue[i + 1];
    }
  }

  for (int k = 0; k < MAX_ETH_CLIENTS; k++) {
    int i = (_rr + k) % MAX_ETH_CLIENTS;
    EthernetClient &c = clients[i];
    if (!c.connected()) continue;

    if (rx_header[i].type == 0 || rx_header[i].length == 0) {
      if (c.available() >= 3) {
        c.readBytes(&rx_header[i].type, 1);
        c.readBytes((uint8_t *)&rx_header[i].length, 2);
      }
    }

    if (rx_header[i].type != 0 && rx_header[i].length != 0) {
      int frame_type = rx_header[i].type;
      int frame_length = rx_header[i].length;

      if (frame_length > c.available()) continue;

      if (frame_length > MAX_FRAME_SIZE || frame_type != '<') {
        while (frame_length > 0) {
          uint8_t skip[1];
          int n = c.read(skip, 1);
          if (n <= 0) break;
          frame_length -= n;
        }
        rx_header[i].type = 0;
        rx_header[i].length = 0;
        continue;
      }

      c.readBytes(dest, frame_length);
      rx_header[i].type = 0;
      rx_header[i].length = 0;
      _last_rx = i;
      _rr = (i + 1) % MAX_ETH_CLIENTS;
      ETH_DEBUG_PRINTLN("TCP rx slot=%d cmd=0x%02x len=%d", i, dest[0], frame_length);
      return frame_length;
    }
  }

  return 0;
}
