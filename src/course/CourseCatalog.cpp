#include "CourseCatalog.h"

#include <ArduinoJson.h>

bool CourseCatalog::loadManifest(const char* manifestPath) {
  File f = SD.open(manifestPath, FILE_READ);
  if (!f) return false;
  size_t sz = f.size();
  if (sz > 16 * 1024) {
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

  JsonDocument doc;
  bool ok = false;
  if (!deserializeJson(doc, buf, got)) {
    strlcpy(course.id, doc["id"] | "", sizeof(course.id));
    strlcpy(course.title, doc["title"] | "", sizeof(course.title));
    strlcpy(course.from, doc["from"] | "", sizeof(course.from));
    strlcpy(course.to, doc["to"] | "", sizeof(course.to));
    if (course.id[0]) {
      // dir is /courses/<id>
      snprintf(course.dir, sizeof(course.dir), "%s/%s", cfg::COURSE_DIR, course.id);
      course.lessonCount = 0;
      for (JsonVariantConst l : doc["lessons"].as<JsonArrayConst>()) {
        if (course.lessonCount >= cfg::MAX_LESSONS) break;
        LessonMeta& m = course.lessons[course.lessonCount];
        strlcpy(m.id, l["id"] | "", sizeof(m.id));
        const char* file = l["file"] | "";
        snprintf(m.file, sizeof(m.file), "%s/%s", course.dir, file);
        strlcpy(m.title, l["title"] | "", sizeof(m.title));
        strlcpy(m.level, l["level"] | "", sizeof(m.level));
        const char* kind = l["kind"] | "mixed";
        m.kind = LessonKind::Mixed;
        if (strcmp(kind, "vocab") == 0) m.kind = LessonKind::Vocab;
        else if (strcmp(kind, "grammar") == 0) m.kind = LessonKind::Grammar;
        else if (strcmp(kind, "reading") == 0) m.kind = LessonKind::Reading;
        if (m.id[0] && m.file[0]) {
          course.lessonCount++;
        }
      }
      ok = course.lessonCount > 0;
    }
  }
  free(buf);
  return ok;
}
