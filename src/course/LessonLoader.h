#pragma once
// LessonLoader parses one lesson JSON file into a caller-owned Lesson.
// Uses ArduinoJson in streaming/filtering mode; everything is copied into
// the Lesson string pool so the JSON document can be freed right after.

#include "Types.h"

// Reads `path` (Arduino FS File API) and fills `lesson`. On failure returns
// a result with a human-readable error.
LessonParseResult lessonLoadFile(Lesson& lesson, const char* path);

// Parses JSON text already in memory (used by tests).
LessonParseResult lessonParse(Lesson& lesson, const char* json, size_t len);
