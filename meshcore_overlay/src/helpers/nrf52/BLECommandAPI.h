#pragma once

#include <Arduino.h>

#ifdef WITH_BLE_COMMAND_API

#include <helpers/nrf52/SerialBLEInterface.h>

#ifndef BLE_API_NAME_PREFIX
#define BLE_API_NAME_PREFIX "MC-RPT-"
#endif

#ifndef BLE_API_PIN_CODE
#define BLE_API_PIN_CODE 123456
#endif

#ifndef BLE_COMMAND_MAX_LEN
#define BLE_COMMAND_MAX_LEN 160
#endif

/**
 * Line-oriented BLE command API for MeshCore repeater firmware.
 *
 * Uses MeshCore's existing encrypted BLE UART transport, but treats the payload
 * as plain text commands instead of Companion binary API frames.
 *
 *   BLE client -> text command + \r or \n
 *   firmware   -> text reply + \r\n
 */
class BLECommandAPI {
  SerialBLEInterface ble;
  char command[BLE_COMMAND_MAX_LEN];
  size_t commandLen;

public:
  BLECommandAPI();

  void begin(char* nodeName, uint32_t pinCode = BLE_API_PIN_CODE);
  void loop();

  bool isConnected() const;
  bool readCommand(char* dest, size_t destSize);
  void writeReply(const char* reply);
  void writeLine(const char* line);
};

#endif // WITH_BLE_COMMAND_API
