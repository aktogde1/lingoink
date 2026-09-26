#pragma once
// Presenter pushes a rendered Canvas to the panel with an e-ink-aware
// refresh policy:
//   Fast     — cursor moves inside a screen (quick, slightly ghosty)
//   Balanced — feedback transitions (delivered as FAST; no HALF in v0.2)
//   Full     — screen change request
// Product rule (user-validated on hardware): every interactive update is a
// fast DU refresh. A FULL refresh happens ONLY when:
//   - the panel must be wiped once (forceFullOnce_, e.g. power-off card,
//     Clean Screen Now, orientation change), or
//   - the user enabled Auto Clean in Settings (every N fast updates).
// The mode argument is advisory only — the waveform is chosen here.

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

  // Queue exactly one FULL refresh for the next present() — used for the
  // power-off card, Clean Screen Now and orientation changes.
  void fullNext() { forceFullOnce_ = true; }

  // Auto clean: 0 = off (default), else a FULL refresh every N fast updates.
  void setAutoCleanScreens(uint8_t every) {
    autoCleanEvery_ = every;
    if (autoCleanEvery_ == 0) fastStreak_ = 0;
  }

  void present(Canvas& canvas, Refresh mode) {
    (void)mode;
    // The canvas buffer is always panel-native 800x480 (portrait is a
    // coordinate transform inside Canvas) — blit the full panel every time.
    display_.drawImage(canvas.bits(), 0, 0, (uint16_t)cfg::SCREEN_W,
                       (uint16_t)cfg::SCREEN_H);
    if (forceFullOnce_ ||
        (autoCleanEvery_ != 0 && fastStreak_ >= autoCleanEvery_)) {
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
  uint8_t autoCleanEvery_ = 0;
  bool forceFullOnce_ = false;
};
