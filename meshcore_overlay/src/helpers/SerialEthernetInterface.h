#pragma once

#include "BaseSerialInterface.h"
#include <RAK13800_W5100S.h>

#ifndef MAX_ETH_CLIENTS
#define MAX_ETH_CLIENTS 3
#endif

#ifndef ETH_FRAME_QUEUE_SIZE
#define ETH_FRAME_QUEUE_SIZE 16
#endif

class SerialEthernetInterface : public BaseSerialInterface {
  bool _isEnabled;
  bool _connected;

  EthernetServer *server;
  EthernetClient clients[MAX_ETH_CLIENTS];

  struct FrameHeader {
    uint8_t type;
    uint16_t length;
  };
  FrameHeader rx_header[MAX_ETH_CLIENTS];

  struct Frame {
    int8_t target;
    uint8_t len;
    uint8_t buf[MAX_FRAME_SIZE];
  };

  int send_queue_len;
  Frame send_queue[ETH_FRAME_QUEUE_SIZE];

  int _last_rx;
  int _rr;

public:
  SerialEthernetInterface() : server(nullptr) {
    _isEnabled = false;
    _connected = false;
    send_queue_len = 0;
    _last_rx = -1;
    _rr = 0;
    for (int i = 0; i < MAX_ETH_CLIENTS; i++) {
      rx_header[i].type = 0;
      rx_header[i].length = 0;
    }
  }

  void begin(int port);
  void enable() override;
  void disable() override;
  bool isEnabled() const override { return _isEnabled; }
  bool isConnected() const override { return _connected; }
  bool isWriteBusy() const override { return false; }
  size_t writeFrame(const uint8_t src[], size_t len) override;
  size_t checkRecvFrame(uint8_t dest[]) override;
};

#if ETH_DEBUG_LOGGING && ARDUINO
#include <Arduino.h>
#define ETH_DEBUG_PRINT(F, ...) Serial.printf("ETH: " F, ##__VA_ARGS__)
#define ETH_DEBUG_PRINTLN(F, ...) Serial.printf("ETH: " F "\n", ##__VA_ARGS__)
#else
#define ETH_DEBUG_PRINT(...) {}
#define ETH_DEBUG_PRINTLN(...) {}
#endif
