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
    : deviceConnected(false), ethernetReady(false), ethernetStarted(false), _isEnabled(false),
      _port(TCP_PORT), beginMillis(0), lastDhcpAttempt(0), lastMaintain(0), lastDelayLog(0),
      server(nullptr), client(EthernetClient()), pendingClient(EthernetClient()),
      injected_frame_len(0), injected_frame_valid(false), pending_frame_len(0),
      pending_frame_pos(0), pending_state(PENDING_WAIT_MAGIC), pending_last_progress_ms(0) {
  send_queue_len = recv_queue_len = 0;
  received_frame_header.type = 0;
  received_frame_header.length = 0;
}

EthernetSerialInterface::~EthernetSerialInterface() {
  dropClient();
  resetPendingClient();
  if (server) {
    delete server;
    server = nullptr;
  }
}

void EthernetSerialInterface::clearBuffers() {
  recv_queue_len = 0;
  send_queue_len = 0;
  resetInjectedFrame();
}

bool EthernetSerialInterface::hasReceivedFrameHeader() const {
  return received_frame_header.type != 0 && received_frame_header.length != 0;
}

void EthernetSerialInterface::resetReceivedFrameHeader() {
  received_frame_header.type = 0;
  received_frame_header.length = 0;
}

void EthernetSerialInterface::resetInjectedFrame() {
  injected_frame_len = 0;
  injected_frame_valid = false;
}

void EthernetSerialInterface::resetPendingParserOnly() {
  pending_frame_len = 0;
  pending_frame_pos = 0;
  pending_state = PENDING_WAIT_MAGIC;
  pending_last_progress_ms = millis();
}

void EthernetSerialInterface::resetPendingClient() {
  if (pendingClient) {
    pendingClient.stop();
  }
  pendingClient = EthernetClient();
  resetPendingParserOnly();
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
  ETH_DEBUG_PRINTLN("PIN_3V3_EN set HIGH");
#endif
}

void EthernetSerialInterface::resetEthernet() {
#ifdef PIN_ETHERNET_RESET
  pinMode(PIN_ETHERNET_RESET, OUTPUT);
  digitalWrite(PIN_ETHERNET_RESET, LOW);
  delay(100);
  digitalWrite(PIN_ETHERNET_RESET, HIGH);
  delay(100);
  ETH_DEBUG_PRINTLN("PIN_ETHERNET_RESET LOW/HIGH pulse");
#endif
}

void EthernetSerialInterface::ensureServerStarted() {
  if (!server) {
    server = new EthernetServer(_port);
  }
  server->begin();
  ETH_DEBUG_PRINTLN("server begin on %d", _port);
  ETH_DEBUG_PRINTLN("TCP companion server listening on %d", _port);
}

