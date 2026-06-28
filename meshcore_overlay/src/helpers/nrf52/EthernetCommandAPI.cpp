#include "EthernetCommandAPI.h"

#ifdef WITH_ETHERNET_COMMAND_API

#ifndef ETH_DEBUG_LOGGING
#define ETH_DEBUG_LOGGING 0
#endif

#if ETH_DEBUG_LOGGING && ARDUINO
#define ETH_DEBUG_PRINTLN(F, ...) Serial.printf("ETHAPI: " F "\n", ##__VA_ARGS__)
#else
#define ETH_DEBUG_PRINTLN(...) {}
#endif

EthernetCommandAPI::EthernetCommandAPI()
    : ethernetReady(false), clientConnected(false), _port(ETH_API_PORT),
      lastDhcpAttempt(0), lastMaintain(0), server(nullptr), client(EthernetClient()),
      commandLen(0) {
  command[0] = 0;
}

EthernetCommandAPI::~EthernetCommandAPI() {
  if (server) {
    delete server;
    server = nullptr;
  }
}

void EthernetCommandAPI::makeMac(uint8_t mac[6]) {
  uint32_t addr0 = NRF_FICR->DEVICEADDR[0];
  uint32_t addr1 = NRF_FICR->DEVICEADDR[1];

  mac[0] = 0x02; // local/unicast
  mac[1] = 0x4D; // M
  mac[2] = 0x43; // C
  mac[3] = 0x52; // R = repeater
  mac[4] = addr0 & 0xFF;
  mac[5] = (addr1 >> 8) & 0xFF;
}

void EthernetCommandAPI::powerUpEthernet() {
#ifdef PIN_3V3_EN
  pinMode(PIN_3V3_EN, OUTPUT);
  digitalWrite(PIN_3V3_EN, HIGH);
  delay(100);
#endif
}

void EthernetCommandAPI::resetEthernet() {
#ifdef PIN_ETHERNET_RESET
  pinMode(PIN_ETHERNET_RESET, OUTPUT);
  digitalWrite(PIN_ETHERNET_RESET, LOW);
  delay(100);
  digitalWrite(PIN_ETHERNET_RESET, HIGH);
  delay(100);
#endif
}

bool EthernetCommandAPI::startEthernet() {
  powerUpEthernet();
  resetEthernet();

#ifdef ETH_SPI_PORT
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

  ETH_DEBUG_PRINTLN("starting DHCP, CS=%d, port=%d", PIN_ETHERNET_SS, _port);
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

bool EthernetCommandAPI::begin(uint16_t port) {
  _port = port;

  if (!startEthernet()) {
    return false;
  }

  if (server) {
    delete server;
  }
  server = new EthernetServer(_port);
  server->begin();
  ETH_DEBUG_PRINTLN("command API listening on %d", _port);
  return true;
}

void EthernetCommandAPI::dropClient() {
  clientConnected = false;
  commandLen = 0;
  command[0] = 0;
  if (client) {
    client.stop();
  }
}

void EthernetCommandAPI::serviceEthernet() {
  unsigned long now = millis();

  if (!ethernetReady) {
    if (now - lastDhcpAttempt >= ETH_DHCP_RETRY_MS) {
      if (startEthernet()) {
        if (!server) {
          server = new EthernetServer(_port);
        }
        server->begin();
        ETH_DEBUG_PRINTLN("command API listening on %d", _port);
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

void EthernetCommandAPI::serviceClient() {
  if (!server || !ethernetReady) return;

  EthernetClient newClient = server->available();
  if (newClient) {
    dropClient();
    client = newClient;
    clientConnected = true;
    commandLen = 0;
    command[0] = 0;
    writeLine("MeshCore repeater Ethernet API ready");
    ETH_DEBUG_PRINTLN("client connected");
  }

  if (clientConnected && !client.connected()) {
    dropClient();
  }
}

void EthernetCommandAPI::loop() {
  serviceEthernet();
  serviceClient();
}

bool EthernetCommandAPI::isConnected() const {
  return clientConnected && client.connected();
}

bool EthernetCommandAPI::readCommand(char* dest, size_t destSize) {
  if (!dest || destSize == 0) return false;
  loop();
  if (!isConnected()) return false;

  while (client.available()) {
    char c = (char)client.read();

    if (c == '\r' || c == '\n') {
      if (commandLen == 0) {
        continue;
      }
      command[commandLen] = 0;
      strlcpy(dest, command, destSize);
      commandLen = 0;
      command[0] = 0;
      return true;
    }

    if (commandLen < sizeof(command) - 1) {
      command[commandLen++] = c;
      command[commandLen] = 0;
    } else {
      // Overflow: clear partial command and tell the client.
      commandLen = 0;
      command[0] = 0;
      writeLine("ERR command too long");
    }
  }

  return false;
}

void EthernetCommandAPI::writeLine(const char* line) {
  if (!isConnected()) return;
  if (line) {
    client.print(line);
  }
  client.print("\r\n");
}

void EthernetCommandAPI::writeReply(const char* reply) {
  if (!isConnected()) return;
  if (reply && reply[0]) {
    writeLine(reply);
  } else {
    writeLine("OK");
  }
}

#endif // WITH_ETHERNET_COMMAND_API
