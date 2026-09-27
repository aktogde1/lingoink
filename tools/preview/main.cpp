// Host preview: renders LingoInk screens into 1-bpp BMPs (out/preview/)
// without hardware. Uses the REAL Canvas/fonts/screens the firmware uses —
// zero drift. Build & run via tools/preview/run.sh (or run.ps1) from the
// repo root.
//
// Scenes cover every screen and interesting state (feedback, dialogs,
// scrolling, no-course) x all 4 orientations.

#include "bmp_out.h"

#include "ui/BootCards.h"
#include "ui/Canvas.h"
#include "ui/FontRegistry.h"
#include "ui/Presenter.h"
#include "ui/Strings.h"
#include "screens/HomeScreen.h"
#include "screens/LessonScreen.h"
#include "screens/LessonsScreen.h"
#include "screens/ProgressScreen.h"
#include "screens/SettingsScreen.h"
#include "course/CourseCatalog.h"
#include "course/LessonLoader.h"
#include "progress/ProgressStore.h"

#include <cstring>
#include <string>

static Canvas canvas;
static EInkDisplay display;
static Presenter presenter(display);
static CourseCatalog catalog;
static ProgressStore progress;
static HomeScreen home;
static LessonsScreen lessons;
static ProgressScreen progressScreen;
static SettingsScreen settings;

static const char* kOutDir = "out/preview";
static int sceneCount = 0;

// Render `draw` in all 4 orientations and write one BMP per orientation.
template <typename Draw>
static void scene(const char* name, Draw draw) {
  printf("scene: %s\n", name);
  fflush(stdout);
  for (int o = 0; o < 4; o++) {
    canvas.setOrientation((uint8_t)o);
    canvas.fillWhite();
    draw();
    char path[128];
    snprintf(path, sizeof(path), "%s/%s_o%d.bmp", kOutDir, name, o);
    if (!bmp::writeScene(path, canvas)) {
      fprintf(stderr, "preview: cannot write %s\n", path);
      exit(1);
    }
    sceneCount++;
  }
}

static void buildDemoState() {
  Strings::setLang(1);  // Russian chrome (product default)

  // Load the REAL course from data/sd through the SD shim — the preview
  // renders exactly what the device loads from /courses/.
  if (!catalog.begin()) {
    fprintf(stderr, "preview: no course found under data/sd\n");
    exit(1);
  }
  printf("course '%s': %u lessons\n", catalog.course.id,
         (unsigned)catalog.course.lessonCount);

  strncpy(progress.courseId, "english_ru", sizeof(progress.courseId) - 1);
  strncpy(progress.lastLessonId, "a2_05", sizeof(progress.lastLessonId) - 1);
  strncpy(progress.currentLevel, "A2", sizeof(progress.currentLevel) - 1);
  progress.uiLang = 1;
  progress.orient = 0;
  progress.doneMask = 0x3;  // lessons 1-2 finished
  progress.time.day = 9;
  progress.bindCourse(catalog);

  progress.mastery.skills[(size_t)Skill::Vocabulary] = {0.74f, 61};
  progress.mastery.skills[(size_t)Skill::Grammar] = {0.52f, 38};
  progress.mastery.skills[(size_t)Skill::Reading] = {0.66f, 24};

  auto tag = [&](int slot, const char* name, float ema, uint16_t count) {
    strncpy(progress.mastery.tags[slot].tag, name,
            sizeof(progress.mastery.tags[slot].tag) - 1);
    progress.mastery.tags[slot].stat = {ema, count};
    progress.mastery.tags[slot].used = true;
  };
  tag(0, "pp-vs-past", 0.31f, 14);
  tag(1, "articles", 0.44f, 11);
  tag(2, "phrasal-verbs", 0.58f, 9);

  auto due = [&](int slot, const char* id, uint16_t nextDay) {
    progress.items[slot] = SrsScheduler::makeItem(id, 0);
    progress.items[slot].nextDay = nextDay;
  };
  due(0, "vp.actually", 7);
  due(1, "vp.basically", 8);
  due(2, "pp.form", 9);
  due(3, "pp.since-for", 12);  // not due yet
  progress.itemCount = 4;
}

// Advance the lesson with OK until it reaches `p` (bounded — broken content
// must not hang the preview).
static void driveTo(LessonScreen& ls, LessonScreen::Phase p, int maxSteps = 200) {
  for (int i = 0; i < maxSteps && ls.phase() != p; i++) {
    if (ls.phase() == LessonScreen::Phase::Summary) return;
    if (ls.phase() == LessonScreen::Phase::Error) return;
    ls.handleKey(Key::Ok);
  }
}

