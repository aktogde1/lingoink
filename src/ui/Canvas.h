#pragma once
// Canvas: a 1-bpp offscreen bitmap with text primitives for the 800x480
// e-ink panel. Bit semantics follow the panel: bit set = white, bit clear =
// black. The buffer is static (BSS), never heap-allocated — one full-screen
// canvas is the only framebuffer LingoInk needs.
//
// Orientation: the buffer layout is ALWAYS panel-native (800x480, stride
// 100). The four rotations (landscape, portrait, landscape 180°, portrait
// 180°) are coordinate transforms inside Canvas — no second framebuffer.
// Every pixel routed through setPixel is mapped logical (x, y) -> panel:
//   0: (x, y)                      logical 800x480
//   1: (SCREEN_W-1-y, x)           logical 480x800  (content 90° CW)
//   2: (SCREEN_W-1-x, SCREEN_H-1-y) logical 800x480 (content 180°)
//   3: (y, SCREEN_H-1-x)           logical 480x800  (content 90° CCW)
// width()/height() return the LOGICAL dims so screens lay out without
// knowing the orientation.

#include "LgFont.h"
#include "config.h"

#include <stdint.h>

class Canvas {
 public:
  // Buffer is 800x480 panel-native in every orientation; only the logical
  // coordinate space and the pixel transform change.
  void init(uint8_t orientation);

  void setOrientation(uint8_t orientation);
  uint8_t orientation() const { return orient_; }
  bool portrait() const { return orient_ == 1 || orient_ == 3; }
  int width() const { return lw_; }   // logical
  int height() const { return lh_; }  // logical
  int margin() const { return portrait_ ? cfg::MARGIN_P : cfg::MARGIN; }
  uint8_t* bits() { return buf_; }
  const uint8_t* bits() const { return buf_; }
  int stride() const { return (cfg::SCREEN_W + 7) / 8; }  // panel stride

  void fillWhite();
  void fillBlack();

  void setPixel(int x, int y, bool black);
  void hline(int x1, int x2, int y, bool black = true);
  void vline(int x, int y1, int y2, bool black = true);
  void frame(int x, int y, int w, int h, bool black = true, int t = 1);
  void rect(int x, int y, int w, int h, bool black = true);
  void invertRect(int x, int y, int w, int h);
  // Rounded rectangles. r is clamped to w/2 and h/2; r = 0 degrades to
  // rect/frame. roundFrame's corners are concentric with radius r-t.
  void roundRect(int x, int y, int w, int h, int r, bool black = true);
  void roundFrame(int x, int y, int w, int h, int r, bool black = true,
                  int t = 2);
  // Dashed line — reads as a light separator on 1-bpp e-ink (no gray levels).
  void hlineDash(int x1, int x2, int y, bool black = true, int period = 4,
                 int fill = 2);
  // 1-bpp bitmap blit: rows padded to whole bytes, MSB first, bit set = drawn.
  // Same layout as LgFont glyph rows.
  void bitmap1(int x, int y, int w, int h, const uint8_t* data,
               bool black = true);

  // Text. (x, y) is the top of the line box; the font's ascender places the
  // baseline internally. Returns the x position after the last glyph.
  int drawText(int x, int y, const LgFont* f, const char* utf8, bool black = true);
  // Draws at most len bytes (for wrapped lines that are not NUL-terminated).
  int drawTextN(int x, int y, const LgFont* f, const char* utf8, int len, bool black = true);
  // White-on-black text for selected rows: caller paints a black rect first.
  int drawTextInv(int x, int y, const LgFont* f, const char* utf8, int len = -1);
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

  uint8_t orient_ = 0;  // 0/1/2/3 — see header comment
  bool portrait_ = false;
  int lw_ = 0;  // logical width  (800 landscape / 480 portrait)
  int lh_ = 0;  // logical height (480 landscape / 800 portrait)
  uint8_t buf_[(cfg::SCREEN_W * cfg::SCREEN_H) / 8];
};

// UTF-8 decoder: reads one codepoint, advances *s. Returns 0 on end of
// string; replaces invalid bytes with U+FFFD (advancing one byte).
uint32_t utf8Next(const char** s);
