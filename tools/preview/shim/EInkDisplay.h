#pragma once
// Host shim of the freeink EInkDisplay (preview only). The preview never
// pushes pixels to hardware; it dumps the Canvas to BMP instead.

#include <cstdint>

class EInkDisplay {
 public:
  enum RefreshMode { FAST_REFRESH = 0, FULL_REFRESH = 1 };

  void begin() {}
  void clearScreen(uint8_t) {}
  void displayBuffer(int) {}
  void drawImage(const uint8_t*, int, int, int, int) {}
};