bool EthernetSerialInterface::startEthernet() {
  ETH_DEBUG_PRINTLN("entering startEthernet()");
  powerUpEthernet();
  resetEthernet();

#ifdef ETH_SPI_PORT
#ifdef ETH_SPI_SET_PINS
  ETH_SPI_PORT.setPins(ETH_SPI_SCK, ETH_SPI_MISO, ETH_SPI_MOSI);
#endif
  ETH_DEBUG_PRINTLN("ETH_SPI_PORT.begin()");
  ETH_SPI_PORT.begin();
  ETH_DEBUG_PRINTLN("Ethernet.init(ETH_SPI_PORT, PIN_ETHERNET_SS)");
  Ethernet.init(ETH_SPI_PORT, PIN_ETHERNET_SS);
#else
  SPI.begin();
  Ethernet.init(PIN_ETHERNET_SS);
#endif

  uint8_t mac[6];
  makeMac(mac);
  ETH_DEBUG_PRINTLN("MAC %02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

#if ETH_USE_STATIC_IP
  IPAddress ip(ETH_IP_A, ETH_IP_B, ETH_IP_C, ETH_IP_D);
  IPAddress dns(ETH_DNS_A, ETH_DNS_B, ETH_DNS_C, ETH_DNS_D);
  IPAddress gateway(ETH_GATEWAY_A, ETH_GATEWAY_B, ETH_GATEWAY_C, ETH_GATEWAY_D);
  IPAddress subnet(ETH_SUBNET_A, ETH_SUBNET_B, ETH_SUBNET_C, ETH_SUBNET_D);

  ETH_DEBUG_PRINTLN("static IP mode");
  ETH_DEBUG_PRINTLN("Ethernet.begin(mac, ip, dns, gateway, subnet) start");
  Ethernet.begin(mac, ip, dns, gateway, subnet);
  ETH_DEBUG_PRINTLN("Ethernet.begin(mac, ip, dns, gateway, subnet) return status=1");
#else
  ETH_DEBUG_PRINTLN("starting DHCP on RAK13800/W5100S, CS=%d, port=%d", PIN_ETHERNET_SS, _port);
  int status = Ethernet.begin(mac);
  if (status == 0) {
    ethernetReady = false;
    lastDhcpAttempt = millis();
    ETH_DEBUG_PRINTLN("DHCP failed");
    return false;
  }
#endif

  ethernetReady = true;
  lastMaintain = millis();
  ensureServerStarted();

#if ETH_USE_STATIC_IP
  ETH_DEBUG_PRINTLN("static IP %u.%u.%u.%u", ETH_IP_A, ETH_IP_B, ETH_IP_C, ETH_IP_D);
#endif
  ETH_DEBUG_PRINTLN("localIP %u.%u.%u.%u", Ethernet.localIP()[0], Ethernet.localIP()[1], Ethernet.localIP()[2], Ethernet.localIP()[3]);
  ETH_DEBUG_PRINTLN("subnet %u.%u.%u.%u", Ethernet.subnetMask()[0], Ethernet.subnetMask()[1], Ethernet.subnetMask()[2], Ethernet.subnetMask()[3]);
  ETH_DEBUG_PRINTLN("gateway %u.%u.%u.%u", Ethernet.gatewayIP()[0], Ethernet.gatewayIP()[1], Ethernet.gatewayIP()[2], Ethernet.gatewayIP()[3]);
  ETH_DEBUG_PRINTLN("dns %u.%u.%u.%u", Ethernet.dnsServerIP()[0], Ethernet.dnsServerIP()[1], Ethernet.dnsServerIP()[2], Ethernet.dnsServerIP()[3]);
  return true;
}

void EthernetSerialInterface::begin(int port) {
  _port = port;
  beginMillis = millis();
  lastDelayLog = beginMillis;
  ethernetStarted = false;
  ethernetReady = false;
  resetReceivedFrameHeader();
  resetPendingParserOnly();
  resetInjectedFrame();
}

void EthernetSerialInterface::enable() {
  if (_isEnabled) return;
  _isEnabled = true;
  clearBuffers();
  resetReceivedFrameHeader();
  resetPendingParserOnly();
}

void EthernetSerialInterface::disable() {
  _isEnabled = false;
  dropClient();
  resetPendingClient();
}

void EthernetSerialInterface::dropClient() {
  if (deviceConnected) {
    ETH_DEBUG_PRINTLN("TCP client disconnected");
  }
  deviceConnected = false;
  if (client) {
    client.stop();
    delay(1);
  }
  client = EthernetClient();
  resetReceivedFrameHeader();
}

bool EthernetSerialInterface::promotePendingClientWithoutInjectedFrame() {
  if (!pendingClient || !pendingClient.connected()) return false;

  if (client) {
    client.stop();
    delay(1);
  }

  client = pendingClient;
  pendingClient = EthernetClient();
  deviceConnected = true;
  resetReceivedFrameHeader();
  resetPendingParserOnly();
  ETH_DEBUG_PRINTLN("promoted pending TCP client to active");
  return true;
}

bool EthernetSerialInterface::promotePendingClientWithInjectedFrame() {
  if (!pendingClient || !pendingClient.connected()) return false;
  if (pending_frame_len == 0 || pending_frame_len > MAX_FRAME_SIZE) return false;

  memcpy(injected_frame, pending_frame_buf, pending_frame_len);
  injected_frame_len = pending_frame_len;
  injected_frame_valid = true;

  const uint8_t cmd = injected_frame[0];
  ETH_DEBUG_PRINTLN("pending client sent startup command, switching active client cmd=%u len=%u", cmd, injected_frame_len);

  if (client) {
    client.stop();
    delay(1);
  }

  client = pendingClient;
  pendingClient = EthernetClient();
  deviceConnected = true;
  resetReceivedFrameHeader();
  resetPendingParserOnly();
  ETH_DEBUG_PRINTLN("TCP client connected");
  return true;
}

bool EthernetSerialInterface::servicePendingClient() {
  if (!pendingClient) return false;

  const unsigned long now = millis();
  if (!pendingClient.connected()) {
    ETH_DEBUG_PRINTLN("pending TCP client disconnected");
    resetPendingClient();
    return false;
  }

  const unsigned long timeout = (pending_state == PENDING_WAIT_MAGIC) ? ETH_PENDING_IDLE_TIMEOUT_MS : ETH_PENDING_FRAME_TIMEOUT_MS;
  if (now - pending_last_progress_ms > timeout) {
    ETH_DEBUG_PRINTLN("pending TCP client frame timeout");
    resetPendingClient();
    return false;
  }

  while (pendingClient.available() > 0) {
    const int b = pendingClient.read();
    if (b < 0) break;
    pending_last_progress_ms = millis();

    switch (pending_state) {
    case PENDING_WAIT_MAGIC:
      if ((uint8_t)b != '<') {
        ETH_DEBUG_PRINTLN("pending bad magic=0x%x", b);
        resetPendingClient();
        return false;
      }
      pending_state = PENDING_READ_LEN_0;
      break;

    case PENDING_READ_LEN_0:
      pending_frame_len = (uint8_t)b;
      pending_state = PENDING_READ_LEN_1;
      break;

    case PENDING_READ_LEN_1:
      pending_frame_len |= ((uint16_t)((uint8_t)b) << 8);
      if (pending_frame_len == 0 || pending_frame_len > MAX_FRAME_SIZE) {
        ETH_DEBUG_PRINTLN("pending invalid frame len=%u", pending_frame_len);
        resetPendingClient();
        return false;
      }
      pending_frame_pos = 0;
      pending_state = PENDING_READ_PAYLOAD;
      break;

    case PENDING_READ_PAYLOAD:
      pending_frame_buf[pending_frame_pos++] = (uint8_t)b;
      if (pending_frame_pos >= pending_frame_len) {
        const uint8_t cmd = pending_frame_buf[0];
        if (cmd == 1 || cmd == 22) {
          return promotePendingClientWithInjectedFrame();
        }

        ETH_DEBUG_PRINTLN("pending non-startup cmd=%u, closing pending client", cmd);
        resetPendingClient();
        return false;
      }
      break;
    }
  }

  return false;
}

void EthernetSerialInterface::serviceEthernet() {
  const unsigned long now = millis();

  if (!ethernetStarted) {
    if (now - beginMillis < ETH_START_DELAY_MS) {
#if ETH_DEBUG_LOGGING
      if (now - lastDelayLog >= 1000UL) {
        const unsigned long remaining = ETH_START_DELAY_MS - (now - beginMillis);
        ETH_DEBUG_PRINTLN("delay countdown: %lu ms remaining", remaining);
        lastDelayLog = now;
      }
#endif
      return;
    }

    ETH_DEBUG_PRINTLN("delay expired");
    ethernetStarted = true;
    startEthernet();
    return;
  }

  if (!ethernetReady) {
#if !ETH_USE_STATIC_IP
    if (now - lastDhcpAttempt >= ETH_DHCP_RETRY_MS) {
      startEthernet();
    }
#endif
    return;
  }

#if !ETH_DISABLE_MAINTAIN
  if (now - lastMaintain >= ETH_MAINTAIN_MS) {
    Ethernet.maintain();
    lastMaintain = now;
  }
#endif

#if ETH_CHECK_LINK_STATUS
  if (Ethernet.hardwareStatus() == EthernetNoHardware || Ethernet.linkStatus() == LinkOFF) {
    ETH_DEBUG_PRINTLN("ethernet unavailable, resetting state");
    ethernetReady = false;
    dropClient();
    resetPendingClient();
    lastDhcpAttempt = now;
  }
#endif
}

void EthernetSerialInterface::serviceClient() {
  if (!server || !ethernetReady) return;

  EthernetClient newClient = server->available();
  if (newClient) {
    if (!deviceConnected || !client.connected()) {
      if (deviceConnected && !client.connected()) {
        dropClient();
      }
      client = newClient;
      deviceConnected = true;
      resetReceivedFrameHeader();
      ETH_DEBUG_PRINTLN("TCP client connected");
    } else if (!pendingClient || !pendingClient.connected()) {
      pendingClient = newClient;
      resetPendingParserOnly();
      ETH_DEBUG_PRINTLN("TCP pending client connected");
    } else {
      ETH_DEBUG_PRINTLN("rejecting extra TCP client, pending already exists");
      newClient.stop();
    }
  }

  servicePendingClient();

  if (deviceConnected && !client.connected()) {
    ETH_DEBUG_PRINTLN("active TCP client disconnected");
    deviceConnected = false;
    client = EthernetClient();
    resetReceivedFrameHeader();
    if (pendingClient && pendingClient.connected()) {
      promotePendingClientWithoutInjectedFrame();
    }
  }
}

size_t EthernetSerialInterface::writeFrame(const uint8_t src[], size_t len) {
  if (!_isEnabled || len == 0) return 0;

  if (len > MAX_FRAME_SIZE) {
    ETH_DEBUG_PRINTLN("writeFrame too big, len=%d", (int)len);
    return 0;
  }

  if (!deviceConnected || !client.connected()) return 0;

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

  if (injected_frame_valid) {
    const uint16_t len = injected_frame_len;
    memcpy(dest, injected_frame, len);
    injected_frame_valid = false;
    injected_frame_len = 0;
    ETH_DEBUG_PRINTLN("delivering injected pending frame len=%u cmd=%u", len, dest[0]);
    return len;
  }

  if (!deviceConnected || !client.connected()) return 0;

  // Flush queued outbound frames first so app responses do not stall behind RX parsing.
  if (send_queue_len > 0) {
    const uint16_t len = send_queue[0].len;
    uint8_t hdr[3];
    hdr[0] = '>';
    hdr[1] = len & 0xFF;
    hdr[2] = (len >> 8) & 0xFF;

    client.write(hdr, sizeof(hdr));
    client.write(send_queue[0].buf, len);
    ETH_DEBUG_PRINTLN("TCP tx frame len=%u hdr=%u", len, send_queue[0].buf[0]);

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

  if (frameLength <= 0 || frameLength > MAX_FRAME_SIZE) {
    ETH_DEBUG_PRINTLN("dropping invalid frame len=%d max=%d", frameLength, MAX_FRAME_SIZE);
    while (frameLength > 0 && client.available()) {
      client.read();
      frameLength--;
    }
    resetReceivedFrameHeader();
    return 0;
  }

  if (client.available() < frameLength) {
    return 0;
  }

  client.readBytes(dest, frameLength);
  resetReceivedFrameHeader();
  ETH_DEBUG_PRINTLN("TCP rx frame len=%d cmd=%u", frameLength, dest[0]);
  return frameLength;
}

#endif // WITH_ETHERNET_TCP_API
