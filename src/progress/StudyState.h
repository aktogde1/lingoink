#pragma once
#include <stdint.h>
#include <string.h>
#include "../config.h"

// Indices in a session refer to one immutable lesson. A content fingerprint
// invalidates only the bookmark when that lesson changes, never learned items.
//
// Cross-lesson review queue (additive in progress v2; older firmware without
// the fields loads the session exactly as before): qLesson/qEx hold up to 12
// (course lesson index, exercise index) entries — one "Повторить" tap builds
// the queue from due items across all studied lessons, and the session walks
// it slice by slice (one lesson loaded at a time). qAnswers/qCorrect are the
// whole-queue totals, qUnresolved a bit per queue entry, qBase the queue
// index of the slice currently in plan[].
struct StudySession {
  char lesson[32] = "";
  uint32_t fingerprint = 0;
  uint8_t phase = 0, theory = 0, line = 0, page = 0;
  uint16_t offset = 0;
  uint8_t plan[96] = {}, length = 0, cursor = 0;
  uint8_t attempts[cfg::MAX_EXERCISES] = {};
  uint16_t answers = 0, correct = 0;
  uint64_t first = 0, unresolved = 0;
  bool answered = false, helped = false, revealed = false, review = false;
  uint8_t chosen = 0;
  uint8_t qLesson[12] = {}, qEx[12] = {};
  uint8_t qLen = 0, qPos = 0, qBase = 0;
  uint16_t qAnswers = 0, qCorrect = 0;
  uint64_t qUnresolved = 0;
};
struct LessonState {
  char id[32] = "";
  uint8_t stage = 0; // 1 viewed; 2 >=80% independent first attempts
  uint16_t day = 0;
};

inline bool leapYear(int y) { return y % 4 == 0 && (y % 100 != 0 || y % 400 == 0); }
inline int monthDays(int y, int m) {
  const uint8_t days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
  return days[m-1] + (m == 2 && leapYear(y));
}
// Date is YYYYMMDD. Date entry is deliberately explicit: no fake uptime days.
inline int dateOrdinal(uint32_t date) {
  int y=date/10000, m=(date/100)%100, d=date%100;
  if(y<2020 || y>2099 || m<1 || m>12 || d<1 || d>monthDays(y,m)) return -1;
  int n=0;
  for(int a=2020;a<y;++a) n+=leapYear(a)?366:365;
  for(int a=1;a<m;++a) n+=monthDays(y,a);
  return n+d-1;
}
