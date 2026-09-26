#pragma once
// Minimal screen contract. Screens render into the shared Canvas and react
// to one Key at a time; navigation is expressed through the returned action.

#include "../ui/Canvas.h"
#include "../input/Keys.h"

enum class Nav : uint8_t {
  Stay,       // nothing changed
  RedrawFast, // selection moved — fast refresh
  RedrawFull, // content changed — full refresh
  Done,       // finish this screen, pop back
};

class Screen {
 public:
  virtual ~Screen() {}
  virtual void render(Canvas& c) = 0;
  virtual Nav handleKey(Key k) = 0;
};
