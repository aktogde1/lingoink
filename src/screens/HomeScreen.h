#pragma once
// Home screen — the "one look" dashboard: level, today's progress, review
// backlog, and the mode menu.

#include "Screen.h"
#include "../ui/FontRegistry.h"
#include "../progress/ProgressStore.h"
#include "../course/CourseCatalog.h"

class HomeScreen : public Screen {
 public:
  enum class Action : uint8_t { None, Continue, Review, Progress };

  void bind(ProgressStore* progress, const CourseCatalog* catalog) {
    progress_ = progress;
    catalog_ = catalog;
  }

  Action takeAction() { Action a = action_; action_ = Action::None; return a; }

  void render(Canvas& c) override {
    const int M = cfg::MARGIN;
    const LgFont* title = fontByRole(FontRole::Title);
    const LgFont* ui = fontByRole(FontRole::UI);
    const LgFont* body = fontByRole(FontRole::Body);

    // Header: target language + level track.
    c.drawText(M, M + 4, title, "ENGLISH");
    char track[24];
    snprintf(track, sizeof(track), "%s %s B1", progress_->currentLevel,
             "\xE2\x86\x92"); // →
    int tw = c.textWidth(ui, track);
    c.drawText(c.width() - M - tw, M + 16, ui, track);

    // Today block.
    int y = M + title->advanceY + 18;
    char today[32];
    snprintf(today, sizeof(today), "Today  %u min", progress_->todayMinutes);
    c.drawText(M, y, body, today);
    y += body->advanceY + 6;
    float frac = progress_->goalMinutes
                     ? (float)progress_->todayMinutes / (float)progress_->goalMinutes
                     : 0;
    if (frac > 1) frac = 1;
    c.progressBar(M, y, 300, 22, frac);
    char pct[8];
    snprintf(pct, sizeof(pct), "%u%%", (unsigned)(frac * 100));
    c.drawText(M + 300 + 14, y + 2, body, pct);
    y += 46;

    // Menu.
    const int rowH = 44;
    int my = y + 10;
    for (int i = 0; i < itemCount(); i++) {
      const char* label = itemLabel(i);
      const bool sel = (i == selected_);
      if (sel) {
        c.rect(M - 12, my - 6, c.width() - 2 * M + 24, rowH - 6, true);
        c.drawTextInv(M, my, body, label);
      } else {
        c.drawText(M, my, body, label);
      }
      my += rowH;
    }

    // Footer.
    const LgFont* f = ui;
    char foot[96];
    uint16_t due = progress_->dueCount();
    if (!catalog_->course.lessonCount) {
      snprintf(foot, sizeof(foot), "NO COURSE — copy /courses to SD");
    } else {
      snprintf(foot, sizeof(foot), "%u due \xC2\xB7 streak %u \xC2\xB7 %s",
               (unsigned)due, (unsigned)progress_->streakDays, courseTitle());
    }
    c.hline(M, c.height() - 34, c.width() - M, true);
    c.drawText(M, c.height() - 28, f, foot);
  }

  Nav handleKey(Key k) override {
    const int n = itemCount();
    switch (k) {
      case Key::Up:
      case Key::Left:
        if (selected_ > 0) selected_--;
        return Nav::RedrawFast;
      case Key::Down:
      case Key::Right:
        if (selected_ < n - 1) selected_++;
        return Nav::RedrawFast;
      case Key::Ok:
        switch (selected_) {
          case 0: action_ = Action::Continue; break;
          case 1: action_ = Action::Review; break;
          case 2: action_ = Action::Progress; break;
          default: break;
        }
        return Nav::RedrawFull;
      default:
        return Nav::Stay;
    }
  }

 private:
  int itemCount() const {
    return progress_->dueCount() > 0 ? 3 : 3; // CONTINUE / REVIEW / PROGRESS
  }

  const char* itemLabel(int i) {
    static char reviewLabel[24];
    if (i == 1) {
      snprintf(reviewLabel, sizeof(reviewLabel), "REVIEW %u",
               (unsigned)progress_->dueCount());
      return reviewLabel;
    }
    if (i == 2) return "PROGRESS";
    return "CONTINUE";
  }

  const char* courseTitle() const {
    return catalog_->course.title[0] ? catalog_->course.title : "course";
  }

  ProgressStore* progress_ = nullptr;
  const CourseCatalog* catalog_ = nullptr;
  int selected_ = 0;
  Action action_ = Action::None;
};
