#include "LessonLoader.h"

#include <ArduinoJson.h>

#ifdef PLATFORM_ESP32
#include <SD.h>
#endif

const char* Lesson::intern(const char* s, int len) {
  if (!s) return nullptr;
  if (len < 0) {
    len = (int)strlen(s);
  }
  if (poolUsed + len + 1 > (uint16_t)sizeof(pool)) {
    return ""; // pool exhausted — loader reports it separately via error
  }
  char* dst = pool + poolUsed;
  memcpy(dst, s, len);
  dst[len] = 0;
  poolUsed = (uint16_t)(poolUsed + len + 1);
  return dst;
}

namespace {

void copyStr(JsonVariantConst v, const char** dst, Lesson& L, const char* field,
             LessonParseResult& res, bool required) {
  if (v.isNull() || !v.is<const char*>()) {
    if (required) {
      snprintf(res.error, sizeof(res.error), "missing field '%s'", field);
      res.ok = false;
    }
    return;
  }
  const char* s = v.as<const char*>();
  const char* pooled = L.intern(s);
  if (*pooled == 0 && *s != 0) {
    snprintf(res.error, sizeof(res.error), "string pool exhausted at '%s'", field);
    res.ok = false;
    return;
  }
  *dst = pooled;
}

void parseCommon(JsonVariantConst ex, Exercise& E, Lesson& L) {
  E.srsCount = 0;
  for (JsonVariantConst id : ex["srs"].as<JsonArrayConst>()) {
    if (E.srsCount >= cfg::MAX_SRS_PER_EX) break;
    if (id.is<const char*>()) {
      E.srsIds[E.srsCount++] = L.intern(id.as<const char*>());
    }
  }
  E.tagCount = 0;
  for (JsonVariantConst t : ex["tags"].as<JsonArrayConst>()) {
    if (E.tagCount >= cfg::MAX_TAGS_PER_EX) break;
    if (t.is<const char*>()) {
      E.tags[E.tagCount++] = L.intern(t.as<const char*>());
    }
  }
}

bool parseOptions(JsonVariantConst ex, Exercise& E, Lesson& L,
                  LessonParseResult& res) {
  E.optionCount = 0;
  JsonArrayConst opts = ex["options"].as<JsonArrayConst>();
  if (opts.isNull() || opts.size() == 0) {
    snprintf(res.error, sizeof(res.error), "exercise has no options");
    return false;
  }
  for (JsonVariantConst o : opts) {
    if (E.optionCount >= cfg::MAX_OPTIONS) break;
    if (!o.is<const char*>()) continue;
    E.options[E.optionCount] = L.intern(o.as<const char*>());
    E.optionCount++;
  }
  if (E.optionCount < 2) {
    snprintf(res.error, sizeof(res.error), "exercise needs >=2 options");
    return false;
  }
  int c = ex["correct"] | -1;
  if (c < 0 || c >= (int)E.optionCount) {
    snprintf(res.error, sizeof(res.error), "bad 'correct' index");
    return false;
  }
  E.correct = (uint8_t)c;
  return true;
}

} // namespace

