#pragma once
// App: event loop + screen state machine.
//
//   Splash → Home ⇄ Lesson / Progress
//
// Everything is statically allocated (no heap in steady state). The loop
// polls input, dispatches one key per iteration, redraws on Nav results and
// auto-sleeps after AUTO_SLEEP_MINUTES of inactivity.
//
// Serial logging (ui/Log.h) marks every validation checkpoint: boot, SD,
// course, screen transitions, keys, answers, progress save/load.

#include "../ui/Canvas.h"
#include "../ui/Presenter.h"
#include "../ui/FontRegistry.h"
#include "../ui/Log.h"
#include "../input/InputLoop.h"
#include "../course/CourseCatalog.h"
#include "../progress/ProgressStore.h"
#include "../screens/HomeScreen.h"
#include "../screens/LessonScreen.h"
#include "../screens/ProgressScreen.h"

#include <EInkDisplay.h>
#include <PowerManager.h>
#include <Arduino.h>
#include <SD.h>
#include <SPI.h>

class App {
 public:
  App(EInkDisplay& display, bool sdMounted)
      : canvas_(), presenter_(display), home_(), lesson_(), progressScreen_() {
    sdOk_ = sdMounted;
  }

  void run();

 private:
  enum class Where : uint8_t { Splash, Home, Lesson, Progress };

  static const char* whereName(Where w) {
    switch (w) {
      case Where::Splash: return "splash";
      case Where::Home: return "home";
      case Where::Lesson: return "lesson";
      case Where::Progress: return "progress";
      default: return "?";
    }
  }

  void goTo(Where w) {
    if (w != where_) {
      LOGI("APP", "screen: %s -> %s", whereName(where_), whereName(w));
      where_ = w;
    }
  }

  void showSplash();
  void goHome();
  bool startLesson(const LessonMeta& meta);
  void finishLesson();
  void powerDown();

  // Next lesson to run: after the resume pointer, or from the start when the
  // course is finished (validation keeps CONTINUE working at the end).
  const LessonMeta* pickNextLesson() const {
    const LessonMeta* m = catalog_.nextAfter(progress_.lastLessonId);
    if (!m && catalog_.course.lessonCount > 0) {
      m = &catalog_.course.lessons[0];
    }
    return m;
  }

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
  // Display and its framebuffer were brought up in main() BEFORE this App
  // was allocated — the framebuffer needs the cleanest heap. Only input and
  // the validation smoke test remain here.
  input_.begin();
  presenter_.smokeTest();
  lastMinutesTick_ = millis();
  LOGI("APP", "display+input initialized, free heap=%u", (unsigned)ESP.getFreeHeap());

  // Splash: logo + boot diagnostics for 1.2s.
  canvas_.init(cfg::SCREEN_W, cfg::SCREEN_H);
  showSplash();
  delay(900);

  // SD was mounted in main() BEFORE the display claimed the SPI pins.
  // Retry once here in case the pre-display mount raced a slow card.
  if (!sdOk_) {
    sdOk_ = SD.begin(cfg::SD_CS_PIN, SPI, 25000000, "/sd", 8);
  }
  if (!sdOk_) {
    LOGE("SD", "SD init failed (no card?) — continuing without progress");
  } else {
    LOGI("SD", "SD mounted, type=%u size=%llu MB", (unsigned)SD.cardType(),
         (unsigned long long)(SD.cardSize() / (1000ull * 1000ull)));
    progress_.begin();
    LOGI("PRG", "progress loaded: day=%u srs=%u lastLesson='%s'",
         (unsigned)progress_.time.day, (unsigned)progress_.itemCount,
         progress_.lastLessonId);
    if (catalog_.begin()) {
      LOGI("CRS", "course '%s' [%s->%s]: %u lessons, title='%s'",
           catalog_.course.id, catalog_.course.from, catalog_.course.to,
           (unsigned)catalog_.course.lessonCount, catalog_.course.title);
    } else {
      LOGE("CRS", "no course found — copy /courses/<id>/ to the SD card");
    }
    if (catalog_.course.id[0]) {
      strncpy(progress_.courseId, catalog_.course.id, sizeof(progress_.courseId) - 1);
    }
  }
  home_.bind(&progress_, &catalog_);
  progressScreen_.bind(&progress_);
  // Single full refresh at boot clears what the panel held through sleep.
  presenter_.fullNext();
  goHome();

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
      LOGI("APP", "power down (%s), idle %lus",
           k == Key::Power ? "power button" : "auto-sleep",
           (unsigned long)(input_.idleMs(now) / 1000));
      powerDown();
    }

    if (k == Key::None) {
      delay(15);
      continue;
    }
    LOGI("KEY", "event %d", (int)k);

    switch (where_) {
      case Where::Home: {
        Nav nav = home_.handleKey(k);
        HomeScreen::Action a = home_.takeAction();
        if (a == HomeScreen::Action::Continue || a == HomeScreen::Action::Review) {
          const LessonMeta* m = pickNextLesson();
          if (m) {
            if (a == HomeScreen::Action::Review) {
              LOGI("APP", "REVIEW: running '%s' (due=%u)", m->id,
                   (unsigned)progress_.dueCount());
            }
            startLesson(*m);
          } else {
            LOGW("CRS", "no lesson available");
          }
        } else if (a == HomeScreen::Action::Progress) {
          goTo(Where::Progress);
          canvas_.fillWhite();
          progressScreen_.render(canvas_);
          presenter_.present(canvas_, Refresh::Full);
        } else if (nav == Nav::RedrawFast) {
          canvas_.fillWhite();
          home_.render(canvas_);
          presenter_.present(canvas_, Refresh::Fast);
        } else if (nav == Nav::RedrawFull) {
          goHome();
        }
        break;
      }

      case Where::Lesson: {
        Nav nav = lesson_.handleKey(k);
        if (lesson_.finished() && (nav == Nav::Done || k == Key::Back)) {
          finishLesson();
        } else if (nav == Nav::RedrawFast) {
          canvas_.fillWhite();
          lesson_.render(canvas_);
          presenter_.present(canvas_, Refresh::Fast);
        } else if (nav == Nav::RedrawFull) {
          canvas_.fillWhite();
          lesson_.render(canvas_);
          presenter_.present(canvas_, Refresh::Balanced);
        }
        break;
      }

      case Where::Progress: {
        Nav nav = progressScreen_.handleKey(k);
        if (nav == Nav::Done) {
          goHome();
        }
        break;
      }

      default:
        goHome();
        break;
    }
  }
}

