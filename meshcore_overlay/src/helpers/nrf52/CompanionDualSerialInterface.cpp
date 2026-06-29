#include "CompanionDualSerialInterface.h"

#include <string.h>

CompanionDualSerialInterface::CompanionDualSerialInterface()
    : active_transport(Transport::Ble), recv_queue_len(0) {}

void CompanionDualSerialInterface::clearRecvQueue() {
  recv_queue_len = 0;
}

void CompanionDualSerialInterface::enqueueRecvFrame(Transport source, const uint8_t src[], size_t len) {
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
  recv_queue[recv_queue_len].source = source;
  memcpy(recv_queue[recv_queue_len].buf, src, len);
  recv_queue_len++;
}

size_t CompanionDualSerialInterface::dequeueRecvFrame(uint8_t dest[]) {
  if (recv_queue_len == 0) {
    return 0;
  }

  size_t len = recv_queue[0].len;
  memcpy(dest, recv_queue[0].buf, len);
  active_transport = recv_queue[0].source;

  recv_queue_len--;
  for (uint8_t i = 0; i < recv_queue_len; i++) {
    recv_queue[i] = recv_queue[i + 1];
  }

  return len;
}

size_t CompanionDualSerialInterface::dequeueRecvFrameFromSource(Transport source, uint8_t dest[]) {
  if (recv_queue_len == 0) {
    return 0;
  }

  for (uint8_t i = 0; i < recv_queue_len; i++) {
    if (recv_queue[i].source != source) {
      continue;
    }

    size_t len = recv_queue[i].len;
    memcpy(dest, recv_queue[i].buf, len);
    active_transport = recv_queue[i].source;

    for (uint8_t j = i; j + 1 < recv_queue_len; j++) {
      recv_queue[j] = recv_queue[j + 1];
    }
    recv_queue_len--;
    return len;
  }

  return 0;
}

void CompanionDualSerialInterface::begin(const char *ble_prefix, char *name, uint32_t pin_code,
                                         int ethernet_port) {
  ble.begin(ble_prefix, name, pin_code);
  ethernet.begin(ethernet_port);
}

void CompanionDualSerialInterface::enable() {
  ble.enable();
  ethernet.enable();
  active_transport = Transport::Ble;
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

  size_t got = 0;

  if (active_transport == Transport::Ble) {
    got = ble.checkRecvFrame(frame);
    if (got > 0) {
      enqueueRecvFrame(Transport::Ble, frame, got);
    }

    got = ethernet.checkRecvFrame(frame);
    if (got > 0) {
      enqueueRecvFrame(Transport::Ethernet, frame, got);
    }
  } else {
    got = ethernet.checkRecvFrame(frame);
    if (got > 0) {
      enqueueRecvFrame(Transport::Ethernet, frame, got);
    }

    got = ble.checkRecvFrame(frame);
    if (got > 0) {
      enqueueRecvFrame(Transport::Ble, frame, got);
    }
  }

  if (recv_queue_len == 0) {
    return 0;
  }

  if (active_transport == Transport::Ble) {
    got = dequeueRecvFrameFromSource(Transport::Ble, dest);
    if (got > 0) {
      return got;
    }
  } else {
    got = dequeueRecvFrameFromSource(Transport::Ethernet, dest);
    if (got > 0) {
      return got;
    }
  }

  return dequeueRecvFrame(dest);
}
