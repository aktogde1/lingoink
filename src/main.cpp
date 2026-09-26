// LingoInk — offline language trainer for Xteink X4 (ESP32-C3).
//
// Boot: USB serial → device detect (X3/X4 panel batches) → display →
// splash → App event loop. The loop never returns; power-off and auto-sleep
// go through freeink::PowerManager deep sleep with power-button wake.

#include <Arduino.h>
#include <SPI.h>
#include <BoardConfig.h>
#include <XteinkDetect.h>

#include "app/App.h"
#include "config.h"

// X4 pins (BoardConfig keeps the authoritative copy; these feed the
// FreeInkDisplay constructor exactly like the reference firmwares do).
static constexpr int8_t EPD_SCLK = 8;
static constexpr int8_t EPD_MOSI = 10;
static constexpr int8_t EPD_CS = 21;
static constexpr int8_t EPD_DC = 4;
static constexpr int8_t EPD_RST = 5;
static constexpr int8_t EPD_BUSY = 6;

static EInkDisplay* display = nullptr;
static App* app = nullptr;

void setup() {
  Serial.begin(115200);
  const unsigned long serialStart = millis();
  while (!Serial && (millis() - serialStart < 1500)) {
    delay(10);
  }
  Serial.print("\x1B[2J\x1B[H");
  Serial.println("=== LingoInk ===");
  Serial.printf("free heap: %u\n", (unsigned)ESP.getFreeHeap());

  // Resolve the Xteink board profile (X3 vs X4, per-batch panel controller)
  // before touching the display.
  freeink::selectXteinkDevice();

  SPI.begin(EPD_SCLK, 7, EPD_MOSI, EPD_CS);
  display = new EInkDisplay(EPD_SCLK, EPD_MOSI, EPD_CS, EPD_DC, EPD_RST, EPD_BUSY);
  if (BoardConfig::ACTIVE.board == BoardConfig::Board::XteinkX3) {
    display->setDisplayX3();
  }

  app = new App(*display);
  app->run(); // never returns
}

void loop() {
  // App::run owns the loop; Arduino requires this symbol.
  delay(1000);
}
