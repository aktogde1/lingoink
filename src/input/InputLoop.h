#pragma once
// InputLoop wraps the freeink-sdk InputManager (single instance — it owns
// edge state) and turns raw button edges into Key events, adding a
// synthesized OkLong when OK is held. Also tracks idle time for auto-sleep.

#include "Keys.h"
#include <InputManager.h>
#include <Arduino.h>

class InputLoop {
 public:
  void begin() { im_.begin(); }

  // Call from the main loop; returns at most one Key per call.
  Key poll(uint32_t nowMs) {
    im_.update();
    Key k = Key::None;

    if (im_.wasPressed(InputManager::BTN_LEFT)) k = Key::Left;
    else if (im_.wasPressed(InputManager::BTN_RIGHT)) k = Key::Right;
    else if (im_.wasPressed(InputManager::BTN_BACK)) k = Key::Back;
    else if (im_.wasPressed(InputManager::BTN_UP)) k = Key::Up;
    else if (im_.wasPressed(InputManager::BTN_DOWN)) k = Key::Down;
    else if (im_.wasPressed(InputManager::BTN_POWER)) k = Key::Power;

    // OK: short press fires on release (unless the long-press already fired);
    // holding >= OK_LONG_PRESS_MS fires OkLong once.
    const bool okDown = im_.isPressed(InputManager::BTN_CONFIRM);
    if (okDown) {
      if (okDownSince_ == 0) {
        okDownSince_ = nowMs;
        okLongFired_ = false;
      } else if (!okLongFired_ && nowMs - okDownSince_ >= cfg::OK_LONG_PRESS_MS) {
        okLongFired_ = true;
        k = Key::OkLong;
      }
    } else {
      if (okDownSince_ != 0 && !okLongFired_) {
        k = Key::Ok;
      }
      okDownSince_ = 0;
    }

    if (k != Key::None) {
      lastActivityMs_ = nowMs;
    }
    return k;
  }

  uint32_t idleMs(uint32_t nowMs) const { return nowMs - lastActivityMs_; }

 private:
  InputManager im_;
  uint32_t okDownSince_ = 0;
  bool okLongFired_ = false;
  uint32_t lastActivityMs_ = 0;
};
