#pragma once
// Host shim of the freeink BatteryMonitor (preview only). Reports a fixed
// healthy percentage; power-off card previews pass explicit values instead,
// so Home no longer needs a battery instance at all.

#include <cstdint>

class BatteryMonitor {
 public:
  bool readPercentageChecked(uint16_t& pct) {
    pct = 76;
    return true;
  }
};
