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

#ifndef ETH_USE_STATIC_IP
#define ETH_USE_STATIC_IP 0
#endif

#ifndef ETH_DISABLE_DHCP
#define ETH_DISABLE_DHCP 0
#endif

#ifndef ETH_PING_ONLY
#define ETH_PING_ONLY 0
#endif

#ifndef ETH_DISABLE_MAINTAIN
#define ETH_DISABLE_MAINTAIN 0
#endif

#ifndef ETH_NO_REINIT_AFTER_SUCCESS
#define ETH_NO_REINIT_AFTER_SUCCESS 0
#endif

#ifndef ETH_TCP_SERVER_DEBUG
#define ETH_TCP_SERVER_DEBUG 0
#endif

#ifndef ETH_STATIC_IP_A
#define ETH_STATIC_IP_A 0
#endif
#ifndef ETH_STATIC_IP_B
#define ETH_STATIC_IP_B 0
#endif
#ifndef ETH_STATIC_IP_C
#define ETH_STATIC_IP_C 0
#endif
#ifndef ETH_STATIC_IP_D
#define ETH_STATIC_IP_D 0
#endif

#ifndef ETH_GATEWAY_A
#define ETH_GATEWAY_A 0
#endif
#ifndef ETH_GATEWAY_B
#define ETH_GATEWAY_B 0
#endif
#ifndef ETH_GATEWAY_C
#define ETH_GATEWAY_C 0
#endif
#ifndef ETH_GATEWAY_D
#define ETH_GATEWAY_D 0
#endif

#ifndef ETH_SUBNET_A
#define ETH_SUBNET_A 0
#endif
#ifndef ETH_SUBNET_B
#define ETH_SUBNET_B 0
#endif
#ifndef ETH_SUBNET_C
#define ETH_SUBNET_C 0
#endif
#ifndef ETH_SUBNET_D
#define ETH_SUBNET_D 0
#endif

#ifndef ETH_DNS_A
#define ETH_DNS_A 0
#endif
#ifndef ETH_DNS_B
#define ETH_DNS_B 0
#endif
#ifndef ETH_DNS_C
#define ETH_DNS_C 0
#endif
#ifndef ETH_DNS_D
#define ETH_DNS_D 0
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
  unsigned long lastServiceLog;
  unsigned long lastClientLog;

  EthernetServer *server;
  // RAK13800's EthernetClient::connected() is not const, but MeshCore's
  // interface requires isConnected() const. Keep the client mutable so the
  // transport can report connection state without breaking the interface.
  mutable EthernetClient client;
  mutable EthernetClient pendingClient;

  struct FrameHeader {
    uint8_t type;
    uint16_t length;
  };

  struct Frame {
    uint16_t len;
    uint8_t buf[MAX_FRAME_SIZE];
  };

  FrameHeader received_frame_header;
  FrameHeader pending_received_frame_header;

  int recv_queue_len;
  Frame recv_queue[ETH_FRAME_QUEUE_SIZE];
  int send_queue_len;
  Frame send_queue[ETH_FRAME_QUEUE_SIZE];
  unsigned long lastDelayLog;
  unsigned long pendingClientSince;
  bool pendingClientValid;
  uint8_t injected_frame_buf[MAX_FRAME_SIZE];
  size_t injected_frame_len;
  bool injected_frame_valid;

  void clearBuffers();
  void resetReceivedFrameHeader();
  void resetPendingReceivedFrameHeader();
  bool hasReceivedFrameHeader() const;
  bool hasPendingReceivedFrameHeader() const;
  void clearInjectedFrame();
  void makeMac(uint8_t mac[6]);
  void powerUpEthernet();
  void resetEthernet();
  bool startEthernet();
  void serviceEthernet();
  void serviceClient();
  void dropClient();
  void clearPendingClient();
  void promotePendingClientToActive();

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
