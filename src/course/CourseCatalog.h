#pragma once
// CourseCatalog scans /courses/<dir>/course.json manifests on the SD card
// and keeps the lesson index in RAM. MVP ships one course; the format is
// multi-course ready (en-ru, ru-de, en-es, ...).

#include "../config.h"
#include "Types.h"
#include <SD.h>

struct LessonMeta {
  char id[32];
  char file[80];   // path relative to SD root, e.g. /courses/english_ru/lessons/x.json
  char title[64];
  char level[8];
  LessonKind kind;
};

struct CourseMeta {
  char dir[48];    // /courses/english_ru
  char id[24];
  char title[48];
  char from[8];    // "ru"
  char to[8];      // "en"
  uint8_t lessonCount;
  LessonMeta lessons[cfg::MAX_LESSONS];
};

class CourseCatalog {
 public:
  CourseMeta course;

  // Finds the first directory with a valid course.json under COURSE_DIR.
  bool begin() {
    strncpy(course.dir, "", sizeof(course.dir));
    course.lessonCount = 0;
    File root = SD.open(cfg::COURSE_DIR);
    if (!root) return false;
    File d;
    while ((d = root.openNextFile())) {
      if (!d.isDirectory()) continue;
      char manifest[96];
      snprintf(manifest, sizeof(manifest), "%s/%s/course.json", cfg::COURSE_DIR, d.name());
      d.close();
      if (SD.exists(manifest)) {
        return loadManifest(manifest);
      }
    }
    root.close();
    return false;
  }

  const LessonMeta* findLesson(const char* id) const {
    for (uint8_t i = 0; i < course.lessonCount; i++) {
      if (strncmp(course.lessons[i].id, id, sizeof(LessonMeta::id)) == 0) {
        return &course.lessons[i];
      }
    }
    return nullptr;
  }

  // Next lesson after `id`, or the first lesson when id is empty/null.
  const LessonMeta* nextAfter(const char* id) const {
    if (!id || !id[0]) return course.lessonCount ? &course.lessons[0] : nullptr;
    for (uint8_t i = 0; i < course.lessonCount; i++) {
      if (strncmp(course.lessons[i].id, id, sizeof(LessonMeta::id)) == 0) {
        if (i + 1 < course.lessonCount) return &course.lessons[i + 1];
        return nullptr; // course finished
      }
    }
    return course.lessonCount ? &course.lessons[0] : nullptr;
  }

 private:
  bool loadManifest(const char* manifestPath);
};
