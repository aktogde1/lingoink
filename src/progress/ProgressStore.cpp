#include "ProgressStore.h"

#include <ArduinoJson.h>

bool ProgressStore::write(File& f) {
  JsonDocument doc;
  doc["v"] = 2;
  doc["date"] = calendarDate;
  JsonArray states=doc["lessonStates"].to<JsonArray>();
  for(const auto& s:lessons) if(s.id[0]) {
    auto o=states.add<JsonObject>(); o["id"]=s.id; o["stage"]=s.stage; o["day"]=s.day;
  }
  const StudySession& s=session;
  JsonObject b=doc["session"].to<JsonObject>();
  b["lesson"]=s.lesson; b["hash"]=s.fingerprint;
  b["phase"]=s.phase; b["theory"]=s.theory; b["line"]=s.line;
  b["page"]=s.page; b["offset"]=s.offset; b["cursor"]=s.cursor;
  b["answers"]=s.answers; b["correct"]=s.correct;
  b["firstLo"]=(uint32_t)s.first; b["firstHi"]=(uint32_t)(s.first>>32);
  b["wrongLo"]=(uint32_t)s.unresolved; b["wrongHi"]=(uint32_t)(s.unresolved>>32);
  b["answered"]=s.answered; b["helped"]=s.helped; b["revealed"]=s.revealed;
  b["review"]=s.review; b["chosen"]=s.chosen; b["gate"]=s.gate;
  b["qLen"]=s.qLen; b["qPos"]=s.qPos; b["qBase"]=s.qBase;
  b["qA"]=s.qAnswers; b["qC"]=s.qCorrect;
  b["qUnresLo"]=(uint32_t)s.qUnresolved; b["qUnresHi"]=(uint32_t)(s.qUnresolved>>32);
  JsonArray ql=b["qLessons"].to<JsonArray>();
  for(uint8_t i=0;i<s.qLen;++i) ql.add(s.qLesson[i]);
  JsonArray qx=b["qEx"].to<JsonArray>();
  for(uint8_t i=0;i<s.qLen;++i) qx.add(s.qEx[i]);
  auto plan=b["plan"].to<JsonArray>(); for(uint8_t i=0;i<s.length;++i) plan.add(s.plan[i]);
  auto attempts=b["attempts"].to<JsonArray>(); for(auto a:s.attempts) attempts.add(a);
  doc["course"] = courseId;
  doc["lastLesson"] = lastLessonId;
  doc["level"] = currentLevel;
  doc["goalMin"] = goalMinutes;
  doc["todayMin"] = todayMinutes;
  doc["streak"] = streakDays;
  doc["day"] = time.day;
  doc["uptimeMin"] = time.uptimeMin;
  doc["orient"] = orient;
  doc["cleanMode"] = cleanMode;
  doc["cleanEvery"] = cleanEvery;
  doc["uiLang"] = uiLang;
  doc["done"] = doneMask;

  JsonArray srs = doc["srs"].to<JsonArray>();
  for (uint16_t i = 0; i < itemCount; i++) {
    const SrsItem& it = items[i];
    JsonObject o = srs.add<JsonObject>();
    o["id"] = it.id;
    o["e"] = it.easePct;
    o["retained"] = it.retained;
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

  return serializeJson(doc, f)==measureJson(doc);
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
  orient = (doc["orient"] | 0) % 4;
  cleanMode = doc["cleanMode"] | 0;
  cleanEvery = doc["cleanEvery"] | 0;
  uiLang = (doc["uiLang"] | 1) ? 1 : 0;
  doneMask = doc["done"] | 0;

  calendarDate=doc["date"] | 0;
  if(dateOrdinal(calendarDate)<0) calendarDate=0;
  stableLessons=doc["lessonStates"].is<JsonArrayConst>();
  for(auto& s:lessons) s=LessonState{};
  for(auto o:doc["lessonStates"].as<JsonArrayConst>()) {
    const char* id=o["id"] | "";
    if(!id[0]) continue;
    auto s=state(id); if(!s) break;
    s->stage=o["stage"] | 0; if(s->stage>2) s->stage=2;
    s->day=o["day"] | 0;
  }
  session=StudySession{};
  auto b=doc["session"].as<JsonObjectConst>();
  auto& s=session;
  strlcpy(s.lesson,b["lesson"] | "",sizeof(s.lesson));
  s.fingerprint=b["hash"] | 0u;
  s.phase=b["phase"] | 0; s.theory=b["theory"] | 0; s.line=b["line"] | 0;
  s.page=b["page"] | 0; s.offset=b["offset"] | 0; s.cursor=b["cursor"] | 0;
  s.answers=b["answers"] | 0; s.correct=b["correct"] | 0;
  s.first=(uint64_t)(b["firstLo"] | 0u) | ((uint64_t)(b["firstHi"] | 0u)<<32);
  s.unresolved=(uint64_t)(b["wrongLo"] | 0u) | ((uint64_t)(b["wrongHi"] | 0u)<<32);
  s.answered=b["answered"] | false; s.helped=b["helped"] | false;
  s.revealed=b["revealed"] | false; s.review=b["review"] | false;
  s.gate=b["gate"] | false;
  s.chosen=b["chosen"] | 0;
  for(auto v:b["plan"].as<JsonArrayConst>()) { if(s.length>=96) break; s.plan[s.length++]=v.as<uint8_t>(); }
  uint8_t ai=0; for(auto v:b["attempts"].as<JsonArrayConst>()) { if(ai>=cfg::MAX_EXERCISES) break; s.attempts[ai++]=v.as<uint8_t>(); }
  s.qLen=(uint8_t)(b["qLen"] | 0); if(s.qLen>12) s.qLen=12;
  s.qPos=(uint8_t)(b["qPos"] | 0); if(s.qPos>s.qLen) s.qPos=s.qLen;
  s.qBase=(uint8_t)(b["qBase"] | 0);
  s.qAnswers=b["qA"] | 0; s.qCorrect=b["qC"] | 0;
  s.qUnresolved=(uint64_t)(b["qUnresLo"] | 0u) | ((uint64_t)(b["qUnresHi"] | 0u)<<32);
  uint8_t qi=0; for(auto v:b["qLessons"].as<JsonArrayConst>()) { if(qi>=s.qLen) break; s.qLesson[qi++]=v.as<uint8_t>(); }
  qi=0; for(auto v:b["qEx"].as<JsonArrayConst>()) { if(qi>=s.qLen) break; s.qEx[qi++]=v.as<uint8_t>(); }
  if(s.cursor>s.length || (s.cursor==s.length && s.phase!=5) || s.phase<2 || s.phase>5) session=StudySession{};

  itemCount = 0;
  for (JsonVariantConst o : doc["srs"].as<JsonArrayConst>()) {
    if (itemCount >= MAX_SRS) break;
    SrsItem& it = items[itemCount++];
    strlcpy(it.id, o["id"] | "", sizeof(it.id));
    it.easePct = o["e"] | 250;
    if(it.easePct<130 || it.easePct>280)it.easePct=250;
    it.intervalDays = o["iv"] | 0;
    it.retained=o["retained"] | false;
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
