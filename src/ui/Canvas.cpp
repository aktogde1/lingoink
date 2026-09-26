#include "Canvas.h"

#include <cmath>
#include <cstring>

uint32_t utf8Next(const char** s) {
  const uint8_t* p = reinterpret_cast<const uint8_t*>(*s);
  if (!p || *p == 0) {
    return 0;
  }
  uint32_t cp = 0;
  int extra = 0;
  uint8_t b = *p++;
  if (b < 0x80) {
    cp = b;
  } else if ((b & 0xE0) == 0xC0) {
    cp = b & 0x1F;
    extra = 1;
  } else if ((b & 0xF0) == 0xE0) {
    cp = b & 0x0F;
    extra = 2;
  } else if ((b & 0xF8) == 0xF0) {
    cp = b & 0x07;
    extra = 3;
  } else {
    *s = reinterpret_cast<const char*>(p);
    return 0xFFFD;
  }
  for (int i = 0; i < extra; i++) {
    uint8_t c = *p;
    if ((c & 0xC0) != 0x80) {
      *s = reinterpret_cast<const char*>(p);
      return 0xFFFD;
    }
    cp = (cp << 6) | (c & 0x3F);
    p++;
  }
  *s = reinterpret_cast<const char*>(p);
  return cp;
}

void Canvas::init(uint8_t orientation) {
  setOrientation(orientation);
  fillWhite();
}

void Canvas::setOrientation(uint8_t orientation) {
  if (orientation > 3) orientation = 0;
  orient_ = orientation;
  portrait_ = (orient_ == 1 || orient_ == 3);
  lw_ = portrait_ ? cfg::SCREEN_H : cfg::SCREEN_W;
  lh_ = portrait_ ? cfg::SCREEN_W : cfg::SCREEN_H;
}

void Canvas::fillWhite() { memset(buf_, 0xFF, sizeof(buf_)); }

void Canvas::fillBlack() { memset(buf_, 0x00, sizeof(buf_)); }

void Canvas::setPixel(int x, int y, bool black) {
  if (x < 0 || y < 0 || x >= lw_ || y >= lh_) {
    return;
  }
  // Logical -> panel transform per orientation (see Canvas.h).
  int px, py;
  switch (orient_) {
    case 1: px = cfg::SCREEN_W - 1 - y; py = x; break;
    case 2: px = cfg::SCREEN_W - 1 - x; py = cfg::SCREEN_H - 1 - y; break;
    case 3: px = y; py = cfg::SCREEN_H - 1 - x; break;
    default: px = x; py = y; break;
  }
  uint8_t& byte = buf_[py * stride() + (px >> 3)];
  const uint8_t mask = 0x80 >> (px & 7);
  if (black) {
    byte &= ~mask;
  } else {
    byte |= mask;
  }
}

void Canvas::hline(int x1, int x2, int y, bool black) {
  if (x1 > x2) {
    int t = x1;
    x1 = x2;
    x2 = t;
  }
  for (int x = x1; x <= x2; x++) {
    setPixel(x, y, black);
  }
}

void Canvas::vline(int x, int y1, int y2, bool black) {
  if (y1 > y2) {
    int t = y1;
    y1 = y2;
    y2 = t;
  }
  for (int y = y1; y <= y2; y++) {
    setPixel(x, y, black);
  }
}

void Canvas::frame(int x, int y, int w, int h, bool black, int t) {
  for (int i = 0; i < t; i++) {
    hline(x, x + w - 1, y + i, black);
    hline(x, x + w - 1, y + h - 1 - i, black);
    vline(x + i, y, y + h - 1, black);
    vline(x + w - 1 - i, y, y + h - 1, black);
  }
}

void Canvas::rect(int x, int y, int w, int h, bool black) {
  for (int yy = y; yy < y + h; yy++) {
    for (int xx = x; xx < x + w; xx++) {
      setPixel(xx, yy, black);
    }
  }
}

