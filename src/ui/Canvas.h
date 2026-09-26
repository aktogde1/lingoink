#pragma once
// Canvas: a 1-bpp offscreen bitmap with text primitives for the 800x480
// e-ink panel. Bit semantics follow the panel: bit set = white, bit clear =
// black. The buffer is static (BSS), never heap-allocated — one full-screen
// canvas is the only framebuffer LingoInk needs.

#include "LgFont.h"
#include "config.h"

#include <stdint.h>

class Canvas {
 public:
  void init(int w, int h);

  int width() const { return w_; }
  int height() const { return h_; }
  uint8_t* bits() { return buf_; }
  const uint8_t* bits() const { return buf_; }
  int stride() const { return (w_ + 7) / 8; }

  void fillWhite();
  void fillBlack();

  void setPixel(int x, int y, bool black);
  void hline(int x1, int x2, int y, bool black = true);
  void vline(int x, int y1, int y2, bool black = true);
  void frame(int x, int y, int w, int h, bool black = true, int t = 1);
  void rect(int x, int y, int w, int h, bool black = true);
  void invertRect(int x, int y, int w, int h);

  // Text. (x, y) is the top of the line box; the font's ascender places the
  // baseline internally. Returns the x position after the last glyph.
  int drawText(int x, int y, const LgFont* f, const char* utf8, bool black = true);
  // Draws at most len bytes (for wrapped lines that are not NUL-terminated).
  int drawTextN(int x, int y, const LgFont* f, const char* utf8, int len, bool black = true);
  int textWidth(const LgFont* f, const char* utf8) const;
  int textWidthN(const LgFont* f, const char* utf8, int len) const;

  // Greedy word wrap. Fills linesOut[] with pointers into `text` and lens[]
  // with byte lengths. Returns the line count (capped at maxLines).
  int wrapText(const LgFont* f, const char* text, int maxWidth,
               const char** linesOut, int* lens, int maxLines) const;

  // Progress bar: outline + filled fraction (0..1). White background.
  void progressBar(int x, int y, int w, int h, float fraction);

 private:
  void drawGlyph(int x, int y, const LgFont* f, const LgGlyph* g, bool black);

  static constexpr int MAX_PIXELS = cfg::SCREEN_W * cfg::SCREEN_H;
  int w_ = 0;
  int h_ = 0;
  uint8_t buf_[MAX_PIXELS / 8];
};

// UTF-8 decoder: reads one codepoint, advances *s. Returns 0 on end of
// string; replaces invalid bytes with U+FFFD (advancing one byte).
uint32_t utf8Next(const char** s);
