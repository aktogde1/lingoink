#pragma once
// Progress screen: skill bars + weak areas + review backlog.

#include "Screen.h"
#include "../ui/FontRegistry.h"
#include "../progress/ProgressStore.h"

class ProgressScreen : public Screen {
 public:
  void bind(ProgressStore* progress) { progress_ = progress; }

  void render(Canvas& c) override {
    const int M = cfg::MARGIN;
    const LgFont* title = fontByRole(FontRole::Title);
    const LgFont* body = fontByRole(FontRole::Body);
    const LgFont* ui = fontByRole(FontRole::UI);

    char head[24];
    snprintf(head, sizeof(head), "LEVEL %s", progress_->currentLevel);
    c.drawText(M, M + 4, title, head);

    int y = M + title->advanceY + 20;
    for (int s = 0; s < (int)Skill::Count; s++) {
      const Stat& st = progress_->mastery.skills[s];
      c.drawText(M, y, body, skillName((Skill)s));
      char pct[8];
      snprintf(pct, sizeof(pct), "%u%%", st.percent());
      int pw = c.textWidth(body, pct);
      c.drawText(c.width() - M - 220 - 14 - pw, y, body, pct);
      c.progressBar(c.width() - M - 220, y + 4, 200, 18,
                    st.count ? st.ema : 0);
      y += body->advanceY + 18;
    }

    y += 16;
    c.drawText(M, y, body, "Weak areas");
    y += body->advanceY + 6;
    const char* weak[3];
    int n = progress_->mastery.weakTags(weak, 3);
    if (n == 0) {
      c.drawText(M + 20, y, ui, "none yet \xE2\x80\x94 keep practicing");
      y += ui->advanceY + 4;
    } else {
      for (int i = 0; i < n; i++) {
        c.drawText(M + 20, y, ui, weak[i]);
        y += ui->advanceY + 4;
      }
    }

    y += 10;
    char foot[64];
    snprintf(foot, sizeof(foot), "%u items due \xC2\xB7 streak %u days",
             (unsigned)progress_->dueCount(), (unsigned)progress_->streakDays);
    c.drawText(M, y, body, foot);

    c.hline(M, c.width() - M, c.height() - 34, true);
    c.drawText(M, c.height() - 28, ui, "OK \xE2\x80\x94 back");
  }

  Nav handleKey(Key k) override {
    if (k == Key::Ok || k == Key::Back) return Nav::Done;
    return Nav::Stay;
  }

 private:
  ProgressStore* progress_ = nullptr;
};
