#include "EthernetSerialInterface.h"

#ifdef WITH_ETHERNET_TCP_API

#ifndef ETH_DEBUG_LOGGING
#define ETH_DEBUG_LOGGING 0
#endif

#ifndef ETH_CHECK_LINK_STATUS
#define ETH_CHECK_LINK_STATUS 0
#endif

#include <type_traits>
#include <utility>

#if ETH_DEBUG_LOGGING && ARDUINO
#define ETH_DEBUG_PRINT(F, ...) Serial.printf("ETH: " F, ##__VA_ARGS__)
#define ETH_DEBUG_PRINTLN(F, ...) Serial.printf("ETH: " F "\n", ##__VA_ARGS__)
#else
#define ETH_DEBUG_PRINT(...) {}
#define ETH_DEBUG_PRINTLN(...) {}
#endif

namespace {

template <typename T>
class has_hardware_status {
  template <typename U>
  static auto test(int) -> decltype(std::declval<U>().hardwareStatus(), std::true_type{});

  template <typename>
  static std::false_type test(...);

public:
  static constexpr bool value = decltype(test<T>(0))::value;
};

template <typename T>
class has_link_status {
  template <typename U>
  static auto test(int) -> decltype(std::declval<U>().linkStatus(), std::true_type{});

  template <typename>
  static std::false_type test(...);

public:
  static constexpr bool value = decltype(test<T>(0))::value;
};

template <typename EthernetT, bool CanCheck = has_hardware_status<EthernetT>::value && has_link_status<EthernetT>::value>
struct EthernetStatusLogger;

template <typename EthernetT>
struct EthernetStatusLogger<EthernetT, true> {
  static void logBeforeDhcp(EthernetT &ethernet) {
    ETH_DEBUG_PRINTLN("hardwareStatus=%d", (int)ethernet.hardwareStatus());
    ETH_DEBUG_PRINTLN("linkStatus=%d", (int)ethernet.linkStatus());
  }

  static void logFailure(EthernetT &ethernet) {
    auto hwStatus = ethernet.hardwareStatus();
    auto linkStatus = ethernet.linkStatus();
    if (hwStatus == EthernetNoHardware) {
      ETH_DEBUG_PRINTLN("EthernetNoHardware");
    } else if (linkStatus == LinkOFF) {
      ETH_DEBUG_PRINTLN("LinkOFF");
    } else {
      ETH_DEBUG_PRINTLN("unknown Ethernet error (hardware=%d link=%d)", (int)hwStatus, (int)linkStatus);
    }
  }

  static void logRunning(EthernetT &ethernet) {
    if (ethernet.hardwareStatus() == EthernetNoHardware || ethernet.linkStatus() == LinkOFF) {
      ETH_DEBUG_PRINTLN("EthernetNoHardware / LinkOFF while running, resetting state");
    }
  }

  static bool isDown(EthernetT &ethernet) {
    return ethernet.hardwareStatus() == EthernetNoHardware || ethernet.linkStatus() == LinkOFF;
  }
};

template <typename EthernetT>
struct EthernetStatusLogger<EthernetT, false> {
  static void logBeforeDhcp(EthernetT &) {
    ETH_DEBUG_PRINTLN("hardware/link status checks unavailable at compile time");
  }

  static void logFailure(EthernetT &) {
    ETH_DEBUG_PRINTLN("unknown Ethernet error");
  }

  static void logRunning(EthernetT &) {}

  static bool isDown(EthernetT &) {
    return false;
  }
};

} // namespace

EthernetSerialInterface::EthernetSerialInterface()
    : deviceConnected(false), ethernetReady(false), _isEnabled(false), _port(TCP_PORT),
      lastDhcpAttempt(0), lastMaintain(0), lastServiceLog(0), lastClientLog(0), lastDelayLog(0),
      server(nullptr), client(EthernetClient()) {
  send_queue_len = recv_queue_len = 0;
  received_frame_header.type = 0;
  received_frame_header.length = 0;
}

EthernetSerialInterface::~EthernetSerialInterface() {
  if (server) {
    delete server;
    server = nullptr;
  }
}

void EthernetSerialInterface::clearBuffers() {
  recv_queue_len = 0;
  send_queue_len = 0;
}

bool EthernetSerialInterface::hasReceivedFrameHeader() const {
  return received_frame_header.type != 0 && received_frame_header.length != 0;
}

void EthernetSerialInterface::resetReceivedFrameHeader() {
  received_frame_header.type = 0;
  received_frame_header.length = 0;
}

