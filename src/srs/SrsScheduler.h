#pragma once
// Spaced repetition scheduler — SM-2 lite.
//
// A learning item is anything the user should retain: a vocabulary pair, a
// grammar pattern, a phrasal verb, a common-mistake rule. Items are graded
// 0..5 (Anki-style); >=3 counts as recalled. Day numbers are a plain uint16
// day counter maintained by ProgressStore (see TimeSource note there).
//
// Pure domain code: no Arduino types, unit-testable on the host.

#include "../config.h"
#include <stdint.h>
#include <string.h>

struct SrsItem {
  char id[28];        // e.g. "vp.actually", "pp.form"
  uint8_t easePct;    // ease factor * 100 (130..280)
  uint16_t intervalDays;
  uint8_t reps;       // consecutive successful recalls
  uint8_t lapses;     // total failures
  uint16_t lastDay;   // day counter of last review
  uint16_t nextDay;   // day counter when the item is due
};

class SrsScheduler {
 public:
  static SrsItem makeItem(const char* id, uint16_t today) {
    SrsItem it{};
    strncpy(it.id, id, sizeof(it.id) - 1);
    it.easePct = (uint8_t)(cfg::EASE_START * 100);
    it.intervalDays = 0;
    it.nextDay = today; // new items are immediately available
    it.lastDay = 0;
    return it;
  }

  // Grades one review. quality: 0..5.
  static void grade(SrsItem& it, uint8_t quality, uint16_t today) {
    if (quality > 5) quality = 5;
    float ease = it.easePct / 100.0f;
    if (quality < 3) {
      it.reps = 0;
      it.lapses++;
      it.intervalDays = cfg::DAY_MIN;
      ease -= 0.20f;
    } else {
      it.reps++;
      if (it.reps == 1) {
        it.intervalDays = cfg::DAY_MIN;
      } else if (it.reps == 2) {
        it.intervalDays = cfg::DAY_SECOND;
      } else {
        float q = (float)quality;
        ease += (float)(0.1 - (0.08 * (5 - q)));
        // SM-2 interval growth
        uint32_t next = (uint32_t)(it.intervalDays * (it.easePct / 100.0f)) + 1;
        if (next > 365) next = 365;
        it.intervalDays = (uint16_t)next;
      }
      if (ease < cfg::EASE_MIN) ease = cfg::EASE_MIN;
      if (ease > cfg::EASE_MAX) ease = cfg::EASE_MAX;
      it.easePct = (uint8_t)(ease * 100);
    }
    if (ease < cfg::EASE_MIN) ease = cfg::EASE_MIN;
    if (ease > cfg::EASE_MAX) ease = cfg::EASE_MAX;
    it.easePct = (uint8_t)(ease * 100);
    it.lastDay = today;
    it.nextDay = (uint16_t)(today + it.intervalDays);
  }

  static bool isDue(const SrsItem& it, uint16_t today) {
    return it.nextDay <= today;
  }
};
