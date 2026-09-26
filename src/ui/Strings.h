#pragma once
// UI chrome strings, English and Russian. Fonts carry Latin+Cyrillic glyph
// subsets, so both languages render on every screen. The active language is
// a persisted setting (ProgressStore::uiLang) applied at boot via
// setLang(); changing it in Settings re-renders immediately.
//
// Keep kEn[] and kRu[] in the exact same order as StrId.

#include <stdint.h>

enum StrId : uint8_t {
  // Home menu (two-line rows: label + subtitle)
  MiLearn, MiLearnSubFmt, MiLessons, MiLessonsSub,
  MiProgress, MiProgressSub, MiSettings, MiSettingsSub,
  NoCourse, DueCourseFmt, LessonOfFmt, TrackFmt, HomeLang, FallbackCourse,
  // Lessons screen (course table of contents)
  LsTitle, LsNow,
  // Settings
  SettingsTitle, StLanguage, StCleanNow, StResetProgress, StBack,
  ResetNote, ResetDo,
  // Lesson
  InsChoose, InsTranslate, InsCloze, InsMistake, InsQuestion, InsReading,
  RuleFmt, PageFmt, ExOfFmt,
  Correct, Wrong, AnswerFmt,
  ExitTitle, ExitNote1, ExitNote2, ExitDo, Cancel,
  Explain, SumLabel, SumCountFmt, SumSrs,
  ErrorTitle,
  // Progress
  LevelFmt, SkVocabulary, SkGrammar, SkReading,
  WeakAreas, DueItemsFmt,
  // Boot card (splash; the sleeping panel shows the same artwork)
  AppName, SplashSub, SplashTag,
  IdCount
};

class Strings {
 public:
  static void setLang(uint8_t lang) { lang_ = lang ? 1 : 0; }
  static uint8_t lang() { return lang_; }
  static const char* get(StrId id) { return (lang_ ? kRu : kEn)[id]; }

 private:
  static inline uint8_t lang_ = 0;  // 0 = English, 1 = Russian
  static inline const char* const kEn[IdCount] = {
      "LEARN",
      "Continue: %s \xC2\xB7 lesson %u of %u",
      "LESSONS",
      "Pick any lesson yourself",
      "PROGRESS",
      "Skills and weak areas",
      "SETTINGS",
      "Language and screen",
      "NO COURSE — copy /courses to SD", "%u due \xC2\xB7 %s",
      "lesson %u of %u", "%s / %s", "ENGLISH", "course",
      "LESSONS", "\xE2\x80\xA2 now",
      "SETTINGS", "LANGUAGE", "CLEAN SCREEN NOW", "RESET PROGRESS", "BACK",
      "SRS, mastery and settings will be wiped.", "RESET",
      "CHOOSE THE CORRECT MEANING", "TRANSLATE", "FILL THE GAP",
      "FIND THE MISTAKE", "QUESTION", "READING",
      "RULE %u/%u", "PAGE %u/%u", "ex. %u of %u",
      "CORRECT", "WRONG", "answer: %s",
      "EXIT LESSON?", "Progress in this lesson is kept,",
      "the lesson stays for later.",
      "EXIT", "CANCEL",
      "EXPLAIN", "CORRECT", "%u of %u answers right",
      "Review scheduled by spaced repetition.",
      "LESSON ERROR",
      "LEVEL %s", "Vocabulary", "Grammar", "Reading",
      "Weak areas", "%u items due",
      "LingoInk", "English \xC2\xB7 Russian", "One device. One purpose.",
  };
  static inline const char* const kRu[IdCount] = {
      "УЧИТЬСЯ",
      "Продолжить: %s \xC2\xB7 урок %u из %u",
      "УРОКИ И ТЕМЫ",
      "Выбрать урок самостоятельно",
      "ПРОГРЕСС",
      "Навыки и слабые места",
      "НАСТРОЙКИ",
      "Язык и экран",
      "НЕТ КУРСА — скопируйте /courses на SD", "%u к повторению \xC2\xB7 %s",
      "урок %u из %u", "%s / %s", "ENGLISH", "курс",
      "УРОКИ", "\xE2\x80\xA2 сейчас",
      "НАСТРОЙКИ", "ЯЗЫК", "ОЧИСТИТЬ ЭКРАН", "СБРОС ПРОГРЕССА", "НАЗАД",
      "SRS, мастерство и настройки будут стёрты.", "СБРОСИТЬ",
      "ВЫБЕРИТЕ ВАРИАНТ", "ПЕРЕВЕДИТЕ", "ЗАПОЛНИТЕ ПРОПУСК",
      "НАЙДИТЕ ОШИБКУ", "ВОПРОС", "ЧТЕНИЕ",
      "ПРАВИЛО %u/%u", "СТРАНИЦА %u/%u", "упр. %u из %u",
      "ВЕРНО", "ОШИБКА", "ответ: %s",
      "ВЫЙТИ ИЗ УРОКА?", "Ответы этого урока сохранятся,",
      "урок останется для повтора.",
      "ВЫЙТИ", "ОТМЕНА",
      "ОБЪЯСНЕНИЕ", "ВЕРНО", "верных ответов: %u из %u",
      "Повторение запланировано по SRS.",
      "ОШИБКА УРОКА",
      "УРОВЕНЬ %s", "Словарь", "Грамматика", "Чтение",
      "Слабые места", "%u к повторению",
      "LingoInk", "Английский \xC2\xB7 Русский", "Один экран. Одна цель.",
  };
};

inline const char* U(const char* en, const char* ru) { return Strings::lang() ? ru : en; }

inline const char* S(StrId id) { return Strings::get(id); }
