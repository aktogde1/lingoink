#pragma once
// Logical key events. Hardware mapping (X4): Back/Confirm(OK)/Left/Right sit
// on the ADC ladder at GPIO1, Up/Down (side page keys) at GPIO2, Power at
// GPIO3. The freeink-sdk InputManager decodes all of that; we only consume
// edges here.

enum class Key : uint8_t {
  None = 0,
  Left,    // choice / previous
  Right,   // choice / next
  Ok,      // confirm
  OkLong,  // OK held — show explanation / extra action
  Back,
  Up,      // side button: previous page / row up
  Down,    // side button: next page / row down
  Power    // power button tap
};
