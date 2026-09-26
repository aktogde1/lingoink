#pragma once
// Home screen — the main menu: five labelled entries, each with a one-line
// subtitle explaining what it does. LEARN jumps straight into the next
// lesson; REVIEW runs the due queue; LESSONS opens the course table of
// contents. Honest info only: the due count lives in the REVIEW subtitle,
// the header carries the language / level track from the course manifest.
// All five rows are on screen at once — no scroll window; the inter-row
// gap is computed from the height left under the header (chrome:: provides
// the shared visual language). The selection moves cyclically (last ->
// first). The battery lives on the power-off card; the footer is gone.

#include "Screen.h"
#include "../ui/Chrome.h"
#include "../ui/FontRegistry.h"
#include "../ui/Strings.h"
#include "../progress/ProgressStore.h"
#include "../course/CourseCatalog.h"

#include <Arduino.h>

class HomeScreen : public Screen {
 public:
  enum class Action : uint8_t { None, Learn, Review, Lessons, Progress, Settings };

  void bind(ProgressStore* progress, const CourseCatalog* catalog) {
    progress_ = progress;
    catalog_ = catalog;
  }

  Action takeAction() { Action a = action_; action_ = Action::None; return a; }

  void render(Canvas& c) override {
    const chrome::Metrics mt = chrome::metrics(c);

    // Header: target language (first word of the course title) + level
    // track from the manifest, falling back to the stored current level.
    char head[24];
    headerTitle(head, sizeof(head));
    char track[24];
    if (catalog_->course.levelFrom[0] && catalog_->course.levelTo[0]) {
      snprintf(track, sizeof(track), S(TrackFmt), catalog_->course.levelFrom,
               catalog_->course.levelTo);
    } else {
      snprintf(track, sizeof(track), "%s", progress_->currentLevel);
    }
    chrome::header(c, mt, head, track);

    // Subtitles are fixed per row; the selected one may wrap to two lines.
    char subs[ITEM_COUNT][96];
    for (int i = 0; i < ITEM_COUNT; i++) {
      itemSubtitle(i, subs[i], sizeof(subs[i]));
    }

    // Layout: five rows must land between top and bottom in every
    // orientation. Prefer the default in-row gap and a two-line selected
    // subtitle; when the height runs out, drop the expansion first, then
    // shrink the in-row gap. Rows never overlap either way — the selected
    // card hugs its text block and every row advances by its own height.
    const LgFont* ui = fontByRole(FontRole::UI);
    const char* sl[2];
    int slens[2];
    int selLines =
        c.wrapText(ui, subs[selected_], mt.w - 2 * mt.m, sl, slens, 2) > 1 ? 2
                                                                          : 1;
    const int avail = mt.bottom - mt.top;
    // The selection card pads the text block by SEL_PAD_Y above and below,
    // so every inter-row gap must clear the card, not just the text.
    const int minGap = cfg::SEL_PAD_Y + cfg::MENU_GAP_MIN;
    int subGap = cfg::MENU_SUB_GAP;
    int rowGap = rowGapFor(selLines, subGap, avail);
    if (rowGap < minGap && selLines == 2) {
      selLines = 1;
      rowGap = rowGapFor(selLines, subGap, avail);
    }
    if (rowGap < minGap) {
      const int base = chrome::menuRowHeight(1, 0);
      subGap = (avail - base * ITEM_COUNT -
                (ITEM_COUNT - 1) * minGap) / ITEM_COUNT;
      if (subGap < 0) subGap = 0;
      rowGap = rowGapFor(selLines, subGap, avail);
      if (rowGap < minGap) rowGap = minGap;
    }
    if (rowGap > cfg::MENU_GAP_MAX) rowGap = cfg::MENU_GAP_MAX;

    int y = mt.top;
    for (int i = 0; i < ITEM_COUNT; i++) {
      const int lines = (i == selected_) ? selLines : 1;
      chrome::menuRow(c, mt, y, itemLabel(i), subs[i], i == selected_, subGap);
      y += chrome::menuRowHeight(lines, subGap) + rowGap;
    }
  }

