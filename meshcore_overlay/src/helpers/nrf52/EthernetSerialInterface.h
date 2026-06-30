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

#ifndef ETH_PENDING_FRAME_TIMEOUT_MS
#define ETH_PENDING_FRAME_TIMEOUT_MS 3000UL
#endif

#ifndef ETH_PENDING_IDLE_TIMEOUT_MS
#define ETH_PENDING_IDLE_TIMEOUT_MS 5000UL
#endif

#ifndef ETH_USE_STATIC_IP
#define ETH_USE_STATIC_IP 0
#endif

#ifndef ETH_IP_A
#define ETH_IP_A 192
#endif
#ifndef ETH_IP_B
#define ETH_IP_B 168
#endif
#ifndef ETH_IP_C
#define ETH_IP_C 3
#endif
#ifndef ETH_IP_D
#define ETH_IP_D 55
#endif

#ifndef ETH_GATEWAY_A
#define ETH_GATEWAY_A 192
#endif
#ifndef ETH_GATEWAY_B
#define ETH_GATEWAY_B 168
#endif
#ifndef ETH_GATEWAY_C
#define ETH_GATEWAY_C 3
#endif
#ifndef ETH_GATEWAY_D
#define ETH_GATEWAY_D 3
#endif

#ifndef ETH_DNS_A
#define ETH_DNS_A 192
#endif
#ifndef ETH_DNS_B
#define ETH_DNS_B 168
#endif
#ifndef ETH_DNS_C
#define ETH_DNS_C 3
#endif
#ifndef ETH_DNS_D
#define ETH_DNS_D 3
#endif

#ifndef ETH_SUBNET_A
#define ETH_SUBNET_A 255
#endif
#ifndef ETH_SUBNET_B
#define ETH_SUBNET_B 255
#endif
#ifndef ETH_SUBNET_C
#define ETH_SUBNET_C 255
#endif
#ifndef ETH_SUBNET_D
#define ETH_SUBNET_D 0
#endif

#ifndef ETH_DISABLE_MAINTAIN
#define ETH_DISABLE_MAINTAIN 0
#endif

#ifndef ETH_CHECK_LINK_STATUS
#define ETH_CHECK_LINK_STATUS 1
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

class EthernetSerialInterface : public BaseSerialInterface {
  enum PendingRxState : uint8_t {
    PENDING_WAIT_MAGIC = 0,
    PENDING_READ_LEN_0,
    PENDING_READ_LEN_1,
    PENDING_READ_PAYLOAD
  };

  bool deviceConnected;
  bool ethernetReady;
  bool ethernetStarted;
  bool _isEnabled;
  uint16_t _port;
  unsigned long beginMillis;
  unsigned long lastDhcpAttempt;
  unsigned long lastMaintain;
  unsigned long lastDelayLog;

  EthernetServer *server;
  mutable EthernetClient client;
  mutable EthernetClient pendingClient;
  bool pendingClientValid;
  bool pendingClientAliasedToActive;

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

  uint8_t injected_frame[MAX_FRAME_SIZE];
  uint16_t injected_frame_len;
  bool injected_frame_valid;

  uint8_t pending_frame_buf[MAX_FRAME_SIZE];
  uint16_t pending_frame_len;
  uint16_t pending_frame_pos;
  PendingRxState pending_state;
  unsigned long pending_last_progress_ms;

  void clearBuffers();
  void resetReceivedFrameHeader();
  bool hasReceivedFrameHeader() const;
  void resetInjectedFrame();
  void resetPendingParserOnly();
  void resetPendingClient();
  bool servicePendingClient();
  bool promotePendingClientWithInjectedFrame();
  bool promotePendingClientWithoutInjectedFrame();
  void makeMac(uint8_t mac[6]);
  void powerUpEthernet();
  void resetEthernet();
  bool startEthernet();
  void serviceEthernet();
  void serviceClient();
  void dropClient();
  void ensureServerStarted();

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
