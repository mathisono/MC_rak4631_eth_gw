#pragma once

#include "../BaseSerialInterface.h"
#include <Arduino.h>

#ifdef WITH_ETHERNET_TCP_API

#include <SPI.h>
#include <RAK13800_W5100S.h>

#ifndef TCP_PORT
#define TCP_PORT 4403
#endif

#ifndef ETH_FRAME_QUEUE_SIZE
#define ETH_FRAME_QUEUE_SIZE 4
#endif

#ifndef ETH_DHCP_RETRY_MS
#define ETH_DHCP_RETRY_MS 5000UL
#endif

#ifndef ETH_MAINTAIN_MS
#define ETH_MAINTAIN_MS 1000UL
#endif

#ifndef ETH_START_DELAY_MS
#define ETH_START_DELAY_MS 0UL
#endif

#ifndef ETH_SPI_SCK
#define ETH_SPI_SCK 3
#endif

#ifndef ETH_SPI_MISO
#define ETH_SPI_MISO 29
#endif

#ifndef ETH_SPI_MOSI
#define ETH_SPI_MOSI 30
#endif

#ifndef PIN_ETHERNET_SS
#define PIN_ETHERNET_SS 26
#endif

#ifndef PIN_ETHERNET_RESET
#define PIN_ETHERNET_RESET 21
#endif

#ifndef PIN_3V3_EN
#define PIN_3V3_EN 34
#endif

#ifndef ETH_SPI_PORT
#define ETH_SPI_PORT SPI1
#endif

/**
 * MeshCore Companion API over RAK13800/W5100S Ethernet.
 *
 * This intentionally mirrors MeshCore's SerialWifiInterface and
 * ArduinoSerialInterface framing:
 *
 *   radio -> client:  '>' + uint16_le(length) + payload
 *   client -> radio:  '<' + uint16_le(length) + payload
 *
 * It is a transport wrapper only. The Companion API command handling
 * remains in examples/companion_radio/MyMesh.cpp.
 */
class EthernetSerialInterface : public BaseSerialInterface {
  bool deviceConnected;
  bool ethernetReady;
  bool ethernetStarted;
  bool _isEnabled;
  uint16_t _port;
  unsigned long beginMillis;
  unsigned long lastDhcpAttempt;
  unsigned long lastMaintain;

  EthernetServer *server;
  // RAK13800's EthernetClient::connected() is not const, but MeshCore's
  // interface requires isConnected() const. Keep the client mutable so the
  // transport can report connection state without breaking the interface.
  mutable EthernetClient client;

  struct FrameHeader {
    uint8_t type;
    uint16_t length;
  };

  struct Frame {
    uint16_t len;
    uint8_t buf[MAX_FRAME_SIZE];
  };

  FrameHeader received_frame_header;

  int recv_queue_len;
  Frame recv_queue[ETH_FRAME_QUEUE_SIZE];
  int send_queue_len;
  Frame send_queue[ETH_FRAME_QUEUE_SIZE];
  unsigned long lastDelayLog;

  void clearBuffers();
  void resetReceivedFrameHeader();
  bool hasReceivedFrameHeader() const;
  void makeMac(uint8_t mac[6]);
  void powerUpEthernet();
  void resetEthernet();
  bool startEthernet();
  void serviceEthernet();
  void serviceClient();
  void dropClient();

public:
  EthernetSerialInterface();
  ~EthernetSerialInterface();

  void begin(int port = TCP_PORT);

  void enable() override;
  void disable() override;
  bool isEnabled() const override { return _isEnabled; }

  bool isConnected() const override;
  bool isWriteBusy() const override;

  size_t writeFrame(const uint8_t src[], size_t len) override;
  size_t checkRecvFrame(uint8_t dest[]) override;

  bool isEthernetReady() const { return ethernetReady; }
  bool hasStartedEthernet() const { return ethernetStarted; }
  uint16_t port() const { return _port; }
};

#endif // WITH_ETHERNET_TCP_API
