#pragma once
// Font roles used by screens. Fonts live in flash (generated headers).

#include "LgFont.h"

enum class FontRole : uint8_t {
  Title,    // screen titles (36px semibold)
  UI,       // menu rows, footers, labels (20px)
  Body,     // exercise prompts, reading text (28px)
  BodyBold, // emphasized prompt line (28px semibold)
};

const LgFont* fontByRole(FontRole role);