  Nav handleKey(Key k) override {
    switch (k) {
      case Key::Up:
      case Key::Left:
        selected_ = (selected_ + ITEM_COUNT - 1) % ITEM_COUNT;  // cyclic
        return Nav::RedrawFast;
      case Key::Down:
      case Key::Right:
        selected_ = (selected_ + 1) % ITEM_COUNT;  // cyclic
        return Nav::RedrawFast;
      case Key::Ok:
        switch (selected_) {
          case 0: action_ = Action::Learn; break;
          case 1: action_ = Action::Review; break;
          case 2: action_ = Action::Lessons; break;
          case 3: action_ = Action::Progress; break;
          case 4: action_ = Action::Settings; break;
          default: break;
        }
        return Nav::RedrawFull;
      default:
        return Nav::Stay;  // Back on Home does nothing (top level)
    }
  }

 private:
  static const int ITEM_COUNT = 5;  // LEARN / REVIEW / LESSONS / PROGRESS / SETTINGS

  // Inter-row gap left when the menu draws ITEM_COUNT rows with `selLines`
  // subtitle lines on the selected one and the given in-row sub gap.
  // Negative means the combination does not fit the content area.
  int rowGapFor(int selLines, int subGap, int avail) const {
    const int rows = chrome::menuRowHeight(1, subGap) * (ITEM_COUNT - 1) +
                     chrome::menuRowHeight(selLines, subGap);
    return (avail - rows) / (ITEM_COUNT - 1);
  }

  // First word of the course title, uppercased ("English A2 → B1" ->
  // "ENGLISH"); falls back to the localized generic label.
  void headerTitle(char* buf, size_t n) const {
    const char* t = catalog_->course.title;
    if (!t[0]) {
      snprintf(buf, n, "%s", S(HomeLang));
      return;
    }
    size_t w = 0;
    while (t[w] && t[w] != ' ' && w < n - 1) w++;
    memcpy(buf, t, w);
    buf[w] = 0;
    for (char* p = buf; *p; p++) {
      if (*p >= 'a' && *p <= 'z') *p = (char)(*p - 'a' + 'A');
    }
  }

  const char* itemLabel(int i) {
    switch (i) {
      case 0: return S(MiLearn);
      case 1: return U("REVIEW", "ПОВТОРИТЬ");
      case 2: return S(MiLessons);
      case 3: return S(MiProgress);
      default: return S(MiSettings);
    }
  }

  void itemSubtitle(int i, char* buf, size_t n) {
    buf[0] = 0;
    switch (i) {
      case 0: {
        const LessonMeta* m = nextLesson();
        const uint16_t total = catalog_->course.lessonCount;
        if(m && progress_->session.lesson[0]) {
          snprintf(buf,n,U("Resume: %s","С места остановки: %s"),m->title);
        } else if (m && total) {
          const int idx = catalog_->indexOf(m->id);
          snprintf(buf, n, S(MiLearnSubFmt),
                   m->title[0] ? m->title : m->id,
                   (unsigned)((idx >= 0 ? idx : 0) + 1), (unsigned)total);
        } else {
          snprintf(buf, n, "%s", catalog_->course.lessonCount ? U("Course viewed. Practise weak areas.","Курс просмотрен. Закрепите слабые темы.") : S(NoCourse));
        }
        break;
      }
      case 1: snprintf(buf,n,U("%u items due; confirm today's date","%u к повторению; проверьте дату"),progress_->dueCount());break;
      case 2: snprintf(buf, n, "%s", S(MiLessonsSub)); break;
      case 3: snprintf(buf, n, "%s", S(MiProgressSub)); break;
      default: snprintf(buf, n, "%s", S(MiSettingsSub)); break;
    }
  }

  const LessonMeta* nextLesson() const {
    return progress_->nextLesson(*catalog_);
  }

  ProgressStore* progress_ = nullptr;
  const CourseCatalog* catalog_ = nullptr;
  int selected_ = 0;
  Action action_ = Action::None;
};