void Canvas::invertRect(int x, int y, int w, int h) {
  if (x < 0) { w += x; x = 0; }
  if (y < 0) { h += y; y = 0; }
  if (x + w > lw_ || y + h > lh_) {
    // Clip conservatively (full overlap only used for menu rows).
    if (x >= lw_ || y >= lh_) return;
    if (x + w > lw_) w = lw_ - x;
    if (y + h > lh_) h = lh_ - y;
  }
  for (int yy = y; yy < y + h; yy++) {
    for (int xx = x; xx < x + w; xx++) {
      // Route through the orientation transform (XOR the mapped pixel).
      int px, py;
      switch (orient_) {
        case 1: px = cfg::SCREEN_W - 1 - yy; py = xx; break;
        case 2: px = cfg::SCREEN_W - 1 - xx; py = cfg::SCREEN_H - 1 - yy; break;
        case 3: px = yy; py = cfg::SCREEN_H - 1 - xx; break;
        default: px = xx; py = yy; break;
      }
      buf_[py * stride() + (px >> 3)] ^= 0x80 >> (px & 7);
    }
  }
}

// Corner inset of a rounded rectangle at row yy (0-based from its top):
// how many pixels the row's span is narrower on each side. Rows at or below
// the corner radius r are full-width.
static int cornerInset(int r, int yy) {
  if (r <= 0 || yy >= r) return 0;
  const int dy = r - yy;  // distance from the corner-circle center row
  const int dx = (int)sqrtf((float)(r * r - dy * dy));
  return r - dx;
}

static int clampRadius(int r, int w, int h) {
  if (r < 0) r = 0;
  if (r * 2 > w) r = w / 2;
  if (r * 2 > h) r = h / 2;
  return r;
}

void Canvas::roundRect(int x, int y, int w, int h, int r, bool black) {
  if (w <= 0 || h <= 0) return;
  r = clampRadius(r, w, h);
  if (r == 0) {
    rect(x, y, w, h, black);
    return;
  }
  for (int yy = 0; yy < h; yy++) {
    int inset;
    if (yy < r) {
      inset = cornerInset(r, yy);
    } else if (yy >= h - r) {
      inset = cornerInset(r, h - 1 - yy);
    } else {
      inset = 0;
    }
    hline(x + inset, x + w - 1 - inset, y + yy, black);
  }
}

void Canvas::roundFrame(int x, int y, int w, int h, int r, bool black, int t) {
  if (w <= 0 || h <= 0) return;
  r = clampRadius(r, w, h);
  if (t < 1) t = 1;
  if (t * 2 > w) t = w / 2;
  if (t * 2 > h) t = h / 2;
  if (r == 0) {
    frame(x, y, w, h, black, t);
    return;
  }
  // Clean ring: fill the outer rounded rect, then hollow it with the inner
  // rounded rect. (Per-row ring drawing produced stepped, lopsided corners
  // on the panel.) NOTE: the interior is painted opaque — roundFrame is for
  // self-contained boxes (cards, chips, bars), not for outlining content.
  roundRect(x, y, w, h, r, black);
  const int ri = clampRadius(r - t, w - 2 * t, h - 2 * t);
  roundRect(x + t, y + t, w - 2 * t, h - 2 * t, ri, !black);
}

void Canvas::hlineDash(int x1, int x2, int y, bool black, int period, int fill) {
  if (period < 2) {
    hline(x1, x2, y, black);
    return;
  }
  if (fill < 1) fill = 1;
  if (fill >= period) {
    hline(x1, x2, y, black);
    return;
  }
  if (x1 > x2) {
    int t = x1;
    x1 = x2;
    x2 = t;
  }
  for (int x = x1; x <= x2; x++) {
    if (((x - x1) % period) < fill) {
      setPixel(x, y, black);
    }
  }
}

void Canvas::bitmap1(int x, int y, int w, int h, const uint8_t* data,
                     bool black) {
  if (w <= 0 || h <= 0 || !data) return;
  const int nb = (w + 7) / 8;
  for (int gy = 0; gy < h; gy++) {
    for (int gx = 0; gx < w; gx++) {
      if (data[gy * nb + (gx >> 3)] & (0x80 >> (gx & 7))) {
        setPixel(x + gx, y + gy, black);
      }
    }
  }
}

void Canvas::drawGlyph(int x, int y, const LgFont* f, const LgGlyph* g, bool black) {
  if (!g || g->width == 0) {
    return;
  }
  const int nb = (g->width + 7) / 8;
  const uint8_t* data = f->bitmap + g->dataOffset;
  // y is the top of the line box; baseline sits at y + ascender.
  const int top = y + f->ascender - g->top;
  for (int gy = 0; gy < g->height; gy++) {
    const int py = top + gy;
    if (py < 0 || py >= lh_) {
      continue;
    }
    for (int gx = 0; gx < g->width; gx++) {
      if (data[gy * nb + (gx >> 3)] & (0x80 >> (gx & 7))) {
        setPixel(x + g->left + gx, py, black);
      }
    }
  }
}

