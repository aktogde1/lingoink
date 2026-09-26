#pragma once
// Central tuning constants. Everything a product decision might change lives
// here so the domain code stays free of magic numbers.

#include <stdint.h>

namespace cfg {

// SD card layout (all user content + state lives on the card).
constexpr const char* COURSE_DIR   = "/courses";
constexpr const char* DATA_DIR     = "/lingoink";
constexpr const char* PROGRESS_FILE = "/lingoink/progress.json";
constexpr const char* PROGRESS_TMP = "/lingoink/progress.tmp";

// X4 SD chip-select (bus itself: SCLK=8, MISO=7, MOSI=10 — see main.cpp).
constexpr int SD_CS_PIN = 12;

// Raw lesson JSON read buffer (static, BSS).
constexpr int JSON_BUF_BYTES    = 36864;

// Lesson loading caps — a lesson larger than this is rejected with a clear
// error instead of fragmenting the heap.
constexpr int LESSON_POOL_BYTES  = 18432; // interned strings (prompts/options/text)
constexpr int MAX_EXERCISES     = 48;
constexpr int MAX_OPTIONS       = 6;
constexpr int MAX_THEORY_LINES  = 20;
constexpr int MAX_READING_PAGES = 12;
constexpr int MAX_SRS_PER_EX    = 4;
constexpr int MAX_TAGS_PER_EX   = 4;
constexpr int MAX_LESSONS       = 24;

// SRS (SM-2 lite).
constexpr float EASE_MIN = 1.3f;
constexpr float EASE_MAX = 2.8f;
constexpr float EASE_START = 2.5f;
constexpr uint16_t DAY_MIN = 1;      // relearn interval, days
constexpr uint16_t DAY_SECOND = 6;   // second successful interval, days

// Mastery exponential moving average weight (new sample share).
constexpr float MASTERY_ALPHA = 0.2f;

// Rendering.
constexpr int SCREEN_W = 800;
constexpr int SCREEN_H = 480;
constexpr int MARGIN = 40;
constexpr int FAST_REFRESH_BETWEEN_FULL = 24;

// Input.
constexpr uint32_t OK_LONG_PRESS_MS = 600;
constexpr uint32_t AUTO_SLEEP_MINUTES = 10;

// Daily goal.
constexpr uint16_t GOAL_MINUTES_DEFAULT = 15;

} // namespace cfg