void EthernetSerialInterface::makeMac(uint8_t mac[6]) {
  // Locally administered, unicast MAC, deterministic from nRF52840 FICR.
  // This avoids hard-coding one MAC into every gateway.
  uint32_t addr0 = NRF_FICR->DEVICEADDR[0];
  uint32_t addr1 = NRF_FICR->DEVICEADDR[1];

  mac[0] = 0x02; // local/unicast
  mac[1] = 0x4D; // 'M'
  mac[2] = 0x43; // 'C'
  mac[3] = (addr1 >> 8) & 0xFF;
  mac[4] = addr0 & 0xFF;
  mac[5] = (addr0 >> 8) & 0xFF;
}

void EthernetSerialInterface::powerUpEthernet() {
#ifdef PIN_3V3_EN
  ETH_DEBUG_PRINTLN("PIN_3V3_EN set HIGH");
  pinMode(PIN_3V3_EN, OUTPUT);
  digitalWrite(PIN_3V3_EN, HIGH);
  delay(100);
#endif
}

void EthernetSerialInterface::resetEthernet() {
#ifdef PIN_ETHERNET_RESET
  ETH_DEBUG_PRINTLN("PIN_ETHERNET_RESET LOW/HIGH pulse");
  pinMode(PIN_ETHERNET_RESET, OUTPUT);
  digitalWrite(PIN_ETHERNET_RESET, LOW);
  delay(100);
  digitalWrite(PIN_ETHERNET_RESET, HIGH);
  delay(100);
#endif
}

