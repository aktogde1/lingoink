#pragma once
// Font roles used by screens. Fonts live in flash (generated headers).

#include "LgFont.h"

enum class FontRole : uint8_t {
  Title,    // screen titles (36px semibold)
  UI,       // menu rows, footers, labels (22px semibold)
  Body,     // exercise prompts, reading text (28px)
  BodyBold, // emphasized prompt line (28px semibold)
  Display,  // poster numerals / power-card headline (56px bold)
};

const LgFont* fontByRole(FontRole role);
