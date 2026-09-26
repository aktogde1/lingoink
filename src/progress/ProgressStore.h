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
#include "StudyState.h"
#include "../course/CourseCatalog.h"

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

  // Settings (persisted here; unknown to older firmware, which ignores them).
  // Orientation: 0 landscape, 1 portrait, 2 landscape 180°, 3 portrait 180°.
  uint8_t orient = 0;
  uint8_t cleanMode = 0;       // 0 = manual clean only, 1 = auto clean
  uint8_t cleanEvery = 0;      // auto clean every N screens (0 = off)
  uint8_t uiLang = 1;          // chrome language: 0 English, 1 Russian
  uint32_t doneMask = 0;       // bit i = lesson #i of the course finished

  uint32_t calendarDate = 0;
  StudySession session;
  LessonState lessons[cfg::MAX_LESSONS];
  bool stableLessons = false;
  bool saveFailed = false;

  void bindCourse(const CourseCatalog& catalog) {
    if (courseId[0] && strcmp(courseId, catalog.course.id) != 0) reset();
    if (!stableLessons) {
      for (uint8_t i=0;i<catalog.course.lessonCount;++i) {
        const auto& m=catalog.course.lessons[i];
        if (m.legacyIndex<32 && (doneMask & (1UL<<m.legacyIndex))) state(m.id)->stage=1;
      }
      stableLessons=true;
    }
    strncpy(courseId,catalog.course.id,sizeof(courseId)-1);
  }
  LessonState* state(const char* id) {
    for(auto& s:lessons) if(strcmp(s.id,id)==0) return &s;
    for(auto& s:lessons) if(!s.id[0]) { strncpy(s.id,id,sizeof(s.id)-1); return &s; }
    return nullptr;
  }
  uint8_t stage(const char* id) const {
    for(const auto& s:lessons) if(strcmp(s.id,id)==0) return s.stage;
    return 0;
  }
  const LessonMeta* nextLesson(const CourseCatalog& c) const {
    if(session.lesson[0]) { auto m=c.findLesson(session.lesson); if(m) return m; }
    for(uint8_t i=0;i<c.course.lessonCount;++i)
      if(stage(c.course.lessons[i].id)==0) return &c.course.lessons[i];
    return nullptr;
  }
  bool setDate(uint32_t date) {
    int next=dateOrdinal(date), prev=dateOrdinal(calendarDate);
    if(next<0 || (prev>=0 && next<prev)) return false;
    if(prev>=0 && next>prev) {
      time.day=(uint16_t)(time.day+(next-prev)); todayMinutes=0;
    }
    calendarDate=date;
    return true;
  }
  uint16_t confirmedCount() const {
    uint16_t n=0;
    for(uint16_t i=0;i<itemCount;++i) if(items[i].retained && !SrsScheduler::isDue(items[i],time.day)) ++n;
    return n;
  }

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

  bool loadPath(const char* path) {
    File f=SD.open(path,FILE_READ);
    if(!f)return false;
    size_t sz=f.size();
    if(!sz || sz>48*1024) {f.close();return false;}
    char* buf=(char*)malloc(sz+1);
    if(!buf){f.close();return false;}
    size_t got=f.readBytes(buf,sz);f.close();buf[got]=0;
    bool ok=got==sz && parse(buf,got);free(buf);return ok;
  }
  bool load() {
    if(SD.exists(cfg::PROGRESS_FILE) && loadPath(cfg::PROGRESS_FILE)) return true;
    if(SD.exists("/lingoink/progress.bak")) return loadPath("/lingoink/progress.bak");
    return !SD.exists(cfg::PROGRESS_FILE);
  }
  bool save() {
    // Keep the previous complete file until replacement succeeds. Recovery
    // also works if power is lost between the two renames.
    if(SD.exists(cfg::PROGRESS_TMP)) SD.remove(cfg::PROGRESS_TMP);
    File f=SD.open(cfg::PROGRESS_TMP,FILE_WRITE);
    if(!f)return false;
    bool ok=write(f);f.flush();f.close();
    if(!ok)return false;
    const char* backup="/lingoink/progress.bak";
    if(SD.exists(cfg::PROGRESS_FILE)) {
      if(SD.exists(backup) && !SD.remove(backup))return false;
      if(!SD.rename(cfg::PROGRESS_FILE,backup))return false;
    }
    if(SD.rename(cfg::PROGRESS_TMP,cfg::PROGRESS_FILE))return true;
    if(SD.exists(backup))SD.rename(backup,cfg::PROGRESS_FILE);
    return false;
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

  // Settings change helper: auto clean is only active in mode 1 with N > 0.
  uint8_t autoCleanScreens() const {
    return (cleanMode == 1) ? cleanEvery : 0;
  }

  // Per-lesson completion marks (bitmask over course lesson index).
  bool lessonDone(uint8_t idx) const {
    return idx < cfg::MAX_LESSONS && ((doneMask >> idx) & 1u) != 0;
  }

  void markLessonDone(uint8_t idx) {
    if (idx < cfg::MAX_LESSONS) doneMask |= (1UL << idx);
  }

  // Wipe all user state back to factory defaults (SRS, mastery, resume
  // pointer, counters AND settings). Courses on the SD are untouched.
  void reset() {
    courseId[0] = 0;
    lastLessonId[0] = 0;
    goalMinutes = cfg::GOAL_MINUTES_DEFAULT;
    todayMinutes = 0;
    streakDays = 0;
    strncpy(currentLevel, "A2", sizeof(currentLevel) - 1);
    currentLevel[sizeof(currentLevel) - 1] = 0;
    orient = 0;
    cleanMode = 0;
    cleanEvery = 0;
    uiLang = 1;
    doneMask = 0;
    time = DayTime{};
    calendarDate=0; session=StudySession{}; stableLessons=false;
    for(auto& s:lessons) s=LessonState{};
    mastery = Mastery{};
    itemCount = 0;
  }

  // Duration is display-only and never advances the calendar.
  void addMinutes(uint16_t m) {
    uint32_t t=(uint32_t)todayMinutes+m;
    todayMinutes=(uint16_t)(t>600?600:t);
  }

 private:
  friend struct StudyTests;
  bool write(File& f);
  bool parse(const char* json, size_t len);
};
