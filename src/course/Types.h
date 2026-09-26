#pragma once
// Course domain types. Strings live in the Lesson's string pool; every
// const char* in these structs points into it. No std::string, no heap.

#include "../config.h"
#include <stdint.h>

enum class ExType : uint8_t {
  Choice,    // multiple choice (vocab RU->EN, dialogue line, comprehension QA)
  Cloze,     // fill the gap: prompt with ___ , option list
  Mistake,   // find the mistake: sentence + "a → b" options
  Reading,   // multi-page text, comprehension questions follow as Choice/Cloze
};

struct TheoryBlock {
  const char* title;
  const char* lines[cfg::MAX_THEORY_LINES];
  uint8_t lineCount;
};

struct Exercise {
  ExType type;
  const char* srsIds[cfg::MAX_SRS_PER_EX];   // learning items touched
  uint8_t srsCount;
  const char* tags[cfg::MAX_TAGS_PER_EX];    // pattern tags for weak-area stats
  uint8_t tagCount;

  const char* prompt;       // question / sentence / stimulus
  const char* promptSmall;  // instruction or translation line (may be null)
  const char* options[cfg::MAX_OPTIONS];
  uint8_t optionCount;
  uint8_t correct;          // index into options

  const char* explain;      // long-press OK (may be null)

  // Reading payload (type == Reading).
  const char* readingTitle;
  const char* pages[cfg::MAX_READING_PAGES];
  uint8_t pageCount;
};

enum class LessonKind : uint8_t { Mixed, Vocab, Grammar, Reading };

struct Lesson {
  char id[32];
  char title[64];
  char level[8];
  LessonKind kind;

  uint8_t theoryCount;
  TheoryBlock theory[4];

  uint16_t exerciseCount;
  Exercise exercises[cfg::MAX_EXERCISES];

  // String pool.
  char pool[cfg::LESSON_POOL_BYTES];
  uint16_t poolUsed;

  const char* intern(const char* s, int len = -1);
};

// Parse error reporting for the loader.
struct LessonParseResult {
  bool ok;
  char error[96];
};
