#pragma once
// Settings screen: UI language, manual screen clean, progress reset.
// The device is portrait-180 only (user decision) — there is no orientation
// item; auto-clean machinery was removed too (the panel is wiped at power
// off and on demand). Values live in ProgressStore (persisted in
// progress.json) and apply immediately. Back returns Home. Rows scroll
// cyclically; the reset confirmation uses the shared chrome:: modal.

#include "Screen.h"
#include "../ui/Chrome.h"
#include "../ui/Canvas.h"
#include "../ui/FontRegistry.h"
#include "../ui/Presenter.h"
#include "../ui/Strings.h"
#include "../progress/ProgressStore.h"

class SettingsScreen : public Screen {
 public:
  void bind(ProgressStore* progress, Canvas* canvas, Presenter* presenter) {
    progress_ = progress;
    canvas_ = canvas;
    presenter_ = presenter;
  }

  bool takeDate() {bool a=date_;date_=false;return a;}

  void render(Canvas& c) override {
    const chrome::Metrics mt = chrome::metrics(c);

    if (state_ == State::ConfirmReset) {
      chrome::modal(c, mt, S(StResetProgress), S(ResetNote), nullptr,
                    S(ResetDo), S(Cancel));
      return;
    }

    chrome::header(c, mt, S(SettingsTitle));

    const int gap = 8;
    const int block = mt.rowH + gap;
    int y = mt.top + 6;
    for (int i = 0; i < ITEM_COUNT; i++) {
      char value[40];
      itemValue(i, value, sizeof(value));
      chrome::settingRow(c, mt, y, itemLabel(i), value, i == selected_);
      if (i < ITEM_COUNT - 1 && i != selected_ && i + 1 != selected_) {
        chrome::separator(c, mt, y + mt.rowH - 10);
      }
      y += block;
    }
  }

  Nav handleKey(Key k) override {
    if (state_ == State::ConfirmReset) {
      if (k == Key::Ok) {
        progress_->reset();
        progress_->save();
        Strings::setLang(progress_->uiLang);
        presenter_->fullNext();  // clean slate on the panel too
        state_ = State::Menu;
        selected_ = 0;
        return Nav::RedrawFull;
      }
      if (k == Key::Back) {
        state_ = State::Menu;
        return Nav::RedrawFull;
      }
      return Nav::Stay;
    }

    switch (k) {
      case Key::Up:
      case Key::Left:
        selected_ = (selected_ + ITEM_COUNT - 1) % ITEM_COUNT;  // cyclic
        return Nav::RedrawFast;
      case Key::Down:
      case Key::Right:
        selected_ = (selected_ + 1) % ITEM_COUNT;  // cyclic
        return Nav::RedrawFast;
      case Key::Ok:
        return activate();
      case Key::Back:
        return Nav::Done;
      default:
        return Nav::Stay;
    }
  }

 private:
  enum class State : uint8_t { Menu, ConfirmReset };
  static const int ITEM_COUNT = 5;  // LANGUAGE / CLEAN NOW / RESET / BACK

  Nav activate() {
    switch (selected_) {
      case 0: {  // LANGUAGE
        progress_->uiLang = progress_->uiLang ? 0 : 1;
        progress_->save();
        Strings::setLang(progress_->uiLang);
        return Nav::RedrawFull;
      }
      case 1:  // CLEAN SCREEN NOW: one full refresh on the next present()
        presenter_->fullNext();
        return Nav::RedrawFull;
      case 2:  // RESET PROGRESS
        state_ = State::ConfirmReset;
        return Nav::RedrawFull;
      case 3: date_=true;return Nav::Done;
      default:  // BACK
        return Nav::Done;
    }
  }

  const char* itemLabel(int i) {
    switch (i) {
      case 0: return S(StLanguage);
      case 1: return S(StCleanNow);
      case 2: return S(StResetProgress);
      case 3: return U("TODAY'S DATE","ДАТА СЕГОДНЯ");
      default: return S(StBack);
    }
  }

  void itemValue(int i, char* buf, size_t n) {
    buf[0] = 0;
    if (i == 0) {
      snprintf(buf, n, progress_->uiLang ? "Русский" : "English");
    }
  }

  ProgressStore* progress_ = nullptr;
  Canvas* canvas_ = nullptr;
  Presenter* presenter_ = nullptr;
  bool date_=false;
  int selected_ = 0;
  State state_ = State::Menu;
};
