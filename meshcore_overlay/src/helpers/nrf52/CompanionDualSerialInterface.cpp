#include "CompanionDualSerialInterface.h"

#include <string.h>

CompanionDualSerialInterface::CompanionDualSerialInterface() : recv_queue_len(0) {}

void CompanionDualSerialInterface::clearRecvQueue() {
  recv_queue_len = 0;
}

void CompanionDualSerialInterface::enqueueRecvFrame(const uint8_t src[], size_t len) {
  if (len == 0 || len > MAX_FRAME_SIZE) {
    return;
  }

  if (recv_queue_len >= RECV_QUEUE_CAPACITY) {
    for (uint8_t i = 0; i + 1 < recv_queue_len; i++) {
      recv_queue[i] = recv_queue[i + 1];
    }
    recv_queue_len--;
  }

  recv_queue[recv_queue_len].len = len;
  memcpy(recv_queue[recv_queue_len].buf, src, len);
  recv_queue_len++;
}

size_t CompanionDualSerialInterface::dequeueRecvFrame(uint8_t dest[]) {
  if (recv_queue_len == 0) {
    return 0;
  }

  size_t len = recv_queue[0].len;
  memcpy(dest, recv_queue[0].buf, len);

  recv_queue_len--;
  for (uint8_t i = 0; i < recv_queue_len; i++) {
    recv_queue[i] = recv_queue[i + 1];
  }

  return len;
}

void CompanionDualSerialInterface::begin(const char *ble_prefix, char *name, uint32_t pin_code,
                                         int ethernet_port) {
  ble.begin(ble_prefix, name, pin_code);
  ethernet.begin(ethernet_port);
}

void CompanionDualSerialInterface::enable() {
  ble.enable();
  ethernet.enable();
  clearRecvQueue();
}

void CompanionDualSerialInterface::disable() {
  ble.disable();
  ethernet.disable();
  clearRecvQueue();
}

bool CompanionDualSerialInterface::isEnabled() const {
  return ble.isEnabled() || ethernet.isEnabled();
}

bool CompanionDualSerialInterface::isConnected() const {
  return ble.isConnected() || ethernet.isConnected();
}

bool CompanionDualSerialInterface::isWriteBusy() const {
  return ble.isWriteBusy() || ethernet.isWriteBusy();
}

size_t CompanionDualSerialInterface::writeFrame(const uint8_t src[], size_t len) {
  size_t written = 0;

  if (ble.writeFrame(src, len) == len) {
    written = len;
  }
  if (ethernet.writeFrame(src, len) == len) {
    written = len;
  }

  return written;
}

size_t CompanionDualSerialInterface::checkRecvFrame(uint8_t dest[]) {
  uint8_t frame[MAX_FRAME_SIZE];

  size_t got = ble.checkRecvFrame(frame);
  if (got > 0) {
    enqueueRecvFrame(frame, got);
  }

  got = ethernet.checkRecvFrame(frame);
  if (got > 0) {
    enqueueRecvFrame(frame, got);
  }

  return dequeueRecvFrame(dest);
}
