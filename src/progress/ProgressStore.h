#pragma once
// ProgressStore owns all persistent user state on the SD card:
//   /lingoink/progress.json
//
// State: current course, last lesson (resume), daily goal/streak, day
// counter, SRS items, mastery stats.
//
// Day counter note: the X4 has no RTC battery and no network in MVP, so
// "days" are derived from accumulated uptime minutes (persisted on every
// save and resumed on boot). If the system clock looks valid (NTP was ever
// set), wall-clock days win. This is honest enough for SRS at daily use.

#include "../srs/SrsScheduler.h"
#include "../srs/Mastery.h"

#include <SD.h>

struct DayTime {
  uint16_t day = 0;        // monotonic day counter for SRS
  uint16_t uptimeMin = 0;  // accumulated active minutes
};

class ProgressStore {
 public:
  char courseId[24] = "";
  char lastLessonId[32] = "";
  uint16_t goalMinutes = cfg::GOAL_MINUTES_DEFAULT;
  uint16_t todayMinutes = 0;  // minutes practiced during the current day counter
  uint16_t streakDays = 0;
  char currentLevel[8] = "A2";

  DayTime time;
  Mastery mastery;

  static constexpr int MAX_SRS = 250;
  SrsItem items[MAX_SRS];
  uint16_t itemCount = 0;

  bool begin() {
    if (!SD.exists(cfg::DATA_DIR)) {
      SD.mkdir(cfg::DATA_DIR);
    }
    return load();
  }

  bool load() {
    if (!SD.exists(cfg::PROGRESS_FILE)) {
      return true; // fresh start
    }
    File f = SD.open(cfg::PROGRESS_FILE, FILE_READ);
    if (!f) return false;
    // Progress JSON is bounded (MAX_SRS items), so read it whole.
    size_t sz = f.size();
    if (sz > 48 * 1024) {
      f.close();
      return false;
    }
    char* buf = (char*)malloc(sz + 1);
    if (!buf) {
      f.close();
      return false;
    }
    size_t got = f.readBytes(buf, sz);
    f.close();
    buf[got] = 0;
    bool ok = parse(buf, got);
    free(buf);
    return ok;
  }

  // Atomic-ish: write temp then rename over the target.
  bool save() {
    File f = SD.open(cfg::PROGRESS_TMP, FILE_WRITE);
    if (!f) return false;
    write(f);
    f.close();
    if (SD.exists(cfg::PROGRESS_FILE)) {
      SD.remove(cfg::PROGRESS_FILE);
    }
    return SD.rename(cfg::PROGRESS_TMP, cfg::PROGRESS_FILE);
  }

  SrsItem* find(const char* id) {
    for (uint16_t i = 0; i < itemCount; i++) {
      if (strncmp(items[i].id, id, sizeof(items[i].id)) == 0) return &items[i];
    }
    return nullptr;
  }

  SrsItem* findOrCreate(const char* id) {
    SrsItem* it = find(id);
    if (it) return it;
    if (itemCount >= MAX_SRS) return nullptr;
    items[itemCount] = SrsScheduler::makeItem(id, time.day);
    return &items[itemCount++];
  }

  uint16_t dueCount() const {
    uint16_t n = 0;
    for (uint16_t i = 0; i < itemCount; i++) {
      if (SrsScheduler::isDue(items[i], time.day)) n++;
    }
    return n;
  }

  // Call once per boot; advances the day counter based on persisted uptime.
  void advanceDay(uint16_t newUptimeMin) {
    uint32_t total = (uint32_t)time.uptimeMin + newUptimeMin;
    time.uptimeMin = (uint16_t)(total % (24u * 60u));
    uint16_t days = (uint16_t)(total / (24u * 60u));
    if (days > 0) {
      uint16_t before = time.day;
      time.day += days;
      if (time.day != before) {
        todayMinutes = 0; // new day, reset the daily counter
      }
    }
  }

  void addMinutes(uint16_t m) {
    uint32_t t = (uint32_t)todayMinutes + m;
    todayMinutes = (uint16_t)(t > 600 ? 600 : t);
    uint32_t up = (uint32_t)time.uptimeMin + m;
    time.uptimeMin = (uint16_t)(up % (24u * 60u));
    if (up >= 24u * 60u) {
      time.day += (uint16_t)(up / (24u * 60u));
      todayMinutes = 0;
    }
  }

 private:
  void write(File& f);
  bool parse(const char* json, size_t len);
};
