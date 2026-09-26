#pragma once
// App: event loop + screen state machine.
//
//   Splash → Home ⇄ Lesson / Progress / Settings
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
#include "../ui/Strings.h"
#include "../ui/BootCards.h"
#include "../ui/Log.h"
#include "../input/InputLoop.h"
#include "../course/CourseCatalog.h"
#include "../progress/ProgressStore.h"
#include "../screens/HomeScreen.h"
#include "../screens/LessonsScreen.h"
#include "../screens/LessonScreen.h"
#include "../screens/ProgressScreen.h"
#include "../screens/SettingsScreen.h"
#include "../screens/DateScreen.h"

#include <EInkDisplay.h>
#include <PowerManager.h>
#include <BatteryMonitor.h>
#include <Arduino.h>
#include <SD.h>
#include <SPI.h>

class App {
 public:
  App(EInkDisplay& display, bool sdMounted)
      : canvas_(), presenter_(display), home_(), lesson_(), progressScreen_(),
        settingsScreen_() {
    sdOk_ = sdMounted;
  }

  void run();

 private:
  enum class Where : uint8_t { Splash, Home, Lessons, Lesson, Progress, Settings, Date, Notice };

  static const char* whereName(Where w) {
    switch (w) {
      case Where::Splash: return "splash";
      case Where::Home: return "home";
      case Where::Lessons: return "lessons";
      case Where::Lesson: return "lesson";
      case Where::Progress: return "progress";
      case Where::Settings: return "settings";
      default: return "?";
    }
  }

  void goTo(Where w) {
    if (w != where_) {
      LOGI("APP", "screen: %s -> %s", whereName(where_), whereName(w));
      where_ = w;
    }
  }

  // One Nav -> Refresh translation for every screen: content changes ask
  // for Full, cursor moves for Fast. The Presenter still owns the actual
  // waveform (FAST everywhere unless fullNext()/Auto Clean — see UX.md).
  static Refresh refreshFor(Nav nav) {
    return (nav == Nav::RedrawFull) ? Refresh::Full : Refresh::Fast;
  }

  void showSplash();
  void goHome();
  void renderCurrent(Refresh mode);
  bool startLesson(const LessonMeta& meta);
  void finishLesson();
  void beginStudy(uint8_t action, const LessonMeta* m=nullptr, const char* tag=nullptr) {
    pending_=action; pendingLesson_=m;
    snprintf(practiceTag_,sizeof(practiceTag_),"%s",tag?tag:"");
    dateScreen_.start(&progress_);goTo(Where::Date);renderCurrent(Refresh::Fast);
  }
  void runStudy() {
    if(pending_==3) {goHome();return;}
    if(pending_==0) {
      auto m=pendingLesson_?pendingLesson_:pickNextLesson();
      if(m) {startLesson(*m);return;}
    } else {
      for(uint8_t i=0;i<catalog_.course.lessonCount;++i) {
        if(lesson_.startReview(catalog_.course.lessons[i],&progress_,&canvas_,pending_==2?practiceTag_:nullptr)) {
          lesson_.checkpoint();saveProgress();goTo(Where::Lesson);renderCurrent(Refresh::Fast);return;
        }
      }
    }
    goTo(Where::Notice);renderCurrent(Refresh::Fast);
  }
  void saveProgress() { if(sdOk_) progress_.saveFailed=!progress_.save(); }

  void powerDown();

  // Apply persisted settings to the singletons they configure. The device
  // is portrait-180 only (user decision): the persisted `orient` field is
  // ignored and the canvas is forced to the one true reading orientation.
  void applySettings() {
    Strings::setLang(progress_.uiLang);
    canvas_.setOrientation(3);  // portrait 180° — the book grip
    presenter_.setAutoCleanScreens(progress_.autoCleanScreens());
  }

  // Next lesson to run: after the resume pointer, or from the start when the
  // course is finished (validation keeps CONTINUE working at the end).
  const LessonMeta* pickNextLesson() const {
    return progress_.nextLesson(catalog_);
  }

  Canvas canvas_;
  Presenter presenter_;
  HomeScreen home_;
  LessonsScreen lessonsScreen_;
  LessonScreen lesson_;
  ProgressScreen progressScreen_;
  SettingsScreen settingsScreen_;
  DateScreen dateScreen_;
  uint8_t pending_=0;
  const LessonMeta* pendingLesson_=nullptr;
  char practiceTag_[32]="";

  CourseCatalog catalog_;
  ProgressStore progress_;
  InputLoop input_;
  BatteryMonitor battery_;

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

