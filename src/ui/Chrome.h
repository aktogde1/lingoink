#pragma once
// Chrome: the shared visual language of LingoInk screens — one source of
// truth for header/footer geometry, list rows, the selection treatment,
// separators, dialogs and small widgets. Screens draw through these helpers
// so spacing and styling cannot drift apart (pre-Chrome, every screen
// carried its own copy of the magic numbers).
//
// All geometry derives from cfg:: chrome metrics + the Canvas orientation;
// portrait gets its own row heights automatically (metrics()).

#include "Canvas.h"
#include "FontRegistry.h"
#include "Strings.h"

#include <cstdio>
#include <cstring>

namespace chrome {

// ---------------------------------------------------------------- metrics
struct Metrics {
  int m = 0;         // side margin
  int w = 0;         // logical width
  int h = 0;         // logical height
  int headerTextY;   // title text top
  int ruleY;         // header rule y
  int top;           // first content row y
  int bottom;        // content must end above this y
  int widgetY;       // bottom-right widgets (battery, ^/+N) top
  int rowH;          // one-line list row pitch
  int menuRowH;      // two-line (label + subtitle) row pitch
};

inline Metrics metrics(const Canvas& c) {
  const bool p = c.portrait();
  const LgFont* title = fontByRole(FontRole::Title);
  Metrics mt;
  mt.m = c.margin();
  mt.w = c.width();
  mt.h = c.height();
  mt.headerTextY = mt.m + cfg::HEADER_TEXT_OFF;
  mt.ruleY = mt.headerTextY + title->advanceY + cfg::HEADER_RULE_GAP;
  mt.top = mt.ruleY + cfg::CONTENT_GAP;
  mt.widgetY = mt.h - cfg::FOOT_WIDGET_OFF;
  mt.bottom = mt.widgetY - cfg::CONTENT_FOOT_GAP;
  mt.rowH = p ? cfg::ROW_H_P : cfg::ROW_H;
  mt.menuRowH = p ? cfg::MENU_ROW_H_P : cfg::MENU_ROW_H;
  return mt;
}

// ---------------------------------------------------------------- pieces

// Truncates `text` to fit maxW in font f, appending "…" when cut. Writes
// into buf/n (>= 8 bytes). A no-op copy when the text already fits.
inline void fitText(Canvas& c, const LgFont* f, const char* text, int maxW,
                    char* buf, size_t n) {
  if (c.textWidth(f, text) <= maxW) {
    snprintf(buf, n, "%s", text);
    return;
  }
  size_t len = strlen(text);
  if (len >= n) len = n - 4;
  memcpy(buf, text, len);
  buf[len] = 0;
  while (len > 0 && c.textWidthN(f, buf, (int)len) > maxW) len--;
  buf[len] = 0;
  if (len + 4 < n) {
    buf[len] = (char)0xE2;
    buf[len + 1] = (char)0x80;
    buf[len + 2] = (char)0xA6;  // …
    buf[len + 3] = 0;
  }
}

// Right-aligned text; returns the x where the text starts.
inline int drawRight(Canvas& c, int xRight, int y, const LgFont* f,
                     const char* s) {
  const int w = c.textWidth(f, s);
  c.drawText(xRight - w, y, f, s);
  return xRight - w;
}

// Right-aligned inverse text (inside a selected row card).
inline int drawRightInv(Canvas& c, int xRight, int y, const LgFont* f,
                        const char* s) {
  const int w = c.textWidth(f, s);
  c.drawTextInv(xRight - w, y, f, s);
  return xRight - w;
}

// Screen header: title on the left, optional right-aligned small text
// baseline-aligned with the title, solid rule underneath. The left title
// is shortened with an ellipsis when it would collide with the right part
// (narrow portrait + long book/rule titles).
inline void header(Canvas& c, const Metrics& mt, const char* left,
                   const char* right = nullptr) {
  const LgFont* title = fontByRole(FontRole::Title);
  int avail = mt.w - 2 * mt.m;
  if (right && right[0]) {
    const LgFont* ui = fontByRole(FontRole::UI);
    avail -= c.textWidth(ui, right) + 24;
  }
  if (avail < 80) avail = 80;
  char buf[96];
  fitText(c, title, left, avail, buf, sizeof(buf));
  c.drawText(mt.m, mt.headerTextY, title, buf);
  if (right && right[0]) {
    const LgFont* ui = fontByRole(FontRole::UI);
    const int ry = mt.headerTextY + (title->ascender - ui->ascender);
    drawRight(c, mt.w - mt.m, ry, ui, right);
  }
  c.hline(mt.m, mt.w - mt.m, mt.ruleY);
}

// Sliding-window hints in the bottom-right corner: "+N" below, "^" above.
// There is no footer line and no button-hint text anywhere (they only ate
// space); the bottom edge stays clean.
inline void scrollHints(Canvas& c, const Metrics& mt, int hiddenBelow,
                        bool aboveTop) {
  const LgFont* ui = fontByRole(FontRole::UI);
  int xr = mt.w - mt.m;
  if (aboveTop) {
    c.drawText(xr - 10, mt.widgetY, ui, "^");
    xr -= 22;
  }
  if (hiddenBelow > 0) {
    char more[16];
    snprintf(more, sizeof(more), "+%u", (unsigned)hiddenBelow);
    drawRight(c, xr, mt.widgetY, ui, more);
  }
}

// The one selection treatment: filled rounded card wider than the margin
// box. Contract: (textTop, textH) describe the TEXT BLOCK itself — the card
// is drawn around it with fixed padding, so wrapped lines can never fall
// outside the card. Draw the row's text with drawTextInv on top of it.
inline void selection(Canvas& c, const Metrics& mt, int textTop, int textH) {
  c.roundRect(mt.m - cfg::SEL_PAD_X, textTop - cfg::SEL_PAD_Y,
              mt.w - 2 * mt.m + 2 * cfg::SEL_PAD_X,
              textH + 2 * cfg::SEL_PAD_Y, cfg::SEL_RADIUS, true);
}

// Light separator between unselected rows (a dash reads as "gray" on 1-bpp).
inline void separator(Canvas& c, const Metrics& mt, int y) {
  c.hlineDash(mt.m, mt.w - mt.m, y, true, cfg::DASH_PERIOD, cfg::DASH_FILL);
}

// Two-line menu row (Home): label + explanatory subtitle. Unselected rows
// shorten the subtitle with an ellipsis; the SELECTED row expands it to two
// wrapped lines so long lesson names stay readable without any animation
// (e-ink has no partial updates — no marquee possible). Both variants fit
// the same row card.
inline void menuRow(Canvas& c, const Metrics& mt, int y, const char* label,
                    const char* sub, bool selected) {
  const LgFont* body = fontByRole(FontRole::Body);
  const LgFont* ui = fontByRole(FontRole::UI);
  char lb[96];
  fitText(c, body, label, mt.w - 2 * mt.m, lb, sizeof(lb));
  const int subY = y + body->advanceY + 8;
  const int subW = mt.w - 2 * mt.m;
  if (selected) {
    const char* sl[2];
    int slens[2];
    const int n = c.wrapText(ui, sub, subW, sl, slens, 2);
    // Card hugs the whole text block (label + gap + subtitle lines).
    const int blockH = body->advanceY + 8 + n * ui->advanceY - 4;
    selection(c, mt, y, blockH);
    c.drawTextInv(mt.m, y, body, lb);
    for (int i = 0; i < n; i++) {
      c.drawTextInv(mt.m, subY + i * ui->advanceY, ui, sl[i], slens[i]);
    }
  } else {
    char sb[96];
    fitText(c, ui, sub, subW, sb, sizeof(sb));
    c.drawText(mt.m, y, body, lb);
    c.drawText(mt.m, subY, ui, sb);
  }
}

// One-line settings row: label left, optional value right-aligned; the
// label yields (ellipsis) when the pair would collide.
inline void settingRow(Canvas& c, const Metrics& mt, int y, const char* label,
                       const char* value, bool selected) {
  const LgFont* body = fontByRole(FontRole::Body);
  const int vw = (value && value[0]) ? c.textWidth(body, value) : 0;
  char lb[96];
  int maxW = mt.w - 2 * mt.m;
  if (vw > 0) maxW -= vw + 24;
  fitText(c, body, label, maxW, lb, sizeof(lb));
  if (selected) selection(c, mt, y, body->advanceY - 2);
  if (selected) c.drawTextInv(mt.m, y, body, lb);
  else c.drawText(mt.m, y, body, lb);
  if (vw > 0) {
    if (selected) c.drawTextInv(mt.w - mt.m - vw, y, body, value);
    else c.drawText(mt.w - mt.m - vw, y, body, value);
  }
}

// ---------------------------------------------------------------- widgets

// Rounded "pill" progress bar: outline + rounded fill.
inline void pillBar(Canvas& c, int x, int y, int w, int h, float fraction) {
  if (fraction < 0) fraction = 0;
  if (fraction > 1) fraction = 1;
  c.roundFrame(x, y, w, h, h / 2, true, 2);
  const int inner = (int)((w - 6) * fraction);
  if (inner > 0) {
    c.roundRect(x + 3, y + 3, inner, h - 6, (h - 6) / 2, true);
  }
}

// Outlined chip with centered text (weak-area tags). Returns the x after
// the chip including its trailing gap.
inline int chip(Canvas& c, int x, int y, const char* text) {
  const LgFont* ui = fontByRole(FontRole::UI);
  const int tw = c.textWidth(ui, text);
  const int chH = ui->advanceY + 10;
  c.roundFrame(x, y, tw + 20, chH, chH / 2, true, 2);
  c.drawText(x + 10, y + 5, ui, text);
  return x + tw + 20 + 10;
}

// Battery glyph + percent, right-aligned ending at xRight. Returns the new
// right edge (left of the widget).
inline int batteryWidget(Canvas& c, int xRight, int y, uint16_t pct) {
  const LgFont* ui = fontByRole(FontRole::UI);
  char txt[8];
  snprintf(txt, sizeof(txt), "%u%%", (unsigned)pct);
  const int tw = c.textWidth(ui, txt);
  c.drawText(xRight - tw, y + 2, ui, txt);

  // Icon: outline 30x14 + terminal nub + fill proportional to pct.
  const int iw = 30, ih = 14;
  const int ix = xRight - tw - 8 - iw - 4;
  c.frame(ix, y, iw, ih, true, 1);
  c.vline(ix + iw + 1, y + 4, y + ih - 5, true);
  const int fill = (iw - 6) * (pct > 100 ? 100 : pct) / 100;
  if (fill > 0) {
    c.rect(ix + 3, y + 3, fill, ih - 6, true);
  }
  return ix - 8;
}

// ---------------------------------------------------------------- dialog

// The one modal style: white rounded card with a clean 2px frame, title,
// up to two wrapped body lines, and OK/CANCEL pill buttons (OK is the
// filled primary, CANCEL the outlined secondary). No drop shadow: a solid
// offset block reads as a rendering glitch on 1-bpp e-ink.
inline void modal(Canvas& c, const Metrics& mt, const char* titleText,
                  const char* line1, const char* line2, const char* okText,
                  const char* cancelText) {
  const LgFont* title = fontByRole(FontRole::Title);
  const LgFont* body = fontByRole(FontRole::Body);
  const LgFont* ui = fontByRole(FontRole::UI);

  const int bw = (mt.w - 2 * mt.m < 560) ? (mt.w - 2 * mt.m) : 560;
  const int bh = c.portrait() ? 310 : 240;
  const int bx = (mt.w - bw) / 2;
  const int by = (mt.h - bh) / 2 - 10;

  c.fillWhite();  // the dialog owns the screen — no content noise behind it
  c.roundRect(bx, by, bw, bh, 20, false);  // opaque white card, clearly rounded
  c.roundFrame(bx, by, bw, bh, 20, true, 2);

  int y = by + 26;
  c.drawText(bx + 28, y, title, titleText);
  y += title->advanceY + 14;
  const int inner = bw - 56;
  const char* l[3];
  int lens[3];
  for (int pass = 0; pass < 2; pass++) {
    const char* text = pass ? line2 : line1;
    if (!text || !text[0]) continue;
    const int n = c.wrapText(body, text, inner, l, lens, 2);
    for (int i = 0; i < n; i++) {
      c.drawTextN(bx + 28, y, body, l[i], lens[i]);
      y += body->advanceY + 4;
    }
    y += 6;
  }

  const int pillH = ui->advanceY + 16;
  const int gap = 16;
  const int pillW = (bw - 56 - gap) / 2;
  int py = by + bh - pillH - 26;
  if (py < y + 14) py = y + 14;  // never overlap the body lines
  c.roundRect(bx + 28, py, pillW, pillH, pillH / 2, true);
  int tw = c.textWidth(ui, okText);
  c.drawTextInv(bx + 28 + (pillW - tw) / 2, py + 8, ui, okText);
  c.roundFrame(bx + 28 + pillW + gap, py, pillW, pillH, pillH / 2, true, 2);
  tw = c.textWidth(ui, cancelText);
  c.drawText(bx + 28 + pillW + gap + (pillW - tw) / 2, py + 8, ui, cancelText);
}

// ---------------------------------------------------------------- glyphs

// Named UTF-8 glyph literals — no raw escapes scattered over screens.
inline constexpr const char* kCheck = "\xE2\x9C\x93";   // ✓
inline constexpr const char* kCross = "\xE2\x9C\x97";   // ✗
inline constexpr const char* kArrowR = "\xE2\x86\x92";  // →
inline constexpr const char* kBullet = "\xE2\x80\xA2";  // •

}  // namespace chrome

