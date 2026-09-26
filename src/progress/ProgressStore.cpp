#include "ProgressStore.h"

#include <ArduinoJson.h>

void ProgressStore::write(File& f) {
  JsonDocument doc;
  doc["v"] = 1;
  doc["course"] = courseId;
  doc["lastLesson"] = lastLessonId;
  doc["level"] = currentLevel;
  doc["goalMin"] = goalMinutes;
  doc["todayMin"] = todayMinutes;
  doc["streak"] = streakDays;
  doc["day"] = time.day;
  doc["uptimeMin"] = time.uptimeMin;

  JsonArray srs = doc["srs"].to<JsonArray>();
  for (uint16_t i = 0; i < itemCount; i++) {
    const SrsItem& it = items[i];
    JsonObject o = srs.add<JsonObject>();
    o["id"] = it.id;
    o["e"] = it.easePct;
    o["iv"] = it.intervalDays;
    o["r"] = it.reps;
    o["l"] = it.lapses;
    o["ld"] = it.lastDay;
    o["nd"] = it.nextDay;
  }

  JsonObject sk = doc["skills"].to<JsonObject>();
  const char* keys[] = {"voc", "grm", "rdg", "prd"};
  for (int i = 0; i < (int)Skill::Count; i++) {
    JsonObject o = sk[keys[i]].to<JsonObject>();
    o["e"] = (int)(mastery.skills[i].ema * 1000.0f);
    o["n"] = mastery.skills[i].count;
  }

  JsonObject tg = doc["tags"].to<JsonObject>();
  for (int i = 0; i < Mastery::MAX_TAGS; i++) {
    if (!mastery.tags[i].used) continue;
    JsonObject o = tg[mastery.tags[i].tag].to<JsonObject>();
    o["e"] = (int)(mastery.tags[i].stat.ema * 1000.0f);
    o["n"] = mastery.tags[i].stat.count;
  }

  serializeJson(doc, f);
}

bool ProgressStore::parse(const char* json, size_t len) {
  JsonDocument doc;
  if (deserializeJson(doc, json, len)) {
    return false;
  }
  strlcpy(courseId, doc["course"] | "", sizeof(courseId));
  strlcpy(lastLessonId, doc["lastLesson"] | "", sizeof(lastLessonId));
  strlcpy(currentLevel, doc["level"] | "A2", sizeof(currentLevel));
  goalMinutes = doc["goalMin"] | cfg::GOAL_MINUTES_DEFAULT;
  todayMinutes = doc["todayMin"] | 0;
  streakDays = doc["streak"] | 0;
  time.day = doc["day"] | 0;
  time.uptimeMin = doc["uptimeMin"] | 0;

  itemCount = 0;
  for (JsonVariantConst o : doc["srs"].as<JsonArrayConst>()) {
    if (itemCount >= MAX_SRS) break;
    SrsItem& it = items[itemCount++];
    strlcpy(it.id, o["id"] | "", sizeof(it.id));
    it.easePct = o["e"] | 250;
    it.intervalDays = o["iv"] | 0;
    it.reps = o["r"] | 0;
    it.lapses = o["l"] | 0;
    it.lastDay = o["ld"] | 0;
    it.nextDay = o["nd"] | 0;
  }

  const char* keys[] = {"voc", "grm", "rdg", "prd"};
  JsonObjectConst sk = doc["skills"].as<JsonObjectConst>();
  for (int i = 0; i < (int)Skill::Count; i++) {
    JsonObjectConst o = sk[keys[i]];
    if (o.isNull()) continue;
    mastery.skills[i].ema = (o["e"] | 0) / 1000.0f;
    mastery.skills[i].count = o["n"] | 0;
  }

  JsonObjectConst tg = doc["tags"].as<JsonObjectConst>();
  for (JsonPairConst kv : tg) {
    Stat* s = nullptr;
    for (int i = 0; i < Mastery::MAX_TAGS; i++) {
      if (!mastery.tags[i].used) {
        mastery.tags[i].used = true;
        strlcpy(mastery.tags[i].tag, kv.key().c_str(), sizeof(mastery.tags[i].tag));
        s = &mastery.tags[i].stat;
        break;
      }
    }
    if (s) {
      JsonObjectConst o = kv.value();
      s->ema = (o["e"] | 0) / 1000.0f;
      s->count = o["n"] | 0;
    }
  }
  return true;
}