void App::showSplash() {
  const LgFont* title = fontByRole(FontRole::Title);
  const LgFont* ui = fontByRole(FontRole::UI);
  canvas_.fillWhite();
  int w = canvas_.textWidth(title, "LingoInk");
  canvas_.drawText((canvas_.width() - w) / 2, canvas_.height() / 2 - 50, title, "LingoInk");
  // Cyrillic on splash doubles as an early font check on real hardware.
  const char* sub = "Английский · Русский";
  w = canvas_.textWidth(ui, sub);
  canvas_.drawText((canvas_.width() - w) / 2, canvas_.height() / 2 + 10, ui, sub);
  const char* tag = "One device. One purpose.";
  w = canvas_.textWidth(ui, tag);
  canvas_.drawText((canvas_.width() - w) / 2, canvas_.height() / 2 + 44, ui, tag);
  presenter_.present(canvas_, Refresh::Fast);
  LOGI("APP", "splash drawn");
}

void App::goHome() {
  goTo(Where::Home);
  canvas_.fillWhite();
  home_.render(canvas_);
  presenter_.present(canvas_, Refresh::Full);
}

bool App::startLesson(const LessonMeta& meta) {
  LOGI("CRS", "loading lesson '%s' (%s)", meta.id, meta.file);
  if (!lesson_.start(meta, &progress_)) {
    LOGE("CRS", "lesson '%s' failed: %s", meta.id, lesson_.errorText());
    canvas_.fillWhite();
    lesson_.render(canvas_);
    presenter_.present(canvas_, Refresh::Full);
    goTo(Where::Lesson);
    return true;
  }
  LOGI("CRS", "lesson '%s' parsed: %u theory, %u exercises", meta.id,
       (unsigned)lesson_.theoryCount(), (unsigned)lesson_.exerciseCount());
  canvas_.fillWhite();
  lesson_.render(canvas_);
  presenter_.present(canvas_, Refresh::Full);
  goTo(Where::Lesson);
  return true;
}

void App::finishLesson() {
  // Advance the resume pointer only when the lesson actually ran.
  if (!lesson_.wasError() && lesson_.lessonId()[0]) {
    strncpy(progress_.lastLessonId, lesson_.lessonId(), sizeof(progress_.lastLessonId) - 1);
    progress_.lastLessonId[sizeof(progress_.lastLessonId) - 1] = 0;
  }
  LOGI("EX", "lesson '%s' done: %u%% correct", progress_.lastLessonId,
       (unsigned)lesson_.accuracyPct());
  if (sdOk_) {
    const bool ok = progress_.save();
    LOGI(ok ? "PRG" : "ERR", "progress %s (%u srs items)", ok ? "saved" : "SAVE FAILED",
         (unsigned)progress_.itemCount);
  }
  goHome();
}

void App::powerDown() {
  if (sdOk_ && progress_.itemCount + progress_.todayMinutes > 0) {
    progress_.save();
  }
  freeink::PowerManager::powerDownRailsForSleep();
  freeink::PowerManager::deepSleepUntilPowerButton();
}
