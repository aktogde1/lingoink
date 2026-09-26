#pragma once
// Presenter pushes a rendered Canvas to the panel with an e-ink-aware
// refresh policy:
//   Fast     — cursor moves inside a screen (quick, slightly ghosty)
//   Balanced — feedback transitions (HALF)
//   Full     — screen change (slow, removes ghosting)
// After FAST_REFRESH_BETWEEN_FULL fast refreshes a full refresh is forced to
// stop ghosting from accumulating.

#include "../ui/Canvas.h"
#include <EInkDisplay.h>

enum class Refresh : uint8_t { Fast, Balanced, Full };

class Presenter {
 public:
  Presenter(EInkDisplay& display) : display_(display) {}

  void begin() { display_.begin(); }

  void present(Canvas& canvas, Refresh mode) {
    display_.drawImage(canvas.bits(), 0, 0, (uint16_t)canvas.width(),
                       (uint16_t)canvas.height());
    EInkDisplay::RefreshMode m;
    if (mode == Refresh::Full || fastStreak_ >= cfg::FAST_REFRESH_BETWEEN_FULL) {
      m = EInkDisplay::FULL_REFRESH;
      fastStreak_ = 0;
    } else if (mode == Refresh::Balanced) {
      m = EInkDisplay::HALF_REFRESH;
      fastStreak_++;
    } else {
      m = EInkDisplay::FAST_REFRESH;
      fastStreak_++;
    }
    display_.displayBuffer(m);
  }

 private:
  EInkDisplay& display_;
  uint8_t fastStreak_ = 0;
};
