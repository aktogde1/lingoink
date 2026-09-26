#include "FontRegistry.h"

extern const LgFont title36;
extern const LgFont ui20;
extern const LgFont body28;
extern const LgFont body28b;

const LgFont* fontByRole(FontRole role) {
  switch (role) {
    case FontRole::Title:
      return &title36;
    case FontRole::UI:
      return &ui20;
    case FontRole::Body:
      return &body28;
    case FontRole::BodyBold:
    default:
      return &body28b;
  }
}
