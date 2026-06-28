#include "DualSerialInterface.h"

BaseSerialInterface* DualSerialInterface::chooseWriteTarget() const {
  if (active && active->isConnected()) return active;
  if (a && a->isConnected()) return a;
  if (b && b->isConnected()) return b;
  return active;
}

void DualSerialInterface::enable() {
  _enabled = true;
  if (a) a->enable();
  if (b) b->enable();
}

void DualSerialInterface::disable() {
  _enabled = false;
  if (a) a->disable();
  if (b) b->disable();
}

bool DualSerialInterface::isConnected() const {
  return (a && a->isConnected()) || (b && b->isConnected());
}

bool DualSerialInterface::isWriteBusy() const {
  BaseSerialInterface* target = chooseWriteTarget();
  return target ? target->isWriteBusy() : false;
}

size_t DualSerialInterface::writeFrame(const uint8_t src[], size_t len) {
  BaseSerialInterface* target = chooseWriteTarget();
  if (!target) return 0;
  return target->writeFrame(src, len);
}

size_t DualSerialInterface::checkRecvFrame(uint8_t dest[]) {
  if (!_enabled) return 0;

  size_t len = 0;

  if (active) {
    len = active->checkRecvFrame(dest);
    if (len > 0) return len;
  }

  BaseSerialInterface* other = (active == a) ? b : a;
  if (other) {
    len = other->checkRecvFrame(dest);
    if (len > 0) {
      active = other;
      return len;
    }
  }

  return 0;
}
