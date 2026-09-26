#include "FontRegistry.h"

#include "fonts/title36.h"
#include "fonts/ui20.h"
#include "fonts/body28.h"
#include "fonts/body28b.h"

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
