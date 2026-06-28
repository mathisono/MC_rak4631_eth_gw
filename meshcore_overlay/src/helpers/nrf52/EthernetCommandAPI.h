#pragma once

#include <Arduino.h>

#ifdef WITH_ETHERNET_COMMAND_API

#include <SPI.h>
#include <RAK13800_W5100S.h>

#ifndef ETH_API_PORT
#define ETH_API_PORT 4403
#endif

#ifndef ETH_DHCP_RETRY_MS
#define ETH_DHCP_RETRY_MS 5000UL
#endif

#ifndef ETH_MAINTAIN_MS
#define ETH_MAINTAIN_MS 1000UL
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

#ifndef ETH_COMMAND_MAX_LEN
#define ETH_COMMAND_MAX_LEN 160
#endif

/**
 * Line-oriented Ethernet command API for MeshCore repeater firmware.
 *
 * This is intentionally NOT the Companion app binary API.
 * It exposes the repeater's existing CommonCLI command path over TCP:
 *
 *   TCP client -> text command + \r or \n
 *   firmware   -> text reply + \r\n
 *
 * Example commands:
 *   get name
 *   get freq
 *   get tx
 *   set name MyRepeater
 *   advert
 */
class EthernetCommandAPI {
  bool ethernetReady;
  bool clientConnected;
  uint16_t _port;
  unsigned long lastDhcpAttempt;
  unsigned long lastMaintain;

  EthernetServer* server;
  EthernetClient client;

  char command[ETH_COMMAND_MAX_LEN];
  size_t commandLen;

  void makeMac(uint8_t mac[6]);
  void powerUpEthernet();
  void resetEthernet();
  bool startEthernet();
  void serviceEthernet();
  void serviceClient();
  void dropClient();

public:
  EthernetCommandAPI();
  ~EthernetCommandAPI();

  bool begin(uint16_t port = ETH_API_PORT);
  void loop();

  bool isReady() const { return ethernetReady; }
  bool isConnected() const;
  uint16_t port() const { return _port; }

  bool readCommand(char* dest, size_t destSize);
  void writeReply(const char* reply);
  void writeLine(const char* line);
};

#endif // WITH_ETHERNET_COMMAND_API
