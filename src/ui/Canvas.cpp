#include "Canvas.h"

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

void Canvas::init(int w, int h) {
  w_ = w;
  h_ = h;
  fillWhite();
}

void Canvas::fillWhite() { memset(buf_, 0xFF, sizeof(buf_)); }

void Canvas::fillBlack() { memset(buf_, 0x00, sizeof(buf_)); }

void Canvas::setPixel(int x, int y, bool black) {
  if (x < 0 || y < 0 || x >= w_ || y >= h_) {
    return;
  }
  const int stride = (w_ + 7) / 8;
  uint8_t& byte = buf_[y * stride + (x >> 3)];
  const uint8_t mask = 0x80 >> (x & 7);
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
  if (x + w > w_ || y + h > h_) {
    // Clip conservatively (full overlap only used for menu rows).
    if (x >= w_ || y >= h_) return;
    if (x + w > w_) w = w_ - x;
    if (y + h > h_) h = h_ - y;
  }
  const int stride = (w_ + 7) / 8;
  for (int yy = y; yy < y + h; yy++) {
    uint8_t* row = buf_ + yy * stride;
    for (int xx = x; xx < x + w; xx++) {
      row[xx >> 3] ^= 0x80 >> (xx & 7);
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
    if (py < 0 || py >= h_) {
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
  while ((cp = utf8Next(&p)) != 0) {
    if (cp == ' ') {
      lastBreak = p - 1;
    }
    const LgGlyph* g = lgFontGlyph(f, cp);
    const int adv = g ? g->advanceX : f->advanceY / 2;
    if (x + adv > maxWidth && p - 1 > lineStart) {
      if (count >= maxLines) {
        return count; // caller decides what to do with the overflow
      }
      const char* lineEnd;
      if (lastBreak && lastBreak > lineStart) {
        lineEnd = lastBreak;
        lineStart = lastBreak + 1;
      } else {
        lineEnd = p - 1; // single glyph wider than maxWidth
        lineStart = p - 1;
      }
      // Trim trailing spaces from the finished line.
      while (lineEnd > lineStart && *(lineEnd - 1) == ' ') {
        lineEnd--;
      }
      linesOut[count] = lineStart;
      lens[count] = (int)(lineEnd - lineStart);
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