  // Load state BEFORE the splash so the boot screen already shows the saved
  // orientation. SD was mounted in main() BEFORE the display claimed the SPI
  // pins; retry once here in case the pre-display mount raced a slow card.
  canvas_.init(0);
  if (!sdOk_) {
    sdOk_ = SD.begin(cfg::SD_CS_PIN, SPI, 25000000, "/sd", 8);
  }
  if (!sdOk_) {
    LOGE("SD", "SD init failed (no card?) — continuing without progress");
  } else {
    LOGI("SD", "SD mounted, type=%u size=%llu MB", (unsigned)SD.cardType(),
         (unsigned long long)(SD.cardSize() / (1000ull * 1000ull)));
    progress_.begin();
    static const char* const kOrient[] = {"landscape", "portrait", "landscape180", "portrait180"};
    LOGI("PRG", "progress loaded: day=%u srs=%u lastLesson='%s' orient=%s",
         (unsigned)progress_.time.day, (unsigned)progress_.itemCount,
         progress_.lastLessonId, kOrient[progress_.orient % 4]);
    if (catalog_.begin()) {
      LOGI("CRS", "course '%s' [%s->%s]: %u lessons, title='%s'",
           catalog_.course.id, catalog_.course.from, catalog_.course.to,
           (unsigned)catalog_.course.lessonCount, catalog_.course.title);
    } else {
      LOGE("CRS", "no course found — copy /courses/<id>/ to the SD card");
    }
    if (catalog_.course.id[0]) {
      progress_.bindCourse(catalog_);
      lesson_.reconcileReviewItems(catalog_,progress_);
    }
  }
  applySettings();
  home_.bind(&progress_, &catalog_);
  lessonsScreen_.bind(&progress_, &catalog_, &canvas_);
  progressScreen_.bind(&progress_);
  settingsScreen_.bind(&progress_, &canvas_, &presenter_);

  // Splash: logo + boot diagnostics for ~1s.
  showSplash();
  delay(900);

  // No full refresh at boot (user decision): the power-off card doubles as a
  // lock screen and fast-swaps straight into the menu. The only full clean is
  // the one before power-off.
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
        if (a == HomeScreen::Action::Learn) {
          beginStudy(0);
        } else if(a==HomeScreen::Action::Review) {
          beginStudy(1);
        } else if (a == HomeScreen::Action::Lessons) {
          goTo(Where::Lessons);
          renderCurrent(Refresh::Full);
        } else if (a == HomeScreen::Action::Progress) {
          goTo(Where::Progress);
          renderCurrent(Refresh::Full);
        } else if (a == HomeScreen::Action::Settings) {
          goTo(Where::Settings);
          renderCurrent(Refresh::Full);
        } else if (nav == Nav::RedrawFast) {
          renderCurrent(Refresh::Fast);
        } else if (nav == Nav::RedrawFull) {
          goHome();
        }
        break;
      }

      case Where::Lessons: {
        Nav nav = lessonsScreen_.handleKey(k);
        const int li = lessonsScreen_.takeLesson();
        if (li >= 0 && li < (int)catalog_.course.lessonCount) {
          beginStudy(0,&catalog_.course.lessons[li]);
        } else if (nav == Nav::Done) {
          goHome();
        } else if (nav == Nav::RedrawFast || nav == Nav::RedrawFull) {
          renderCurrent(nav == Nav::RedrawFull ? Refresh::Full : Refresh::Fast);
        }
        break;
      }

      case Where::Lesson: {
        const StudySession before=progress_.session;
        Nav nav = lesson_.handleKey(k);
        if(nav!=Nav::Stay) {
          lesson_.checkpoint();
          if(memcmp(&before,&progress_.session,sizeof(before))!=0)saveProgress();
        }
        // Done may arrive from Summary, the error screen OR the mid-lesson
        // "EXIT LESSON?" dialog — finishLesson() sorts out resume semantics
        // via wasError()/aborted().
        if (nav == Nav::Done) {
          finishLesson();
        } else if (nav == Nav::RedrawFast || nav == Nav::RedrawFull) {
          renderCurrent(refreshFor(nav));
        }
        break;
      }

      case Where::Progress: {
        Nav nav = progressScreen_.handleKey(k);
        const char* tag=progressScreen_.takePractice();
        if(tag) {beginStudy(2,nullptr,tag);break;}
        if (nav == Nav::Done) {
          goHome();
        } else if (nav == Nav::RedrawFast || nav == Nav::RedrawFull) {
          renderCurrent(refreshFor(nav));
        }
        break;
      }

      case Where::Settings: {
        Nav nav = settingsScreen_.handleKey(k);
        if(settingsScreen_.takeDate()) {beginStudy(3);break;}
        if (nav == Nav::Done) {
          goHome();
        } else if (nav == Nav::RedrawFast || nav == Nav::RedrawFull) {
          renderCurrent(refreshFor(nav));
        }
        break;
      }

      case Where::Date: {
        Nav nav=dateScreen_.handleKey(k);
        if(nav==Nav::Done) {
          if(dateScreen_.accepted()) {saveProgress();runStudy();} else goHome();
        } else renderCurrent(Refresh::Fast);
        break;
      }
      case Where::Notice:
        if(k==Key::Ok || k==Key::Back) goHome();
        break;
      default:
        goHome();
        break;
    }
  }
}

