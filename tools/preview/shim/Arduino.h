#pragma once
// Host shim of the Arduino core subset used by LingoInk headers.
// Preview only — never compiled into firmware (tools/preview).
// millis() is a constant so option shuffling and battery throttling are
// deterministic between preview runs.

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

inline uint32_t millis() { return 12345u; }
inline void delay(uint32_t) {}