int main() {
  setvbuf(stdout, nullptr, _IONBF, 0);
  buildDemoState();

  home.bind(&progress, &catalog);
  lessons.bind(&progress, &catalog, &canvas);
  progressScreen.bind(&progress);
  settings.bind(&progress, &canvas, &presenter);

  printf("preview scenes -> %s\n", kOutDir);

  // ---- Boot cards -------------------------------------------------------
  scene("splash", [] { bootcards::drawSplash(canvas); });
  // Power-off / auto-sleep card: same artwork + charge line measured fresh
  // at shutdown; -1 = telemetry unavailable (line omitted, never faked).
  scene("poweroff_b5", [] { bootcards::drawPowerOff(canvas, 5); });
  scene("poweroff_b76", [] { bootcards::drawPowerOff(canvas, 76); });
  scene("poweroff_b100", [] { bootcards::drawPowerOff(canvas, 100); });
  scene("poweroff_bna", [] { bootcards::drawPowerOff(canvas, -1); });

  // ---- Home -------------------------------------------------------------
  home.bind(&progress, &catalog);
  scene("home", [&] { home.render(canvas); });  // LEARN selected: long subtitle
  home.handleKey(Key::Down);                    // second entry selected
  scene("home_sel1", [&] { home.render(canvas); });
  home.handleKey(Key::Down);
  home.handleKey(Key::Down);
  home.handleKey(Key::Down);                    // last entry selected
  scene("home_sel4", [&] { home.render(canvas); });

  // No-course variant.
  const uint8_t realCount = catalog.course.lessonCount;
  catalog.course.lessonCount = 0;
  scene("home_nocourse", [&] { home.render(canvas); });
  scene("lessons_nocourse", [&] { lessons.render(canvas); });
  catalog.course.lessonCount = realCount;

  // ---- Lessons (table of contents) ---------------------------------------
  scene("lessons", [&] { lessons.render(canvas); });
  for (int i = 0; i < 9; i++) lessons.handleKey(Key::Down);  // scrolled
  scene("lessons_scrolled", [&] { lessons.render(canvas); });
  for (int i = 0; i < 9; i++) lessons.handleKey(Key::Up);

  // ---- Lesson phases (real lesson JSON from data/sd) ---------------------
  const LessonMeta& vocab = catalog.course.lessons[0];    // a2_01, no theory
  const LessonMeta& grammar = catalog.course.lessons[4];  // a2_05 Past Simple
  const LessonMeta& psTheory = catalog.course.lessons[1]; // a2_02 PP theory
  const LessonMeta& reading = catalog.course.lessons[5];  // a2_06 book

  {
    // "Already know" gate on a never-viewed lesson (a2_06).
    LessonScreen gls;
    gls.start(reading, &progress, &canvas);
    scene("lesson_gate", [&] { gls.render(canvas); });
  }

  {
    LessonScreen ls;
    ls.start(psTheory, &progress, &canvas);
    scene("lesson_theory", [&] { ls.render(canvas); });
    // Exit dialog over the theory phase.
    ls.handleKey(Key::Back);
    scene("lesson_exit", [&] { ls.render(canvas); });
    ls.handleKey(Key::Back);  // cancel
  }
  {
    LessonScreen ls;
    ls.start(grammar, &progress, &canvas);
    scene("lesson_theory_wrapped", [&] { ls.render(canvas); });
  }
  {
    LessonScreen ls;
    ls.start(vocab, &progress, &canvas);
    driveTo(ls, LessonScreen::Phase::Exercise);
    scene("lesson_exercise", [&] { ls.render(canvas); });
    ls.handleKey(Key::Down);  // move selection once
    scene("lesson_exercise_sel1", [&] { ls.render(canvas); });
    ls.handleKey(Key::Ok);  // answer -> feedback
    scene("lesson_feedback", [&] { ls.render(canvas); });
    if (ls.phase() == LessonScreen::Phase::Exercise) {
      ls.handleKey(Key::OkLong);  // explanation reader (when present)
      scene("lesson_explain", [&] { ls.render(canvas); });
    }
    driveTo(ls, LessonScreen::Phase::Summary);
    scene("lesson_summary", [&] { ls.render(canvas); });
  }
  {
    LessonScreen ls;
    ls.start(reading, &progress, &canvas);
    driveTo(ls, LessonScreen::Phase::Reading);
    scene("lesson_reading", [&] { ls.render(canvas); });
    // 3 pages -> Q1; 5 answered questions -> the long-option question.
    for (int i = 0; i < 3; i++) ls.handleKey(Key::Ok);
    for (int i = 0; i < 10; i++) ls.handleKey(Key::Ok);
    scene("lesson_longoption", [&] { ls.render(canvas); });
  }
  {
    // The mixed-review cloze (spoke/talked/told/said) — unanswered and
    // answered-with-feedback, reproducing the on-device report.
    LessonScreen ls;
    ls.start(catalog.course.lessons[3], &progress, &canvas);
    driveTo(ls, LessonScreen::Phase::Exercise);
    for (int i = 0; i < 6; i++) ls.handleKey(Key::Ok);  // -> exercise 4
    scene("lesson_cloze", [&] { ls.render(canvas); });
    ls.handleKey(Key::Ok);  // answer (first option)
    scene("lesson_cloze_feedback", [&] { ls.render(canvas); });
  }
  {
    LessonScreen ls;
    LessonMeta bad = vocab;
    snprintf(bad.file, sizeof(bad.file), "data/sd/english_ru/lessons/missing.json");
    ls.start(bad, &progress, &canvas);
    scene("lesson_error", [&] { ls.render(canvas); });
  }

  // ---- Progress -----------------------------------------------------------
  scene("progress", [&] { progressScreen.render(canvas); });

  // ---- Settings -----------------------------------------------------------
  scene("settings", [&] { settings.render(canvas); });
  for (int i = 0; i < 2; i++) settings.handleKey(Key::Down);
  settings.handleKey(Key::Ok);  // RESET PROGRESS -> confirm modal
  scene("settings_reset", [&] { settings.render(canvas); });

  printf("done: %d images (%d scenes x 4 orientations)\n", sceneCount,
         sceneCount / 4);
  return 0;
}
