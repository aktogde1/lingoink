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
  uint16_t easePct;    // ease factor * 100 (130..280)
  uint16_t intervalDays;
  bool retained = false; // independent free recall on a later due day
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
  static void grade(SrsItem& it, uint8_t quality, uint16_t today, bool freeRecall = false) {
    if (quality > 5) quality = 5;
    // A second success today is practice, not another spaced recall.
    // A failure still wins, and cannot be erased by immediately seeing the answer.
    if (quality >= 3 && (it.reps || it.lapses) &&
        (it.lastDay == today || it.nextDay > today)) return;
    if (quality < 3 && it.lastDay == today && it.lapses && it.reps == 0) return;
    float ease = it.easePct / 100.0f;
    if (quality < 3) {
      it.retained=false;
      it.reps = 0;
      if(it.lapses<255) it.lapses++;
      it.intervalDays = cfg::DAY_MIN;
      ease -= 0.20f;
    } else {
      if(it.reps<255) it.reps++;
      if(freeRecall && it.reps>=2) it.retained=true;
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
      it.easePct = (uint16_t)(ease * 100);
    }
    if (ease < cfg::EASE_MIN) ease = cfg::EASE_MIN;
    if (ease > cfg::EASE_MAX) ease = cfg::EASE_MAX;
    it.easePct = (uint16_t)(ease * 100);
    it.lastDay = today;
    it.nextDay = (uint16_t)(today + it.intervalDays);
  }

  static bool isDue(const SrsItem& it, uint16_t today) {
    return it.nextDay <= today;
  }

  // Self-assessed free recall outcomes (LessonScreen recall menu).
  enum class RecallOutcome : uint8_t { Miss = 0, Hint = 1, Recalled = 2 };

  // Grade one self-assessed recall. The three outcomes map to:
  //   Recalled — full success: SM-2 grade 5 with free-recall retention
  //              tracking. The caller must pass this outcome ONLY when the
  //              answer was produced without any help.
  //   Hint     — partial knowledge: NOT a success (the interval never grows,
  //              the item comes back tomorrow), but softer than a lapse: the
  //              ease penalty is half a fail and lapses is not incremented.
  //              Seeing an item early (not due yet) or again the same day is
  //              practice — the item is left unchanged.
  //   Miss     — full lapse: grade 1 (reps reset, ease -0.2, interval 1 day).
  // The same-day guards of grade() apply to every outcome: a repeat on the
  // same day can never inflate an interval, and a failure cannot be erased
  // by an immediate retry.
  static void gradeRecall(SrsItem& it, uint8_t outcome, uint16_t today) {
    if (outcome > (uint8_t)RecallOutcome::Recalled) {
      outcome = (uint8_t)RecallOutcome::Recalled;
    }
    if (outcome == (uint8_t)RecallOutcome::Recalled) {
      grade(it, 5, today, true);
      return;
    }
    if (outcome == (uint8_t)RecallOutcome::Miss) {
      grade(it, 1, today);
      return;
    }
    // Hint: due items retry tomorrow with a half-size ease penalty; early or
    // same-day repeats are practice and change nothing.
    if (it.lastDay == today || it.nextDay > today) return;
    it.reps = 0;
    it.retained = false;
    it.intervalDays = cfg::DAY_MIN;
    float ease = it.easePct / 100.0f - 0.10f;
    if (ease < cfg::EASE_MIN) ease = cfg::EASE_MIN;
    if (ease > cfg::EASE_MAX) ease = cfg::EASE_MAX;
    it.easePct = (uint16_t)(ease * 100);
    it.lastDay = today;
    it.nextDay = (uint16_t)(today + it.intervalDays);
  }
};
