#include "BLECommandAPI.h"

#ifdef WITH_BLE_COMMAND_API

BLECommandAPI::BLECommandAPI() : commandLen(0) {
  command[0] = 0;
}

void BLECommandAPI::begin(char* nodeName, uint32_t pinCode) {
  ble.begin(BLE_API_NAME_PREFIX, nodeName, pinCode);
  ble.enable();
}

void BLECommandAPI::loop() {
  uint8_t scratch[MAX_FRAME_SIZE];
  ble.checkRecvFrame(scratch);
}

bool BLECommandAPI::isConnected() const {
  return ble.isConnected();
}

bool BLECommandAPI::readCommand(char* dest, size_t destSize) {
  if (!dest || destSize == 0) return false;

  uint8_t buf[MAX_FRAME_SIZE];
  size_t len = ble.checkRecvFrame(buf);
  if (len == 0) return false;

  for (size_t i = 0; i < len; i++) {
    char c = (char)buf[i];

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
      commandLen = 0;
      command[0] = 0;
      writeLine("ERR command too long");
    }
  }

  return false;
}

void BLECommandAPI::writeLine(const char* line) {
  if (!isConnected()) return;

  char out[BLE_COMMAND_MAX_LEN + 4];
  if (line && line[0]) {
    snprintf(out, sizeof(out), "%s\r\n", line);
  } else {
    snprintf(out, sizeof(out), "\r\n");
  }
  ble.writeFrame((const uint8_t*)out, strlen(out));

  // Service the BLE TX queue immediately once.
  uint8_t scratch[MAX_FRAME_SIZE];
  ble.checkRecvFrame(scratch);
}

void BLECommandAPI::writeReply(const char* reply) {
  if (reply && reply[0]) {
    writeLine(reply);
  } else {
    writeLine("OK");
  }
}

#endif // WITH_BLE_COMMAND_API
