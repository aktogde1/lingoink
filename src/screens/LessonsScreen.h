#pragma once
// Lessons screen — the course table of contents, grouped by day
// ("День 1…7"). Columns: right-aligned number, title, status mark at the
// right edge (✓ done / • now). The list scrolls cyclically (last -> first).
// A long title expands to two wrapped lines while its row is selected —
// no marquee (e-ink has no partial updates), the full text simply shows.

#include "Screen.h"
#include "../ui/Chrome.h"
#include "../ui/Canvas.h"
#include "../ui/FontRegistry.h"
#include "../ui/Strings.h"
#include "../course/CourseCatalog.h"
#include "../progress/ProgressStore.h"

class LessonsScreen : public Screen {
 public:
  void bind(ProgressStore* progress, const CourseCatalog* catalog,
            Canvas* canvas) {
    progress_ = progress;
    catalog_ = catalog;
    canvas_ = canvas;
  }

  // Lesson index confirmed with OK since the last call, or -1.
  int takeLesson() {
    int i = chosen_;
    chosen_ = -1;
    return i;
  }

  void render(Canvas& c) override {
    const chrome::Metrics mt = chrome::metrics(c);
    const int total = catalog_->course.lessonCount;
    if (total == 0) {
      chrome::header(c, mt, S(LsTitle));
      c.drawText(mt.m, mt.top + 4, fontByRole(FontRole::Body), S(NoCourse));
      return;
    }

    char head[32];
    snprintf(head, sizeof(head), S(LessonOfFmt),
             (unsigned)(selected_ + 1), (unsigned)total);
    chrome::header(c, mt, S(LsTitle), head);

    const char* level = "";
    int y = mt.top + 2;
    int shown = 0;
    int prev = -1;
    for (int i = topIdx_; i < total; i++) {
      // Day-group header whenever the level changes (always shown).
      const char* lv = catalog_->course.lessons[i].level;
      if (lv[0] && strcmp(lv, level) != 0) {
        const int headH = groupHeaderH();
        if (y + headH + mt.rowH > mt.bottom) break;
        y += 8;  // breathing room before a new group
        c.drawText(mt.m, y, fontByRole(FontRole::UI), lv);
        y += headH;
        level = lv;
      }
      const int h = rowHeight(c, mt, i);
      if (y + h > mt.bottom && shown > 0) break;
      // Light separator inside a group, away from the selection card.
      if (prev >= 0 && prev != selected_ && i != selected_ &&
          sameLevel(prev, i)) {
        chrome::separator(c, mt, y - 10);
      }
      drawLessonRow(c, mt, i, y, h);
      y += h;
      shown++;
      prev = i;
    }

    chrome::scrollHints(c, mt, total - (topIdx_ + shown), topIdx_ > 0);
  }

  Nav handleKey(Key k) override {
    const int total = catalog_->course.lessonCount;
    if (total == 0) {
      if (k == Key::Ok || k == Key::Back) return Nav::Done;
      return Nav::Stay;
    }
    switch (k) {
      case Key::Up:
      case Key::Left:
        selected_ = (selected_ + total - 1) % total;  // cyclic
        ensureVisible();
        return Nav::RedrawFast;
      case Key::Down:
      case Key::Right:
        selected_ = (selected_ + 1) % total;  // cyclic
        ensureVisible();
        return Nav::RedrawFast;
      case Key::Ok:
        chosen_ = selected_;
        return Nav::Done;
      case Key::Back:
        return Nav::Done;
      default:
        break;
    }
    return Nav::Stay;
  }

 private:
  int groupHeaderH() const {
    return fontByRole(FontRole::UI)->advanceY + 8;
  }

  bool sameLevel(int a, int b) const {
    const char* la = catalog_->course.lessons[a].level;
    const char* lb = catalog_->course.lessons[b].level;
    if (!la[0] || !lb[0]) return true;  // ungrouped lessons share the flow
    return strcmp(la, lb) == 0;
  }

  // Row height: the selected row grows by one line when its title wraps.
  int rowHeight(const Canvas& c, const chrome::Metrics& mt, int i) const {
    const LgFont* body = fontByRole(FontRole::Body);
    if (i != selected_) return mt.rowH;
    const LessonMeta& m = catalog_->course.lessons[i];
    const char* title = m.title[0] ? m.title : m.id;
    const int maxW = titleWrapW(c, mt);
    const char* l[2];
    int lens[2];
    const int n = c.wrapText(body, title, maxW, l, lens, 2);
    return (n <= 1) ? mt.rowH : mt.rowH + body->advanceY;
  }

  int titleWrapW(const Canvas& c, const chrome::Metrics& mt) const {
    // Between the number column and the status mark.
    const int numX = mt.m + c.textWidth(fontByRole(FontRole::UI), "88") + 4;
    return mt.w - mt.m - 60 - (numX + 18);
  }

