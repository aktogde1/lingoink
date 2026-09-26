#pragma once
// Boot card: full-frame static image drawn exactly once. One artwork serves
// both ends: the boot splash, and the card the panel sleeps on after
// power-off / auto-sleep (2026-09-26, per user: the standby image must be the
// same identity picture that is shown at boot). Pure drawing (no state, no
// input), so the host preview (tools/preview) renders it exactly like the
// firmware does.
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

}  // namespace bootcards