bool EthernetSerialInterface::startEthernet() {
  ETH_DEBUG_PRINTLN("entering startEthernet()");
  powerUpEthernet();
  resetEthernet();

#ifdef ETH_SPI_PORT
  // On RAK4631 the actual SPI1 pins should come from variant.h by setting
  // SPI_INTERFACES_COUNT=2 and PIN_SPI1_* macros. Do not call non-portable
  // setter names here by default; different cores expose different APIs.
  // If a local core requires explicit pin assignment, define ETH_SPI_SET_PINS
  // and provide a SPIClass::setPins(sck, miso, mosi)-compatible core.
#ifdef ETH_SPI_SET_PINS
  ETH_SPI_PORT.setPins(ETH_SPI_SCK, ETH_SPI_MISO, ETH_SPI_MOSI);
#endif
  ETH_DEBUG_PRINTLN("ETH_SPI_PORT.begin()");
  ETH_SPI_PORT.begin();
  ETH_DEBUG_PRINTLN("Ethernet.init(ETH_SPI_PORT, PIN_ETHERNET_SS)");
  Ethernet.init(ETH_SPI_PORT, PIN_ETHERNET_SS);
#else
  ETH_DEBUG_PRINTLN("SPI.begin()");
  SPI.begin();
  ETH_DEBUG_PRINTLN("Ethernet.init(PIN_ETHERNET_SS)");
  Ethernet.init(PIN_ETHERNET_SS);
#endif

  uint8_t mac[6];
  makeMac(mac);
  ETH_DEBUG_PRINTLN("MAC %02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

#if ETH_USE_STATIC_IP
  ETH_DEBUG_PRINTLN("static IP mode");
  IPAddress ip(ETH_STATIC_IP_A, ETH_STATIC_IP_B, ETH_STATIC_IP_C, ETH_STATIC_IP_D);
  IPAddress dns(ETH_DNS_A, ETH_DNS_B, ETH_DNS_C, ETH_DNS_D);
  IPAddress gateway(ETH_GATEWAY_A, ETH_GATEWAY_B, ETH_GATEWAY_C, ETH_GATEWAY_D);
  IPAddress subnet(ETH_SUBNET_A, ETH_SUBNET_B, ETH_SUBNET_C, ETH_SUBNET_D);
  ETH_DEBUG_PRINTLN("Ethernet.begin(mac, ip, dns, gateway, subnet) start");
  Ethernet.begin(mac, ip, dns, gateway, subnet);
  int status = 1;
  ETH_DEBUG_PRINTLN("Ethernet.begin(mac, ip, dns, gateway, subnet) return status=%d", status);
#if ETH_PING_ONLY
  ETH_DEBUG_PRINTLN("ping-only mode");
#endif
  ethernetReady = true;
  ethernetStarted = true;
  if (!server) {
    ETH_DEBUG_PRINTLN("server begin on %d", _port);
    server = new EthernetServer(_port);
  } else {
    ETH_DEBUG_PRINTLN("server begin on %d", _port);
  }
  server->begin();
  ETH_DEBUG_PRINTLN("TCP companion server listening on %d", _port);
  ETH_DEBUG_PRINTLN("static IP %u.%u.%u.%u", Ethernet.localIP()[0], Ethernet.localIP()[1], Ethernet.localIP()[2], Ethernet.localIP()[3]);
  ETH_DEBUG_PRINTLN("localIP %u.%u.%u.%u", Ethernet.localIP()[0], Ethernet.localIP()[1], Ethernet.localIP()[2], Ethernet.localIP()[3]);
  ETH_DEBUG_PRINTLN("subnet %u.%u.%u.%u", Ethernet.subnetMask()[0], Ethernet.subnetMask()[1], Ethernet.subnetMask()[2], Ethernet.subnetMask()[3]);
  ETH_DEBUG_PRINTLN("gateway %u.%u.%u.%u", Ethernet.gatewayIP()[0], Ethernet.gatewayIP()[1], Ethernet.gatewayIP()[2], Ethernet.gatewayIP()[3]);
  ETH_DEBUG_PRINTLN("dns %u.%u.%u.%u", Ethernet.dnsServerIP()[0], Ethernet.dnsServerIP()[1], Ethernet.dnsServerIP()[2], Ethernet.dnsServerIP()[3]);
  lastClientLog = millis();
  return true;
#elif ETH_DISABLE_DHCP
  ETH_DEBUG_PRINTLN("DHCP disabled");
  return false;
#else
  ETH_DEBUG_PRINTLN("DHCP mode");
  ETH_DEBUG_PRINTLN("Ethernet.setRetransmissionTimeout(200)");
  Ethernet.setRetransmissionTimeout(200);
  ETH_DEBUG_PRINTLN("Ethernet.setRetransmissionCount(2)");
  Ethernet.setRetransmissionCount(2);
  ETH_DEBUG_PRINTLN("Ethernet.begin(mac) start");
  int status = Ethernet.begin(mac);
  ETH_DEBUG_PRINTLN("Ethernet.begin(mac) return status=%d", status);
#endif

  if (status == 0) {
    ethernetReady = false;
    lastDhcpAttempt = millis();
#if !ETH_PING_ONLY
    ETH_DEBUG_PRINTLN("Ethernet.begin failed");
    ETH_DEBUG_PRINTLN("hardwareStatus=%d", (int)Ethernet.hardwareStatus());
#if ETH_CHECK_LINK_STATUS
    ETH_DEBUG_PRINTLN("linkStatus=%d", (int)Ethernet.linkStatus());
#endif
    EthernetStatusLogger<decltype(Ethernet)>::logFailure(Ethernet);
#else
    ETH_DEBUG_PRINTLN("Ethernet.begin failed in ping-only mode");
#endif
    return false;
  }

  ethernetReady = true;
  lastMaintain = millis();
  ETH_DEBUG_PRINTLN("localIP %u.%u.%u.%u", Ethernet.localIP()[0], Ethernet.localIP()[1], Ethernet.localIP()[2], Ethernet.localIP()[3]);
  ETH_DEBUG_PRINTLN("subnet %u.%u.%u.%u", Ethernet.subnetMask()[0], Ethernet.subnetMask()[1], Ethernet.subnetMask()[2], Ethernet.subnetMask()[3]);
  ETH_DEBUG_PRINTLN("gateway %u.%u.%u.%u", Ethernet.gatewayIP()[0], Ethernet.gatewayIP()[1], Ethernet.gatewayIP()[2], Ethernet.gatewayIP()[3]);
  ETH_DEBUG_PRINTLN("dns %u.%u.%u.%u", Ethernet.dnsServerIP()[0], Ethernet.dnsServerIP()[1], Ethernet.dnsServerIP()[2], Ethernet.dnsServerIP()[3]);
  return true;
}

void EthernetSerialInterface::begin(int port) {
  ETH_DEBUG_PRINTLN("entering EthernetSerialInterface::begin()");
  _port = port;
  beginMillis = millis();
  lastDhcpAttempt = beginMillis;
  lastMaintain = beginMillis;
  lastServiceLog = beginMillis;
  lastClientLog = beginMillis;
  lastDelayLog = beginMillis;
  ethernetReady = false;
  ethernetStarted = false;

#if ETH_START_DELAY_MS == 0
  if (!startEthernet()) {
    return;
  }
#else
  ETH_DEBUG_PRINTLN("Ethernet startup delayed by %lu ms", (unsigned long)ETH_START_DELAY_MS);
#endif
}

void EthernetSerialInterface::enable() {
  if (_isEnabled) return;
  _isEnabled = true;
  clearBuffers();
  resetReceivedFrameHeader();
}

void EthernetSerialInterface::disable() {
  _isEnabled = false;
  dropClient();
}

void EthernetSerialInterface::dropClient() {
  if (deviceConnected) {
    ETH_DEBUG_PRINTLN("TCP client disconnected");
  }
  deviceConnected = false;
  if (client) {
    client.stop();
  }
  resetReceivedFrameHeader();
}

void EthernetSerialInterface::serviceEthernet() {
  unsigned long now = millis();

  if (lastServiceLog == 0 || now - lastServiceLog >= 5000UL) {
    lastServiceLog = now;
    ETH_DEBUG_PRINTLN("entering serviceEthernet()");
  }

  if (ethernetReady) {
    return;
  }

#if ETH_START_DELAY_MS > 0
  if (!ethernetStarted) {
    unsigned long elapsed = now - beginMillis;
    if (elapsed < ETH_START_DELAY_MS) {
      if (now - lastDelayLog >= 1000UL) {
        lastDelayLog = now;
        ETH_DEBUG_PRINTLN("delay countdown: %lu ms remaining",
                          (unsigned long)(ETH_START_DELAY_MS - elapsed));
      }
      return;
    }
    ETH_DEBUG_PRINTLN("delay expired");
    ethernetStarted = true;
  }
#endif
  if (now - lastDhcpAttempt < ETH_DHCP_RETRY_MS) {
    return;
  }

  lastDhcpAttempt = now;
  if (!startEthernet()) {
    ETH_DEBUG_PRINTLN("startEthernet() failed; will retry");
  }
}

void EthernetSerialInterface::serviceClient() {
#if ETH_TCP_SERVER_DEBUG
  unsigned long now = millis();
  if (lastClientLog == 0 || now - lastClientLog >= 5000UL) {
    lastClientLog = now;
    ETH_DEBUG_PRINTLN("serviceClient server=%d ready=%d", server ? 1 : 0, ethernetReady ? 1 : 0);
  }
#endif

  if (!server || !ethernetReady) return;

  EthernetClient newClient = server->available();
  if (newClient) {
    if (deviceConnected && client) {
      client.stop();
    }
    client = newClient;
    deviceConnected = true;
    resetReceivedFrameHeader();
    ETH_DEBUG_PRINTLN("TCP client connected");
  }

  if (deviceConnected && !client.connected()) {
    dropClient();
  }
}

size_t EthernetSerialInterface::writeFrame(const uint8_t src[], size_t len) {
  if (!_isEnabled || len == 0) return 0;

  if (len > MAX_FRAME_SIZE) {
    ETH_DEBUG_PRINTLN("writeFrame too big, len=%d", (int)len);
    return 0;
  }

  if (!deviceConnected) return 0;

  if (send_queue_len >= ETH_FRAME_QUEUE_SIZE) {
    ETH_DEBUG_PRINTLN("send queue full");
    return 0;
  }

  send_queue[send_queue_len].len = len;
  memcpy(send_queue[send_queue_len].buf, src, len);
  send_queue_len++;
  return len;
}

bool EthernetSerialInterface::isWriteBusy() const {
  return false;
}

bool EthernetSerialInterface::isConnected() const {
  return deviceConnected && client.connected();
}

size_t EthernetSerialInterface::checkRecvFrame(uint8_t dest[]) {
  if (!_isEnabled) return 0;

  serviceEthernet();
  ETH_DEBUG_PRINTLN("after serviceEthernet");
  serviceClient();
  ETH_DEBUG_PRINTLN("after serviceClient");

  if (!deviceConnected) return 0;

  // Flush queued outbound frames first so app responses do not stall behind RX parsing.
  if (send_queue_len > 0) {
    const uint16_t len = send_queue[0].len;
    uint8_t hdr[3];
    hdr[0] = '>';
    hdr[1] = len & 0xFF;
    hdr[2] = (len >> 8) & 0xFF;

    client.write(hdr, sizeof(hdr));
    client.write(send_queue[0].buf, len);

    send_queue_len--;
    for (int i = 0; i < send_queue_len; i++) {
      send_queue[i] = send_queue[i + 1];
    }
    return 0;
  }

  if (!hasReceivedFrameHeader()) {
    const int headerLen = 3;
    if (client.available() >= headerLen) {
      client.readBytes(&received_frame_header.type, 1);
      client.readBytes((uint8_t *)&received_frame_header.length, 2);
    }
  }

  if (!hasReceivedFrameHeader()) {
    return 0;
  }

  const int frameType = received_frame_header.type;
  int frameLength = received_frame_header.length;

  if (frameType != '<') {
    ETH_DEBUG_PRINTLN("dropping unexpected frame type=0x%x len=%d", frameType, frameLength);
    while (frameLength > 0 && client.available()) {
      client.read();
      frameLength--;
    }
    if (frameLength == 0) resetReceivedFrameHeader();
    return 0;
  }

  if (frameLength > MAX_FRAME_SIZE) {
    ETH_DEBUG_PRINTLN("dropping oversize frame len=%d max=%d", frameLength, MAX_FRAME_SIZE);
    while (frameLength > 0 && client.available()) {
      client.read();
      frameLength--;
    }
    if (frameLength == 0) resetReceivedFrameHeader();
    return 0;
  }

  if (client.available() < frameLength) {
    return 0;
  }

  client.readBytes(dest, frameLength);
  resetReceivedFrameHeader();
  return frameLength;
}

#endif // WITH_ETHERNET_TCP_API