// One shared redraw path: white fill + screen render + present.
void App::renderCurrent(Refresh mode) {
  canvas_.fillWhite();
  switch (where_) {
    case Where::Home: home_.render(canvas_); break;
    case Where::Lessons: lessonsScreen_.render(canvas_); break;
    case Where::Lesson: lesson_.render(canvas_); break;
    case Where::Progress: progressScreen_.render(canvas_); break;
    case Where::Settings: settingsScreen_.render(canvas_); break;
    case Where::Date: dateScreen_.render(canvas_);break;
    case Where::Notice: chrome::modal(canvas_,chrome::metrics(canvas_),U("READY","ГОТОВО"),
      U("No matching tasks right now.","Подходящих заданий сейчас нет."),
      U("Choose a lesson or check the date.","Выберите урок или проверьте дату."),"OK",nullptr);break;
    default: break;
  }
  if(progress_.saveFailed) {
    auto mt=chrome::metrics(canvas_);
    canvas_.rect(0,mt.widgetY-4,mt.w,32,false);
    canvas_.drawText(mt.m,mt.widgetY,fontByRole(FontRole::UI),U("SAVE FAILED — check SD","НЕ СОХРАНЕНО — проверьте SD"));
  }
  presenter_.present(canvas_, mode);
}

void App::showSplash() {
  canvas_.fillWhite();
  bootcards::drawSplash(canvas_);
  presenter_.present(canvas_, Refresh::Fast);
  LOGI("APP", "splash drawn");
}

void App::goHome() {
  goTo(Where::Home);
  renderCurrent(Refresh::Full);
}

bool App::startLesson(const LessonMeta& meta) {
  LOGI("CRS", "loading lesson '%s' (%s)", meta.id, meta.file);
  if (!lesson_.start(meta, &progress_, &canvas_)) {
    LOGE("CRS", "lesson '%s' failed: %s", meta.id, lesson_.errorText());
    goTo(Where::Lesson);
    renderCurrent(Refresh::Full);
    return true;
  }
  LOGI("CRS", "lesson '%s' parsed: %u theory, %u exercises", meta.id,
       (unsigned)lesson_.theoryCount(), (unsigned)lesson_.exerciseCount());
  lesson_.checkpoint();saveProgress();
  goTo(Where::Lesson);
  renderCurrent(Refresh::Full);
  return true;
}

void App::finishLesson() {
  // Advance the resume pointer only when the lesson actually ran to the end
  // (not on parse errors, not on "EXIT LESSON?" aborts).
  if (!lesson_.wasError() && !lesson_.aborted() && lesson_.lessonId()[0]) {
    strncpy(progress_.lastLessonId, lesson_.lessonId(), sizeof(progress_.lastLessonId) - 1);
    progress_.lastLessonId[sizeof(progress_.lastLessonId) - 1] = 0;
    const int idx = catalog_.indexOf(lesson_.lessonId());
    if (idx >= 0 && !lesson_.isReview()) progress_.markLessonDone(catalog_.course.lessons[idx].legacyIndex);
    if(!lesson_.isReview()) {
      auto s=progress_.state(lesson_.lessonId());
      if(s) {uint8_t stage=lesson_.independentAnswers() && lesson_.accuracyPct()>=80?2:1;
        if(stage>s->stage)s->stage=stage;
        s->day=progress_.time.day;}
    }
    progress_.session=StudySession{};
  }
  if (lesson_.aborted()) {
    LOGI("EX", "lesson '%s' aborted at %u%% (%u answers)", lesson_.lessonId(),
         (unsigned)lesson_.accuracyPct(), (unsigned)lesson_.exerciseCount());
  } else {
    LOGI("EX", "lesson '%s' done: %u%% correct", lesson_.lessonId(),
         (unsigned)lesson_.accuracyPct());
  }
  if (sdOk_) {
    const bool ok = progress_.save();
    progress_.saveFailed=!ok;
    LOGI(ok ? "PRG" : "ERR", "progress %s (%u srs items)", ok ? "saved" : "SAVE FAILED",
         (unsigned)progress_.itemCount);
  }
  goHome();
}

void App::powerDown() {
  if(where_==Where::Lesson) lesson_.checkpoint();
  saveProgress();
  // The power-off card doubles as the session-end deep clean: the panel
  // sleeps showing the same identity picture as the boot splash (user
  // request 2026-09-26) with a charge line, and the full waveform wipes
  // everything the session accumulated. This plus user-triggered Clean
  // Screen Now are the only full cleans. Order matters: progress is saved
  // first, the battery is measured fresh here (not the menu's cached value;
  // unavailable telemetry simply omits the line), then the single FULL
  // refresh carries the card to the panel before the rails drop — nothing
  // refreshes during sleep.
  uint16_t charge = 0;
  const int chargePct =
      battery_.readPercentageChecked(charge) ? (int)charge : -1;
  canvas_.fillWhite();
  bootcards::drawPowerOff(canvas_, chargePct);
  presenter_.fullNext();
  presenter_.present(canvas_, Refresh::Full);
  freeink::PowerManager::powerDownRailsForSleep();
  freeink::PowerManager::deepSleepUntilPowerButton();
}
