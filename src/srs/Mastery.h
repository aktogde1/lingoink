#pragma once
// Mastery model: per-skill and per-pattern rolling accuracy.
//
// Skills are the coarse stats shown on the Progress screen; pattern tags are
// fine-grained ("pp-vs-past", "articles") and drive the weak-areas list and
// later exercise prioritisation: when a pattern's EMA drops, the scheduler
// prefers NEW exercises tagged with it instead of re-serving the same card.
//
// Pure domain code.

#include "../config.h"
#include <stdint.h>

enum class Skill : uint8_t { Vocabulary = 0, Grammar, Reading, Production, Count };

inline const char* skillName(Skill s) {
  switch (s) {
    case Skill::Vocabulary: return "Vocabulary";
    case Skill::Grammar: return "Grammar";
    case Skill::Reading: return "Reading";
    case Skill::Production: return "Sentence use";
    default: return "?";
  }
}

struct Stat {
  float ema = 0;       // exponential moving average of correctness
  uint16_t count = 0;  // samples seen

  void update(bool correct) {
    float target = correct ? 1.0f : 0.0f;
    ema = (count == 0) ? target : (ema * (1.0f - cfg::MASTERY_ALPHA) + target * cfg::MASTERY_ALPHA);
    if (count < 0xFFFF) count++;
  }

  uint8_t percent() const {
    if (count == 0) return 0;
    int p = (int)(ema * 100.0f + 0.5f);
    if (p < 0) p = 0;
    if (p > 100) p = 100;
    return (uint8_t)p;
  }
};

struct TagStat {
  char tag[20];
  Stat stat;
  bool used = false;
};

class Mastery {
 public:
  Stat skills[(size_t)Skill::Count];

  // Fixed-size tag table (no hash map on this device).
  static constexpr int MAX_TAGS = 24;
  TagStat tags[MAX_TAGS];

  void record(Skill skill, const char* const* exTags, uint8_t tagCount, bool correct) {
    skills[(size_t)skill].update(correct);
    for (uint8_t i = 0; i < tagCount && exTags[i]; i++) {
      Stat* s = tagStat(exTags[i]);
      if (s) s->update(correct);
    }
  }

  // Top weak tags (lowest EMA with enough samples), out-parameters filled
  // best-first. Returns how many were written.
  int weakTags(const char** out, int maxOut) const {
    int n = 0;
    // Selection by repeated minimum scan; MAX_TAGS is tiny.
    bool taken[MAX_TAGS] = {false};
    for (int r = 0; r < maxOut; r++) {
      int best = -1;
      for (int i = 0; i < MAX_TAGS; i++) {
        if (!tags[i].used || taken[i]) continue;
        if (tags[i].stat.count < 5) continue; // not enough evidence yet
        if (best < 0 || tags[i].stat.ema < tags[best].stat.ema) best = i;
      }
      if (best < 0) break;
      taken[best] = true;
      out[n++] = tags[best].tag;
    }
    return n;
  }

 private:
  Stat* tagStat(const char* tag) {
    int freeSlot = -1;
    for (int i = 0; i < MAX_TAGS; i++) {
      if (tags[i].used && strncmp(tags[i].tag, tag, sizeof(tags[i].tag)) == 0) {
        return &tags[i].stat;
      }
      if (!tags[i].used && freeSlot < 0) freeSlot = i;
    }
    if (freeSlot < 0) return nullptr; // table full — drop the new tag
    tags[freeSlot].used = true;
    strncpy(tags[freeSlot].tag, tag, sizeof(tags[freeSlot].tag) - 1);
    return &tags[freeSlot].stat;
  }
};
