#pragma once
// Minimal 1-bpp BMP writer for the preview tool. The Canvas buffer is always
// panel-native 800x480; this writer applies the orientation transform to
// produce the LOGICAL image the user actually sees (800x480 or 480x800).

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "ui/Canvas.h"

namespace bmp {

inline bool writeScene(const std::string& path, const Canvas& c) {
  const int pw = 800, ph = 480;  // panel-native, see config.h
  const int W = c.width();
  const int H = c.height();
  const int orient = (int)c.orientation();
  const uint8_t* bits = c.bits();
  const int stride = (pw + 7) / 8;

  auto panelPx = [&](int x, int y) -> int {
    return (bits[y * stride + (x >> 3)] >> (7 - (x & 7))) & 1;  // 1 = white
  };
  // panel = f(logical) per Canvas::setPixel; invert the mapping here.
  auto logicalPx = [&](int x, int y) -> int {
    switch (orient) {
      case 1: return panelPx(pw - 1 - y, x);
      case 2: return panelPx(pw - 1 - x, ph - 1 - y);
      case 3: return panelPx(y, ph - 1 - x);
      default: return panelPx(x, y);
    }
  };

  const int rowBytes = (W + 7) / 8;          // 800->100, 480->60 (both %4==0)
  const int paletteBytes = 8;
  const uint32_t dataOffset = 14 + 40 + paletteBytes;
  const uint32_t fileSize = dataOffset + (uint32_t)(rowBytes * H);

  std::FILE* f = fopen(path.c_str(), "wb");
  if (!f) return false;

  uint8_t hdr[14] = {0};
  hdr[0] = 'B'; hdr[1] = 'M';
  memcpy(hdr + 2, &fileSize, 4);
  uint32_t off = dataOffset;
  memcpy(hdr + 10, &off, 4);
  fwrite(hdr, 1, 14, f);

  uint8_t info[40] = {0};
  int32_t w32 = W, h32 = H;
  uint16_t planes = 1, bpp = 1;
  uint32_t sz = 40, imgSize = (uint32_t)(rowBytes * H), used = 2, important = 2;
  int32_t ppm = 2835;
  memcpy(info + 0, &sz, 4);
  memcpy(info + 4, &w32, 4);
  memcpy(info + 8, &h32, 4);   // positive height -> bottom-up rows
  memcpy(info + 12, &planes, 2);
  memcpy(info + 14, &bpp, 2);
  memcpy(info + 20, &imgSize, 4);
  memcpy(info + 24, &ppm, 4);
  memcpy(info + 28, &ppm, 4);
  memcpy(info + 32, &used, 4);
  memcpy(info + 36, &important, 4);
  fwrite(info, 1, 40, f);

  const uint8_t palette[8] = {0, 0, 0, 0, 255, 255, 255, 0};  // 0=black, 1=white
  fwrite(palette, 1, paletteBytes, f);

  std::vector<uint8_t> row(rowBytes);
  for (int y = H - 1; y >= 0; y--) {  // bottom-up
    memset(row.data(), 0, rowBytes);
    for (int x = 0; x < W; x++) {
      if (logicalPx(x, y)) row[x >> 3] |= (uint8_t)(0x80 >> (x & 7));
    }
    fwrite(row.data(), 1, rowBytes, f);
  }
  fclose(f);
  return true;
}

}  // namespace bmp