int Canvas::drawTextN(int x, int y, const LgFont* f, const char* utf8, int len, bool black) {
  const char* p = utf8;
  const char* end = utf8 + len;
  uint32_t cp;
  while (p < end && (cp = utf8Next(&p)) != 0) {
    const LgGlyph* g = lgFontGlyph(f, cp);
    if (g) {
      drawGlyph(x, y, f, g, black);
      x += g->advanceX;
    } else {
      x += f->advanceY / 2; // missing glyph: small gap
    }
  }
  return x;
}

int Canvas::drawText(int x, int y, const LgFont* f, const char* utf8, bool black) {
  return drawTextN(x, y, f, utf8, (int)strlen(utf8), black);
}

int Canvas::drawTextInv(int x, int y, const LgFont* f, const char* utf8, int len) {
  const int n = (len < 0) ? (int)strlen(utf8) : len;
  return drawTextN(x, y, f, utf8, n, false); // white glyphs on caller's black row
}

int Canvas::textWidthN(const LgFont* f, const char* utf8, int len) const {
  int x = 0;
  const char* p = utf8;
  const char* end = utf8 + len;
  uint32_t cp;
  while (p < end && (cp = utf8Next(&p)) != 0) {
    const LgGlyph* g = lgFontGlyph(f, cp);
    x += g ? g->advanceX : f->advanceY / 2;
  }
  return x;
}

int Canvas::textWidth(const LgFont* f, const char* utf8) const {
  return textWidthN(f, utf8, (int)strlen(utf8));
}

int Canvas::wrapText(const LgFont* f, const char* text, int maxWidth,
                     const char** linesOut, int* lens, int maxLines) const {
  int count = 0;
  const char* lineStart = text;
  const char* p = text;
  const char* lastBreak = nullptr; // last candidate space inside this line
  int x = 0;
  uint32_t cp;
  while (true) {
    const char* glyphStart=p;
    cp=utf8Next(&p);
    if(!cp)break;
    if (cp == ' ') {
      lastBreak = p - 1;
    }
    const LgGlyph* g = lgFontGlyph(f, cp);
    const int adv = g ? g->advanceX : f->advanceY / 2;
    if (x + adv > maxWidth && glyphStart > lineStart) {
      if (count >= maxLines) {
        return count; // caller decides what to do with the overflow
      }
      const char* lineEnd;
      const char* finished = lineStart;  // the line being flushed
      if (lastBreak && lastBreak > lineStart) {
        lineEnd = lastBreak;
        lineStart = lastBreak + 1;
      } else {
        lineEnd = glyphStart; // break at a UTF-8 codepoint boundary
        lineStart = glyphStart;
      }
      // Trim trailing spaces from the finished line.
      while (lineEnd > finished && *(lineEnd - 1) == ' ') {
        lineEnd--;
      }
      linesOut[count] = finished;
      lens[count] = (int)(lineEnd - finished);
      count++;
      // Re-measure the tail carried into the next line.
      x = 0;
      const char* q = lineStart;
      while (q < p) {
        uint32_t qc = utf8Next(&q);
        const LgGlyph* qg = lgFontGlyph(f, qc);
        x += qg ? qg->advanceX : f->advanceY / 2;
      }
      lastBreak = nullptr;
      continue;
    }
    x += adv;
  }
  if (count < maxLines && *lineStart != 0) {
    const char* lineEnd = text + strlen(text);
    while (lineEnd > lineStart && *(lineEnd - 1) == ' ') {
      lineEnd--;
    }
    linesOut[count] = lineStart;
    lens[count] = (int)(lineEnd - lineStart);
    count++;
  }
  return count;
}

void Canvas::progressBar(int x, int y, int w, int h, float fraction) {
  if (fraction < 0) fraction = 0;
  if (fraction > 1) fraction = 1;
  frame(x, y, w, h, true, 2);
  const int inner = (w - 6) * fraction;
  if (inner > 0) {
    rect(x + 3, y + 3, inner, h - 6, true);
  }
}
