#pragma once
// Host shim of the freeink BatteryMonitor (preview only). Reports a fixed
// healthy percentage so the Home footer shows the battery widget.

#include <cstdint>

class BatteryMonitor {
 public:
  bool readPercentageChecked(uint16_t& pct) {
    pct = 76;
    return true;
  }
};
