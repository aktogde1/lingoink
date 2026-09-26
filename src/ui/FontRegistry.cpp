#include "FontRegistry.h"

#include "fonts/title36.h"
#include "fonts/ui22.h"
#include "fonts/body28.h"
#include "fonts/body28b.h"
#include "fonts/display56.h"

const LgFont* fontByRole(FontRole role) {
  switch (role) {
    case FontRole::Title:
      return &title36;
    case FontRole::UI:
      return &ui22;
    case FontRole::Body:
      return &body28;
    case FontRole::Display:
      return &display56;
    case FontRole::BodyBold:
    default:
      return &body28b;
  }
}
