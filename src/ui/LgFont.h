#pragma once
// LingoInk bitmap font format (1 bpp).
//
// A font is three flat arrays: packed glyph bitmaps (rows padded to whole
// bytes, MSB-first, bit set = black pixel), a glyph record per codepoint, and
// a sorted interval table that maps unicode ranges to glyph indices. Lookup
// is a binary search over intervals plus a direct index — no hashing, no
// dynamic allocation. Bitmaps live in flash (const).

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  uint8_t width;       // bitmap width in pixels (0 = blank glyph, e.g. space)
  uint8_t height;      // bitmap height in pixels
  uint8_t advanceX;    // cursor advance after this glyph
  int8_t left;         // bearing: x from cursor to bitmap left edge
  int8_t top;          // bearing: y from baseline to bitmap top edge
  uint16_t dataLength; // bitmap bytes for this glyph
  uint32_t dataOffset; // byte offset into the font's bitmap array
} LgGlyph;

typedef struct {
  uint32_t first; // first unicode codepoint of the interval
  uint32_t last;  // last unicode codepoint of the interval
  uint32_t offset; // glyph index of `first` in the glyph array
} LgInterval;

typedef struct {
  const char* name;
  const uint8_t* bitmap;
  const LgGlyph* glyphs;
  uint16_t glyphCount;
  const LgInterval* intervals;
  uint16_t intervalCount;
  uint8_t advanceY;  // baseline-to-baseline distance
  int8_t ascender;   // pixels above baseline
  int8_t descender;  // pixels below baseline (negative)
} LgFont;

// Returns the glyph for a codepoint, or nullptr when the font lacks it.
inline const LgGlyph* lgFontGlyph(const LgFont* f, uint32_t cp) {
  uint16_t lo = 0, hi = f->intervalCount;
  while (lo < hi) {
    uint16_t mid = (uint16_t)((lo + hi) / 2);
    const LgInterval* iv = &f->intervals[mid];
    if (cp < iv->first) {
      hi = mid;
    } else if (cp > iv->last) {
      lo = (uint16_t)(mid + 1);
    } else {
      uint32_t idx = iv->offset + (cp - iv->first);
      if (idx >= f->glyphCount) {
        return nullptr;
      }
      return &f->glyphs[idx];
    }
  }
  return nullptr;
}

#ifdef __cplusplus
}
#endif
