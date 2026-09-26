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

// Rendering. SCREEN_W/H are PANEL-native dims (the buffer layout is always
// panel-native; portrait is a coordinate transform inside Canvas, not a
// second framebuffer). Logical screen is 800x480 or 480x800.
constexpr int SCREEN_W = 800;
constexpr int SCREEN_H = 480;
constexpr int MARGIN = 40;    // landscape page margin
constexpr int MARGIN_P = 28;  // portrait page margin (narrower screen)

// UI chrome metrics — the single source of truth behind ui/Chrome.h.
// Header = title text, a solid rule under it, content; the bottom edge has
// no hint line (removed — it only ate space): battery and scroll hints sit
// alone in the bottom-right corner.
constexpr int HEADER_TEXT_OFF = 4;   // title top below the margin line
constexpr int HEADER_RULE_GAP = 8;   // title box bottom -> header rule
constexpr int CONTENT_GAP = 20;      // header rule -> first content row
constexpr int FOOT_WIDGET_OFF = 28;  // bottom widgets (battery, ^/+N) top
constexpr int CONTENT_FOOT_GAP = 10; // last content row -> widget zone
constexpr int ROW_H = 50;            // one-line list row pitch (landscape)
constexpr int ROW_H_P = 56;          // portrait
constexpr int MENU_ROW_H = 92;       // two-line menu row (landscape)
constexpr int MENU_ROW_H_P = 124;    // portrait (fits a wrapped subtitle)
constexpr int SEL_PAD_X = 14;        // selection card beyond the margin
constexpr int SEL_PAD_Y = 9;         // selection card above the text top
constexpr int SEL_RADIUS = 10;       // selection card corner radius
constexpr int DASH_PERIOD = 4;       // separator dash pattern, px
constexpr int DASH_FILL = 2;

// Input.
constexpr uint32_t OK_LONG_PRESS_MS = 600;
constexpr uint32_t POWER_HOLD_MS = 1000;  // hold power this long to shut down
constexpr uint32_t AUTO_SLEEP_MINUTES = 10;

// Daily goal.
constexpr uint16_t GOAL_MINUTES_DEFAULT = 15;

// Auto clean (periodic FULL refresh) choices offered in Settings, in screens.
constexpr uint8_t CLEAN_CHOICES[] = {0, 20, 40};
constexpr int CLEAN_CHOICES_COUNT = 3;

} // namespace cfg
