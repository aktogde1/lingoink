#pragma once
// App: event loop + screen state machine.
//
//   Splash → Home ⇄ Lesson / Progress
//
// Everything is statically allocated (no heap in steady state). The loop
// polls input, dispatches one key per iteration, redraws on Nav results and
// auto-sleeps after AUTO_SLEEP_MINUTES of inactivity.

#include "../ui/Canvas.h"
#include "../ui/Presenter.h"
#include "../ui/FontRegistry.h"
#include "../input/InputLoop.h"
#include "../course/CourseCatalog.h"
#include "../progress/ProgressStore.h"
#include "../screens/HomeScreen.h"
#include "../screens/LessonScreen.h"
#include "../screens/ProgressScreen.h"

#include <EInkDisplay.h>
#include <PowerManager.h>
#include <BatteryMonitor.h>
#include <Arduino.h>
#include <SD.h>
#include <SPI.h>

class App {
 public:
  App(EInkDisplay& display)
      : canvas_(), presenter_(display), home_(), lesson_(), progressScreen_() {}

  void run();

 private:
  enum class Where : uint8_t { Splash, Home, Lesson, Progress };

  void showSplash();
  void goHome(bool full);
  bool startLesson(const LessonMeta& meta);
  void finishLesson();

  Canvas canvas_;
  Presenter presenter_;
  HomeScreen home_;
  LessonScreen lesson_;
  ProgressScreen progressScreen_;

  CourseCatalog catalog_;
  ProgressStore progress_;
  InputLoop input_;

  Where where_ = Where::Splash;
  uint32_t lastMinutesTick_ = 0;
  bool sdOk_ = false;
};

void App::run() {
  presenter_.begin();
  input_.begin();
  lastMinutesTick_ = millis();

  // Splash: logo + boot diagnostics for 1.2s.
  canvas_.init(cfg::SCREEN_W, cfg::SCREEN_H);
  showSplash();
  delay(1200);

  // Progress from SD (creates /lingoink when missing).
  sdOk_ = SD.begin(12, SPI, 25000000, "/sd", 8);
  if (sdOk_) {
    progress_.begin();
    progress_.advanceDay(0); // uptime carry happens on save cycles
    catalog_.begin();
    if (catalog_.course.id[0]) {
      strncpy(progress_.courseId, catalog_.course.id, sizeof(progress_.courseId) - 1);
    }
  }
  home_.bind(&progress_, &catalog_);
  progressScreen_.bind(&progress_);
  goHome(true);

  while (true) {
    const uint32_t now = millis();

    // Track active minutes for the daily goal / day counter.
    if (now - lastMinutesTick_ >= 60000) {
      lastMinutesTick_ = now;
      progress_.addMinutes(1);
    }

    const Key k = input_.poll(now);

    // Power handling is global.
    if (k == Key::Power || input_.idleMs(now) > cfg::AUTO_SLEEP_MINUTES * 60000UL) {
      if (sdOk_) progress_.save();
      freeink::PowerManager::powerDownRailsForSleep();
      freeink::PowerManager::deepSleepUntilPowerButton();
    }

    if (k == Key::None) {
      delay(15);
      continue;
    }

    switch (where_) {
      case Where::Home: {
        Nav nav = home_.handleKey(k);
        HomeScreen::Action a = home_.takeAction();
        if (a == HomeScreen::Action::Continue) {
          const LessonMeta* m = catalog_.nextAfter(progress_.lastLessonId);
          if (m) {
            if (startLesson(*m)) where_ = Where::Lesson;
          }
        } else if (a == HomeScreen::Action::Review) {
          // MVP: review runs the next lesson too; a dedicated due-item
          // drill screen lands with the full SRS milestone.
          const LessonMeta* m = catalog_.nextAfter(progress_.lastLessonId);
          if (m) {
            if (startLesson(*m)) where_ = Where::Lesson;
          }
        } else if (a == HomeScreen::Action::Progress) {
          where_ = Where::Progress;
          progressScreen_.render(canvas_);
          presenter_.present(canvas_, Refresh::Full);
        } else if (nav == Nav::RedrawFast) {
          home_.render(canvas_);
          presenter_.present(canvas_, Refresh::Fast);
        } else if (nav == Nav::RedrawFull) {
          goHome(true);
        }
        break;
      }

      case Where::Lesson: {
        Nav nav = lesson_.handleKey(k);
        if (lesson_.finished() && (nav == Nav::Done || k == Key::Back)) {
          finishLesson();
        } else if (nav == Nav::RedrawFast) {
          lesson_.render(canvas_);
          presenter_.present(canvas_, Refresh::Fast);
        } else if (nav == Nav::RedrawFull) {
          lesson_.render(canvas_);
          presenter_.present(canvas_, Refresh::Balanced);
        }
        break;
      }

      case Where::Progress: {
        Nav nav = progressScreen_.handleKey(k);
        if (nav == Nav::Done) {
          goHome(true);
        }
        break;
      }

      default:
        goHome(true);
        break;
    }
  }
}

void App::showSplash() {
  const LgFont* title = fontByRole(FontRole::Title);
  const LgFont* ui = fontByRole(FontRole::UI);
  canvas_.fillWhite();
  int w = canvas_.textWidth(title, "LingoInk");
  canvas_.drawText((canvas_.width() - w) / 2, canvas_.height() / 2 - 40, title, "LingoInk");
  const char* sub = "One device. One purpose.";
  w = canvas_.textWidth(ui, sub);
  canvas_.drawText((canvas_.width() - w) / 2, canvas_.height() / 2 + 20, ui, sub);
  presenter_.present(canvas_, Refresh::Full);
}

void App::goHome(bool full) {
  where_ = Where::Home;
  home_.render(canvas_);
  presenter_.present(canvas_, full ? Refresh::Full : Refresh::Balanced);
}

bool App::startLesson(const LessonMeta& meta) {
  if (!lesson_.start(meta, &progress_)) {
    // Show the parse error as the lesson screen does; user returns home.
    lesson_.render(canvas_);
    presenter_.present(canvas_, Refresh::Full);
    where_ = Where::Lesson;
    return true;
  }
  lesson_.render(canvas_);
  presenter_.present(canvas_, Refresh::Full);
  return true;
}

void App::finishLesson() {
  // Advance the resume pointer only when the lesson actually ran.
  if (!lesson_.wasError() && lesson_.lessonId()[0]) {
    strncpy(progress_.lastLessonId, lesson_.lessonId(),
            sizeof(progress_.lastLessonId) - 1);
    progress_.lastLessonId[sizeof(progress_.lastLessonId) - 1] = 0;
  }
  if (sdOk_) progress_.save();
  goHome(true);
}
