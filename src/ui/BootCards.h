#pragma once
// Boot cards: full-frame static images drawn exactly once. One artwork serves
// both ends: the boot splash, and the card the panel sleeps on after
// power-off / auto-sleep (2026-09-26, per user: the standby image must be the
// same identity picture that is shown at boot) with a small charge line at
// the bottom. Pure drawing (no state, no input), so the host preview
// (tools/preview) renders them exactly like the firmware does.
//
// Uses the Display font for the poster look; the Cyrillic subtitles double as
// an early font check on real hardware.

#include "Canvas.h"
#include "Chrome.h"
#include "FontRegistry.h"
#include "Strings.h"

namespace bootcards {

inline void drawSplash(Canvas& c) {
  const LgFont* disp = fontByRole(FontRole::Display);
  const LgFont* ui = fontByRole(FontRole::UI);

  int w = c.textWidth(disp, S(AppName));
  int y = c.height() / 2 - disp->advanceY / 2 - 34;
  c.drawText((c.width() - w) / 2, y, disp, S(AppName));
  y += disp->advanceY + 6;
  // Short centered rule as a divider between wordmark and subtitles.
  c.hline((c.width() - 120) / 2, (c.width() + 120) / 2, y);
  y += 18;
  w = c.textWidth(ui, S(SplashSub));
  c.drawText((c.width() - w) / 2, y, ui, S(SplashSub));
  y += ui->advanceY + 8;
  w = c.textWidth(ui, S(SplashTag));
  c.drawText((c.width() - w) / 2, y, ui, S(SplashTag));
}

// The power-off / auto-sleep card: the splash artwork plus a small centered
// charge line ("Заряд N%") at the bottom. chargePct < 0 (or > 100) means the
// measurement was unavailable — the line is skipped, never faked. The panel
// keeps showing this image through deep sleep; nothing refreshes until the
// next boot.
inline void drawPowerOff(Canvas& c, int chargePct) {
  drawSplash(c);
  if (chargePct < 0 || chargePct > 100) return;
  const LgFont* ui = fontByRole(FontRole::UI);
  char line[24];
  snprintf(line, sizeof(line), S(ChargeFmt), (unsigned)chargePct);
  const int w = c.textWidth(ui, line);
  c.drawText((c.width() - w) / 2, c.height() - c.margin() - ui->advanceY, ui,
             line);
}

}  // namespace bootcards
