#pragma once
// Presenter pushes a rendered Canvas to the panel with an e-ink-aware
// refresh policy:
//   Fast     — cursor moves inside a screen (quick, slightly ghosty)
//   Balanced — feedback transitions (HALF)
//   Full     — screen change (slow, removes ghosting)
// After FAST_REFRESH_BETWEEN_FULL fast refreshes a full refresh is forced to
// stop ghosting from accumulating.

#include "Canvas.h"
#include "Log.h"
#include <EInkDisplay.h>

enum class Refresh : uint8_t { Fast, Balanced, Full };

class Presenter {
 public:
  Presenter(EInkDisplay& display) : display_(display) {}

  void begin() { display_.begin(); }

  // Hardware validation: one blank refresh immediately after init, with heap
  // diagnostics. Separates "SDK bus/framebuffer broken" from "rendering broken".
  void smokeTest() {
    extern void lgHeapDiag(const char* where);
    lgHeapDiag("pre-smoke");
    display_.clearScreen(0xFF);
    display_.displayBuffer(EInkDisplay::FAST_REFRESH);
    LOGI("APP", "display smoke refresh OK");
    lgHeapDiag("post-smoke");
  }

  // Queue exactly one FULL refresh for the next present() — used once at boot
  // to clear whatever the panel held through deep sleep.
  void fullNext() { forceFullOnce_ = true; }

  void present(Canvas& canvas, Refresh mode) {
    display_.drawImage(canvas.bits(), 0, 0, (uint16_t)canvas.width(),
                       (uint16_t)canvas.height());
    // Product rule (user-validated on hardware): NEVER a multi-second wipe
    // during interaction. Every update is a fast DU refresh; FULL appears only
    // once at boot (fullNext) and periodically to clear e-ink residue.
    if (forceFullOnce_ ||
        (cfg::FAST_REFRESH_BETWEEN_FULL != 0 &&
         fastStreak_ >= cfg::FAST_REFRESH_BETWEEN_FULL)) {
      display_.displayBuffer(EInkDisplay::FULL_REFRESH);
      fastStreak_ = 0;
      forceFullOnce_ = false;
    } else {
      display_.displayBuffer(EInkDisplay::FAST_REFRESH);
      fastStreak_++;
    }
  }

 private:
  EInkDisplay& display_;
  uint8_t fastStreak_ = 0;
  bool forceFullOnce_ = false;
};
