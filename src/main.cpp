// LingoInk — offline language trainer for Xteink X4 (ESP32-C3).
//
// Boot order (must not be reordered):
//   1. USB serial
//   2. XteinkDetect: X3-vs-X4 fingerprint, then per-batch panel-controller
//      probe — BOTH before SPI claims the display pins (the probe is a
//      half-duplex read on the display bus).
//   3. SPI + EInkDisplay
//   4. App event loop (never returns; power-off and auto-sleep go through
//      freeink::PowerManager deep sleep with power-button wake).

#include <Arduino.h>
#include <SPI.h>
#include <BoardConfig.h>
#include <XteinkDetect.h>

#include "app/App.h"
#include "config.h"
#include "ui/Log.h"

// X4 pins (BoardConfig keeps the authoritative copy; these feed the
// FreeInkDisplay constructor exactly like the reference firmwares do).
static constexpr int8_t EPD_SCLK = 8;
static constexpr int8_t EPD_MOSI = 10;
static constexpr int8_t EPD_CS = 21;
static constexpr int8_t EPD_DC = 4;
static constexpr int8_t EPD_RST = 5;
static constexpr int8_t EPD_BUSY = 6;
static constexpr int8_t SD_MISO = 7;

static EInkDisplay* display = nullptr;

void setup() {
  Serial.begin(115200);
  const unsigned long serialStart = millis();
  while (!Serial && (millis() - serialStart < 1500)) {
    delay(10);
  }
  Serial.print("\x1B[2J\x1B[H");
  Serial.println("=== LingoInk ===");
  LOGI("BOOT", "boot start, free heap=%u", (unsigned)ESP.getFreeHeap());

  // 1. Board profile: X3 vs X4 (I2C fingerprint, self-contained Wire usage).
  const bool isX3 = freeink::selectXteinkDevice();
  LOGI("HW", "device: %s (%s controller default)",
       isX3 ? "Xteink X3" : "Xteink X4", isX3 ? "UC8253" : "SSD1677");

  // 2. Per-batch panel controller — before SPI owns the display pins.
  //    Promotes the X4 SSD1677 default to UC8279/UC8179 on UltraChip batches.
  const bool promoted = freeink::applyXteinkDisplayController();
  LOGI("HW", "panel controller probe: %s",
       promoted ? "UltraChip variant promoted" : "default controller kept");

  // 3. Bus + SD FIRST, display after: on the X4 the e-ink driver claims the
  //    shared SPI pins, and an SD.begin() after display init fails on real
  //    hardware (verified: eenk mounts the card before constructing the
  //    display; the reversed order was LingoInk bug #1 on device).
  SPI.begin(EPD_SCLK, SD_MISO, EPD_MOSI, EPD_CS);
  bool sdOk = SD.begin(cfg::SD_CS_PIN, SPI, 25000000, "/sd", 8);
  if (sdOk) {
    LOGI("SD", "SD mounted (type=%u size=%llu MB, pre-display)",
         (unsigned)SD.cardType(), (unsigned long long)(SD.cardSize() / (1000ull * 1000ull)));
  } else {
    LOGW("SD", "SD init failed before display init (card seated?)");
  }

  display = new EInkDisplay(EPD_SCLK, EPD_MOSI, EPD_CS, EPD_DC, EPD_RST, EPD_BUSY);
  if (isX3) {
    display->setDisplayX3();
  }
  LOGI("HW", "display constructed (%ux%u), free heap=%u",
       (unsigned)BoardConfig::ACTIVE.displayWidth,
       (unsigned)BoardConfig::ACTIVE.displayHeight, (unsigned)ESP.getFreeHeap());

  // 4. Display begin BEFORE the App exists: begin() heap-allocates the panel
  //    framebuffer (~53 KB contiguous) and MUST get the cleanest pool of the
  //    boot. App (~90 KB) is placed after it. Verified crash on the X4: the
  //    reverse order left the framebuffer NULL and panicked in SPI write.
  const uint32_t freeBefore = ESP.getFreeHeap();
  display->begin();
  const uint32_t freeAfter = ESP.getFreeHeap();
  lgHeapDiag("post-display-begin");
  // begin() must consume ~49 KB (48K framebuffer + allocator overhead) from
  // the free pool. Note: largest-block delta is NOT a valid signal — the
  // allocation may carve from a different hole than the largest one.
  const uint32_t consumed = freeBefore - freeAfter;
  if (consumed < 45 * 1024) {
    LOGE("HW", "framebuffer not allocated (begin consumed only %u bytes)",
         (unsigned)consumed);
    while (true) {
      delay(1000); // park with a diagnostic instead of a panic later
    }
  }
  LOGI("HW", "framebuffer allocated OK (begin consumed %u bytes)", (unsigned)consumed);

  // 5. App on the heap, after the framebuffer (never returns).
  App* app = new App(*display, sdOk);
  app->run();
  LOGE("BOOT", "app loop returned");
}

void loop() {
  // App::run owns the loop; Arduino requires this symbol.
  delay(1000);
}
