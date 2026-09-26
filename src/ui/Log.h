#pragma once
// Serial logging for hardware validation. Gated by LINGOINK_LOG (default on
// for the validation build); costs nothing on the UI path — one printf per
// event. Tags: BOOT, HW, SD, CRS, APP, KEY, EX, PRG, ERR.

#ifdef ARDUINO

#include <Arduino.h>

#if !defined(LINGOINK_LOG)
#define LINGOINK_LOG 1
#endif

#if LINGOINK_LOG
#define LI_LOG(tag, fmt, ...) \
  Serial.printf("[%lus][%-3s] " fmt "\n", (unsigned long)(millis() / 1000), tag, ##__VA_ARGS__)
#else
#define LI_LOG(tag, fmt, ...) do {} while (0)
#endif

#else
#define LI_LOG(tag, fmt, ...) do {} while (0)
#endif

#define LOGI(tag, fmt, ...) LI_LOG(tag, fmt, ##__VA_ARGS__)
#define LOGW(tag, fmt, ...) LI_LOG(tag, "WARN: " fmt, ##__VA_ARGS__)
#define LOGE(tag, fmt, ...) LI_LOG(tag, "ERROR: " fmt, ##__VA_ARGS__)

#ifdef ARDUINO
#include <esp_heap_caps.h>
// Heap fragmentation diagnostics: total free vs largest contiguous block.
inline void lgHeapDiag(const char* where) {
  Serial.printf("[%lus][MEM ] %s: free=%u largest=%u\n",
                (unsigned long)(millis() / 1000), where,
                (unsigned)ESP.getFreeHeap(),
                (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
}
#else
inline void lgHeapDiag(const char*) {}
#endif