  void drawLessonRow(Canvas& c, const chrome::Metrics& mt, int i, int y,
                     int h) {
    const LgFont* body = fontByRole(FontRole::Body);
    const LgFont* ui = fontByRole(FontRole::UI);
    const LgFont* bold = fontByRole(FontRole::BodyBold);
    const LessonMeta& m = catalog_->course.lessons[i];

    const bool sel = (i == selected_);
    // Text block: one title line, or two when the selected row expanded.
    const int textH = (sel && h > mt.rowH) ? 2 * body->advanceY - 4
                                           : body->advanceY - 2;
    if (sel) chrome::selection(c, mt, y, textH);

    char num[8];
    snprintf(num, sizeof(num), "%u", (unsigned)(i + 1));
    const int numX = mt.m + c.textWidth(ui, "88") + 4;
    const int textX = numX + 18;
    if (sel) chrome::drawRightInv(c, numX, y, ui, num);
    else chrome::drawRight(c, numX, y, ui, num);

    const char* title = m.title[0] ? m.title : m.id;
    const int maxW = mt.w - mt.m - 60 - textX;
    if (sel) {
      // Selected: up to 2 wrapped lines — the whole title is visible.
      const char* l[2];
      int lens[2];
      const int n = c.wrapText(body, title, maxW, l, lens, 2);
      for (int j = 0; j < n; j++) {
        c.drawTextInv(textX, y + j * body->advanceY, body, l[j], lens[j]);
      }
    } else {
      char buf[64];
      chrome::fitText(c, body, title, maxW, buf, sizeof(buf));
      c.drawText(textX, y, body, buf);
    }

    // Status at the right edge of the first line: ✓ done / • now.
    if (progress_->stage(m.id)>0) {
      if (sel) c.drawTextInv(c.width() - mt.m - 12, y, bold, progress_->stage(m.id)>=2 ? chrome::kCheck : "~");
      else c.drawText(c.width() - mt.m - 12, y, bold, progress_->stage(m.id)>=2 ? chrome::kCheck : "~");
      return;
    }
    const LessonMeta* next = progress_->nextLesson(*catalog_);
    if (next && next == &m) {
      if (sel) chrome::drawRightInv(c, c.width() - mt.m, y, ui, S(LsNow));
      else chrome::drawRight(c, c.width() - mt.m, y, ui, S(LsNow));
    }
  }

  // Lessons visible from `from`, taking group headers and the expanded
  // selected row into account (>= 1). Mirrors render()'s loop.
  int visibleFrom(int from) const {
    const chrome::Metrics mt = chrome::metrics(*canvas_);
    int y = mt.top + 2;
    int shown = 0;
    const char* level = "";
    for (int i = from; i < catalog_->course.lessonCount; i++) {
      const char* lv = catalog_->course.lessons[i].level;
      if (lv[0] && strcmp(lv, level) != 0) {
        y += 8;
        y += groupHeaderH();
        level = lv;
      }
      const int h = (i == selected_) ? rowHeight(*canvas_, mt, i) : mt.rowH;
      if (y + h > mt.bottom && shown > 0) break;
      y += h;
      shown++;
    }
    return shown > 0 ? shown : 1;
  }

  void ensureVisible() {
    // Cyclic: a wrap to the other end recentres the window on that end.
    if (selected_ == 0) {
      topIdx_ = 0;
      return;
    }
    if (selected_ == catalog_->course.lessonCount - 1) {
      // Scroll far enough that the last lesson is the bottom row.
      while (selected_ > topIdx_ &&
             !rowOnScreen(catalog_->course.lessonCount - 1)) {
        topIdx_++;
      }
      return;
    }
    while (selected_ < topIdx_) topIdx_--;
    while (selected_ >= topIdx_ + visibleFrom(topIdx_)) topIdx_++;
  }

  // Whether row `i` fits on screen when the window starts at topIdx_.
  bool rowOnScreen(int i) const {
    const chrome::Metrics mt = chrome::metrics(*canvas_);
    int y = mt.top + 2;
    const char* level = "";
    for (int k = topIdx_; k <= i; k++) {
      const char* lv = catalog_->course.lessons[k].level;
      if (lv[0] && strcmp(lv, level) != 0) {
        y += 8 + groupHeaderH();
        level = lv;
      }
      const int h = (k == selected_) ? rowHeight(*canvas_, mt, k) : mt.rowH;
      if (y + h > mt.bottom) return false;
      y += h;
    }
    return true;
  }

  ProgressStore* progress_ = nullptr;
  const CourseCatalog* catalog_ = nullptr;
  Canvas* canvas_ = nullptr;  // set by App so key handling can measure layout
  int selected_ = 0;
  int topIdx_ = 0;
  int chosen_ = -1;
};
