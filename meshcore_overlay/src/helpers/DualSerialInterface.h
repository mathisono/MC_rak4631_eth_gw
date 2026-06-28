#pragma once

#include <Arduino.h>
#include <helpers/BaseSerialInterface.h>

class DualSerialInterface : public BaseSerialInterface {
  BaseSerialInterface* a;
  BaseSerialInterface* b;
  BaseSerialInterface* active;
  bool _enabled;

  BaseSerialInterface* chooseWriteTarget() const;

public:
  DualSerialInterface(BaseSerialInterface& first, BaseSerialInterface& second)
      : a(&first), b(&second), active(&first), _enabled(false) {}

  void enable() override;
  void disable() override;
  bool isEnabled() const override { return _enabled; }

  bool isConnected() const override;
  bool isWriteBusy() const override;
  size_t writeFrame(const uint8_t src[], size_t len) override;
  size_t checkRecvFrame(uint8_t dest[]) override;
};
