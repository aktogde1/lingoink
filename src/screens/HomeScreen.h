#pragma once
// Home screen — the main menu: four labelled entries, each with a one-line
// subtitle explaining what it does. LEARN jumps straight into the next
// lesson; LESSONS opens the course table of contents. Honest info only:
// due count (hidden while zero), course, battery. The header language /
// level track comes from the course manifest; chrome:: provides the shared
// visual language. The list scrolls cyclically (last -> first).

#include "Screen.h"
#include "../ui/Chrome.h"
#include "../ui/FontRegistry.h"
#include "../ui/Strings.h"
#include "../progress/ProgressStore.h"
#include "../course/CourseCatalog.h"

#include <Arduino.h>
#include <BatteryMonitor.h>

class HomeScreen : public Screen {
 public:
  enum class Action : uint8_t { None, Learn, Review, Lessons, Progress, Settings };

  void bind(ProgressStore* progress, const CourseCatalog* catalog,
            BatteryMonitor* battery) {
    progress_ = progress;
    catalog_ = catalog;
    battery_ = battery;
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

    // Menu: four two-line cards with rounded selection, breathing room.
    const int gap = 10;
    const int rowH = mt.menuRowH;
    const int block = rowH + gap;
    int y = mt.top + 8;
    for (int i = top_; i < ITEM_COUNT && i < top_+4; i++) {
      char sub[96];
      itemSubtitle(i, sub, sizeof(sub));
      chrome::menuRow(c, mt, y, itemLabel(i), sub, i == selected_);
      y += block;
    }

    // Bottom-left: honest info. Due count only when it is non-zero (the
    // day counter grows with usage — showing a permanent 0 reads broken).
    // The title yields (ellipsis) when the battery widget needs the corner.
    char foot[96];
    const uint16_t due = progress_->dueCount();
    if (!catalog_->course.lessonCount) {
      snprintf(foot, sizeof(foot), "%s", S(NoCourse));
    } else if (due > 0) {
      snprintf(foot, sizeof(foot), S(DueCourseFmt), (unsigned)due,
               courseTitle());
    } else {
      snprintf(foot, sizeof(foot), "%s", courseTitle());
    }
    const bool showBat = refreshBattery();
    const LgFont* ui = fontByRole(FontRole::UI);
    char fit[96];
    chrome::fitText(c, ui, foot, mt.w - 2 * mt.m - (showBat ? 120 : 0), fit,
                    sizeof(fit));
    c.drawText(mt.m, mt.widgetY, ui, fit);

    if (showBat) {
      chrome::batteryWidget(c, mt.w - mt.m, mt.widgetY - 2, batPct_);
    }
  }

  Nav handleKey(Key k) override {
    switch (k) {
      case Key::Up:
      case Key::Left:
        selected_ = (selected_ + ITEM_COUNT - 1) % ITEM_COUNT;  // cyclic
        top_=selected_>=4?1:0;
        return Nav::RedrawFast;
      case Key::Down:
      case Key::Right:
        selected_ = (selected_ + 1) % ITEM_COUNT;  // cyclic
        top_=selected_>=4?1:0;
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
  static const int ITEM_COUNT = 5;  // LEARN / LESSONS / PROGRESS / SETTINGS

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

  const char* courseTitle() const {
    if (catalog_->course.title[0]) return catalog_->course.title;
    if (catalog_->course.id[0]) return catalog_->course.id;
    return S(FallbackCourse);
  }

  // Reads the battery at most once per 5 s (ADC reads are cheap but the
  // displayed value should not flicker between cursor moves). Returns false
  // when the board has no usable telemetry.
  bool refreshBattery() {
    if (!battery_) return false;
    const uint32_t now = millis();
    if (batValid_ && now - batLastMs_ < 5000) return batValid_;
    batLastMs_ = now;
    uint16_t pct = 0;
    batValid_ = battery_->readPercentageChecked(pct);
    if (batValid_) batPct_ = pct;
    return batValid_;
  }

  ProgressStore* progress_ = nullptr;
  const CourseCatalog* catalog_ = nullptr;
  BatteryMonitor* battery_ = nullptr;
  int selected_ = 0, top_=0;
  Action action_ = Action::None;
  bool batValid_ = false;
  uint16_t batPct_ = 0;
  uint32_t batLastMs_ = 0;
};