// Contract: `L` must be zeroed by the caller before the first parse
// (the struct is reused across lessons; exercise slots are re-zeroed here).
LessonParseResult lessonParse(Lesson& L, const char* json, size_t len) {
  LessonParseResult res;
  res.ok = true;
  res.error[0] = 0;

  L.poolUsed = 0;

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, json, len);
  if (err) {
    snprintf(res.error, sizeof(res.error), "JSON: %s", err.c_str());
    res.ok = false;
    return res;
  }

  // id/title/level are fixed char arrays in the Lesson struct.
  if (!res.ok) return res;
  const char* id = doc["id"] | "";
  const char* title = doc["title"] | "";
  const char* level = doc["level"] | "";
  strncpy(L.id, id, sizeof(L.id) - 1);
  strncpy(L.title, title, sizeof(L.title) - 1);
  strncpy(L.level, level, sizeof(L.level) - 1);
  if (L.id[0] == 0) {
    snprintf(res.error, sizeof(res.error), "lesson has no id");
    res.ok = false;
    return res;
  }

  const char* kind = doc["kind"] | "mixed";
  L.kind = LessonKind::Mixed;
  if (strcmp(kind, "vocab") == 0) L.kind = LessonKind::Vocab;
  else if (strcmp(kind, "grammar") == 0) L.kind = LessonKind::Grammar;
  else if (strcmp(kind, "reading") == 0) L.kind = LessonKind::Reading;

  // Theory blocks.
  L.theoryCount = 0;
  for (JsonVariantConst tb : doc["theory"].as<JsonArrayConst>()) {
    if (L.theoryCount >= 4) break;
    TheoryBlock& B = L.theory[L.theoryCount];
    B.lineCount = 0;
    B.title = nullptr;
    copyStr(tb["title"], &B.title, L, "theory.title", res, true);
    if (!res.ok) return res;
    for (JsonVariantConst ln : tb["lines"].as<JsonArrayConst>()) {
      if (B.lineCount >= cfg::MAX_THEORY_LINES) break;
      if (ln.is<const char*>()) {
        B.lines[B.lineCount] = L.intern(ln.as<const char*>());
        if (B.lines[B.lineCount][0] == 0 && ln.as<const char*>()[0] != 0) {
          snprintf(res.error, sizeof(res.error), "pool exhausted in theory");
          res.ok = false;
          return res;
        }
        B.lineCount++;
      }
    }
    L.theoryCount++;
  }

  // Exercises.
  L.exerciseCount = 0;
  for (JsonVariantConst ex : doc["exercises"].as<JsonArrayConst>()) {
    if (L.exerciseCount >= cfg::MAX_EXERCISES) break;
    Exercise& E = L.exercises[L.exerciseCount];
    memset(&E, 0, sizeof(E));
    E.prompt = nullptr;
    E.promptSmall = nullptr;
    E.explain = nullptr;
    E.readingTitle = nullptr;
    E.pageCount = 0;

    const char* type = ex["type"] | "choice";
    if (strcmp(type, "cloze") == 0) E.type = ExType::Cloze;
    else if (strcmp(type, "mistake") == 0) E.type = ExType::Mistake;
    else if (strcmp(type, "reading") == 0) E.type = ExType::Reading;
    else if (strcmp(type, "recall") == 0) E.type = ExType::Recall;
    else E.type = ExType::Choice;

    parseCommon(ex, E, L);
    E.reviewable = ex["reviewable"] | true;
    copyStr(ex["skill"], &E.skill, L, "skill", res, false);
    if (!res.ok) return res;

    if (E.type == ExType::Reading) {
      copyStr(ex["title"], &E.readingTitle, L, "reading.title", res, true);
      if (!res.ok) return res;
      for (JsonVariantConst pg : ex["pages"].as<JsonArrayConst>()) {
        if (E.pageCount >= cfg::MAX_READING_PAGES) break;
        if (pg.is<const char*>()) {
          E.pages[E.pageCount] = L.intern(pg.as<const char*>());
          if (E.pages[E.pageCount][0] == 0 && pg.as<const char*>()[0] != 0) {
            snprintf(res.error, sizeof(res.error), "pool exhausted in reading page");
            res.ok = false;
            return res;
          }
          E.pageCount++;
        }
      }
      if (E.pageCount == 0) {
        snprintf(res.error, sizeof(res.error), "reading has no pages");
        res.ok = false;
        return res;
      }
    } else {
      copyStr(ex["prompt"], &E.prompt, L, "prompt", res, true);
      if (!res.ok) return res;
      if (!ex["promptSmall"].isNull()) {
        copyStr(ex["promptSmall"], &E.promptSmall, L, "promptSmall", res, false);
      }
      if (!ex["explain"].isNull()) {
        copyStr(ex["explain"], &E.explain, L, "explain", res, false);
      }
      if (E.type == ExType::Recall) {
        copyStr(ex["answer"], &E.answer, L, "answer", res, true);
        if (!res.ok || !E.answer || !E.answer[0]) { res.ok = false; return res; }
        E.optionCount = 2;
        E.correct = 1;
        E.options[0] = "Not yet";
        E.options[1] = "Recalled";
      } else if (!parseOptions(ex, E, L, res)) {
        res.ok = false;
        return res;
      }
    }
    L.exerciseCount++;
  }

  if (L.exerciseCount == 0 && L.theoryCount == 0) {
    snprintf(res.error, sizeof(res.error), "lesson has no exercises or theory");
    res.ok = false;
  }
  return res;
}

#ifdef PLATFORM_ESP32
// Static scratch buffer for the raw JSON (the Lesson pool is rebuilt during
// parsing, so it cannot double as the read target).
static char s_jsonBuf[cfg::JSON_BUF_BYTES];

LessonParseResult lessonLoadFile(Lesson& L, const char* path) {
  LessonParseResult res;
  res.ok = false;
  snprintf(res.error, sizeof(res.error), "open failed: %s", path);
  File f = SD.open(path, FILE_READ);
  if (!f) {
    return res;
  }
  size_t sz = f.size();
  if (sz > sizeof(s_jsonBuf)) {
    f.close();
    snprintf(res.error, sizeof(res.error), "lesson file too large (%u bytes)", (unsigned)sz);
    return res;
  }
  size_t got = f.readBytes(s_jsonBuf, sz);
  f.close();
  if (got != sz) {
    snprintf(res.error, sizeof(res.error), "short read on %s", path);
    return res;
  }
  memset(&L, 0, sizeof(L));
  res = lessonParse(L, s_jsonBuf, sz);
  return res;
}
#endif
