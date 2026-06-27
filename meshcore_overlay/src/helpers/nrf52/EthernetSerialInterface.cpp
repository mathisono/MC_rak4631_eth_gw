#include "EthernetSerialInterface.h"

#ifdef WITH_ETHERNET_TCP_API

#ifndef ETH_DEBUG_LOGGING
#define ETH_DEBUG_LOGGING 0
#endif

#if ETH_DEBUG_LOGGING && ARDUINO
#define ETH_DEBUG_PRINT(F, ...) Serial.printf("ETH: " F, ##__VA_ARGS__)
#define ETH_DEBUG_PRINTLN(F, ...) Serial.printf("ETH: " F "\n", ##__VA_ARGS__)
#else
#define ETH_DEBUG_PRINT(...) {}
#define ETH_DEBUG_PRINTLN(...) {}
#endif

EthernetSerialInterface::EthernetSerialInterface()
    : deviceConnected(false), ethernetReady(false), _isEnabled(false), _port(TCP_PORT),
      lastDhcpAttempt(0), lastMaintain(0), server(nullptr), client(EthernetClient()) {
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
  pinMode(PIN_3V3_EN, OUTPUT);
  digitalWrite(PIN_3V3_EN, HIGH);
  delay(100);
#endif
}

void EthernetSerialInterface::resetEthernet() {
#ifdef PIN_ETHERNET_RESET
  pinMode(PIN_ETHERNET_RESET, OUTPUT);
  digitalWrite(PIN_ETHERNET_RESET, LOW);
  delay(100);
  digitalWrite(PIN_ETHERNET_RESET, HIGH);
  delay(100);
#endif
}

bool EthernetSerialInterface::startEthernet() {
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
  ETH_SPI_PORT.begin();
  Ethernet.init(ETH_SPI_PORT, PIN_ETHERNET_SS);
#else
  SPI.begin();
  Ethernet.init(PIN_ETHERNET_SS);
#endif

  uint8_t mac[6];
  makeMac(mac);

  ETH_DEBUG_PRINTLN("starting DHCP on RAK13800/W5100S, CS=%d, port=%d", PIN_ETHERNET_SS, _port);
  int status = Ethernet.begin(mac);

  if (status == 0) {
    ethernetReady = false;
    lastDhcpAttempt = millis();
    ETH_DEBUG_PRINTLN("DHCP failed");
    return false;
  }

  ethernetReady = true;
  lastMaintain = millis();
  ETH_DEBUG_PRINTLN("IP %u.%u.%u.%u", Ethernet.localIP()[0], Ethernet.localIP()[1], Ethernet.localIP()[2], Ethernet.localIP()[3]);
  return true;
}

void EthernetSerialInterface::begin(int port) {
  _port = port;

  if (!startEthernet()) {
    // Leave server unset until DHCP succeeds during serviceEthernet().
    return;
  }

  if (server) {
    delete server;
  }
  server = new EthernetServer(_port);
  server->begin();
  ETH_DEBUG_PRINTLN("TCP companion server listening on %d", _port);
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
    ETH_DEBUG_PRINTLN("client disconnected");
  }
  deviceConnected = false;
  if (client) {
    client.stop();
  }
  resetReceivedFrameHeader();
}

void EthernetSerialInterface::serviceEthernet() {
  unsigned long now = millis();

  if (!ethernetReady) {
    if (now - lastDhcpAttempt >= ETH_DHCP_RETRY_MS) {
      if (startEthernet()) {
        if (!server) {
          server = new EthernetServer(_port);
        }
        server->begin();
        ETH_DEBUG_PRINTLN("TCP companion server listening on %d", _port);
      }
    }
    return;
  }

  if (now - lastMaintain >= ETH_MAINTAIN_MS) {
    Ethernet.maintain();
    lastMaintain = now;
  }

  if (Ethernet.hardwareStatus() == EthernetNoHardware || Ethernet.linkStatus() == LinkOFF) {
    ETH_DEBUG_PRINTLN("ethernet unavailable, resetting state");
    ethernetReady = false;
    dropClient();
    lastDhcpAttempt = now;
  }
}

void EthernetSerialInterface::serviceClient() {
  if (!server || !ethernetReady) return;

  EthernetClient newClient = server->available();
  if (newClient) {
    dropClient();
    client = newClient;
    deviceConnected = true;
    resetReceivedFrameHeader();
    ETH_DEBUG_PRINTLN("client connected");
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
  serviceClient();

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
