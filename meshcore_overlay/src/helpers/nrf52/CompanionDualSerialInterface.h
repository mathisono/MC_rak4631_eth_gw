#pragma once

#include "../BaseSerialInterface.h"
#include "EthernetSerialInterface.h"
#include "SerialBLEInterface.h"

class CompanionDualSerialInterface : public BaseSerialInterface {
  enum class Transport : uint8_t {
    Ble,
    Ethernet,
  };

  struct Frame {
    uint8_t len;
    Transport source;
    uint8_t buf[MAX_FRAME_SIZE];
  };

  static constexpr uint8_t RECV_QUEUE_CAPACITY = 8;

  SerialBLEInterface ble;
  EthernetSerialInterface ethernet;
  Transport active_transport;
  uint8_t recv_queue_len;
  Frame recv_queue[RECV_QUEUE_CAPACITY];

  void clearRecvQueue();
  void enqueueRecvFrame(Transport source, const uint8_t src[], size_t len);
  size_t dequeueRecvFrame(uint8_t dest[]);
  size_t dequeueRecvFrameFromSource(Transport source, uint8_t dest[]);

public:
  CompanionDualSerialInterface();

  void begin(const char *ble_prefix, char *name, uint32_t pin_code, int ethernet_port = TCP_PORT);

  void enable() override;
  void disable() override;
  bool isEnabled() const override;

  bool isConnected() const override;

  bool isWriteBusy() const override;
  size_t writeFrame(const uint8_t src[], size_t len) override;
  size_t checkRecvFrame(uint8_t dest[]) override;
};
