#pragma once
// LessonScreen runs one lesson end to end:
//   theory pages → exercises (choice/cloze/mistake, reading + QA) → summary.
//
// Grading flows into ProgressStore (SRS items + mastery). The App saves
// progress when the screen finishes.

#include "Screen.h"
#include "../ui/FontRegistry.h"
#include "../ui/Log.h"
#include "../course/LessonLoader.h"
#include "../progress/ProgressStore.h"

class LessonScreen : public Screen {
 public:
  // Loads the lesson from SD. Returns false with a readable error.
  bool start(const LessonMeta& meta, ProgressStore* progress) {
    progress_ = progress;
    meta_ = meta;
    LessonParseResult r = lessonLoadFile(lesson_, meta.file);
    if (!r.ok) {
      strncpy(errText_, r.error, sizeof(errText_) - 1);
      phase_ = Phase::Error;
      return false;
    }
    theoryIdx_ = 0;
    theoryLine_ = 0;
    exIdx_ = 0;
    correctCount_ = 0;
    answerCount_ = 0;
    enterExercise(lesson_.theoryCount > 0 ? Phase::Theory : firstContentPhase());
    return true;
  }

  bool finished() const { return phase_ == Phase::Summary || phase_ == Phase::Error; }
  bool wasError() const { return phase_ == Phase::Error; }
  const char* lessonId() const { return lesson_.id; }
  uint8_t theoryCount() const { return lesson_.theoryCount; }
  uint16_t exerciseCount() const { return lesson_.exerciseCount; }
  const char* errorText() const { return errText_; }
  uint8_t accuracyPct() const {
    if (answerCount_ == 0) return 0;
    return (uint8_t)((correctCount_ * 100 + answerCount_ / 2) / answerCount_);
  }

  void render(Canvas& c) override {
    const int M = cfg::MARGIN;
    const LgFont* title = fontByRole(FontRole::Title);
    const LgFont* ui = fontByRole(FontRole::UI);
    const LgFont* body = fontByRole(FontRole::Body);

    switch (phase_) {
      case Phase::Error:
        c.drawText(M, M + 4, title, "LESSON ERROR");
        c.drawText(M, M + title->advanceY + 20, body, errText_);
        drawFooterHint(c, "press OK");
        break;

      case Phase::Theory: {
        const TheoryBlock& tb = lesson_.theory[theoryIdx_];
        c.drawText(M, M + 4, title, tb.title ? tb.title : "");
        int y = M + title->advanceY + 16;
        for (uint8_t i = theoryLine_; i < tb.lineCount; i++) {
          if (y + body->advanceY > c.height() - 60) break;
          c.drawText(M, y, body, tb.lines[i]);
          y += body->advanceY + 4;
        }
        char p[24];
        snprintf(p, sizeof(p), "RULE %u/%u", (unsigned)(theoryIdx_ + 1),
                 (unsigned)lesson_.theoryCount);
        drawPageFoot(c, p, "OK \xE2\x80\x94 continue");
        break;
      }

      case Phase::Reading: {
        const Exercise& E = cur();
        c.drawText(M, M + 4, title, E.readingTitle ? E.readingTitle : "READING");
        const char* lines[20];
        int lens[20];
        int n = c.wrapText(body, E.pages[pageIdx_], c.width() - 2 * M, lines, lens, 20);
        int y = M + title->advanceY + 16;
        for (int i = 0; i < n; i++) {
          if (y + body->advanceY > c.height() - 60) break;
          c.drawTextN(M, y, body, lines[i], lens[i]);
          y += body->advanceY + 4;
        }
        char p[24];
        snprintf(p, sizeof(p), "PAGE %u/%u", (unsigned)(pageIdx_ + 1), (unsigned)E.pageCount);
        drawPageFoot(c, p,
                     pageIdx_ + 1 < E.pageCount ? "OK \xE2\x80\x94 next page"
                                                : "OK \xE2\x80\x94 questions");
        break;
      }

      case Phase::Exercise: {
        const Exercise& E = cur();
        renderExercise(c, E);
        break;
      }

      case Phase::Summary: {
        char head[32];
        snprintf(head, sizeof(head), "%u%% CORRECT", (unsigned)accuracyPct());
        c.drawText(M, M + 4, title, head);
        int y = M + title->advanceY + 24;
        char line[64];
        snprintf(line, sizeof(line), "%u of %u answers right", (unsigned)correctCount_,
                 (unsigned)answerCount_);
        c.drawText(M, y, fontByRole(FontRole::Body), line);
        y += fontByRole(FontRole::Body)->advanceY + 12;
        c.drawText(M, y, ui, "Review scheduled by spaced repetition.");
        drawFooterHint(c, "OK \xE2\x80\x94 home");
        break;
      }

      case Phase::Load:
        break;
    }
  }

  Nav handleKey(Key k) override {
    switch (phase_) {
      case Phase::Error:
        return Nav::Done;

      case Phase::Summary:
        if (k == Key::Ok || k == Key::Back) return Nav::Done;
        return Nav::Stay;

      case Phase::Theory: {
        if (k == Key::Ok || k == Key::Down || k == Key::Right) {
          const TheoryBlock& tb = lesson_.theory[theoryIdx_];
          // Advance one screenful, then to the next block.
          int y = cfg::MARGIN + fontByRole(FontRole::Title)->advanceY + 16;
          int shown = 0;
          const int lineH = fontByRole(FontRole::Body)->advanceY + 4;
          for (uint8_t i = theoryLine_; i < tb.lineCount; i++) {
            if (y + fontByRole(FontRole::Body)->advanceY > cfg::SCREEN_H - 60) break;
            y += lineH;
            shown++;
          }
          theoryLine_ += (uint8_t)shown;
          if (theoryLine_ >= tb.lineCount) {
            theoryIdx_++;
            theoryLine_ = 0;
            if (theoryIdx_ >= lesson_.theoryCount) {
              enterExercise(Phase::Exercise);
            }
          }
          return Nav::RedrawFull;
        }
        if ((k == Key::Up || k == Key::Left) && theoryIdx_ > 0) {
          theoryIdx_--;
          theoryLine_ = 0;
          return Nav::RedrawFull;
        }
        return Nav::Stay;
      }

      case Phase::Reading: {
        if (k == Key::Down || k == Key::Right || k == Key::Ok) {
          if (pageIdx_ + 1 < cur().pageCount) {
            pageIdx_++;
            return Nav::RedrawFull;
          }
          advanceToNextExercise();
          return Nav::RedrawFull;
        }
        if ((k == Key::Up || k == Key::Left) && pageIdx_ > 0) {
          pageIdx_--;
          return Nav::RedrawFull;
        }
        return Nav::Stay;
      }

      case Phase::Exercise: {
        if (k == Key::OkLong) {
          explainShown_ = !explainShown_;
          LOGI("KEY", "long OK: explain %s", explainShown_ ? "shown" : "hidden");
          return Nav::RedrawFull;
        }
        const Exercise& E = cur();
        if (!answered_) {
          switch (k) {
            case Key::Left:
            case Key::Up:
              if (sel_ > 0) sel_--;
              return Nav::RedrawFast;
            case Key::Right:
            case Key::Down:
              if (sel_ < E.optionCount - 1) sel_++;
              return Nav::RedrawFast;
            case Key::Ok: {
              answered_ = true;
              chosen_ = sel_;
              const bool correct = (sel_ == E.correct);
              if (correct) correctCount_++;
              answerCount_++;
              LOGI("EX", "answer ex=%u type=%u chosen=%u correct=%u -> %s",
                   (unsigned)exIdx_, (unsigned)E.type, (unsigned)chosen_, (unsigned)E.correct,
                   correct ? "RIGHT" : "WRONG");
              recordResult(E, correct);
              return Nav::RedrawFull;
            }
            default:
              return Nav::Stay;
          }
        }
        if (k == Key::Ok || k == Key::Back || k == Key::Right || k == Key::Down) {
          advanceToNextExercise();
          return Nav::RedrawFull;
        }
        return Nav::Stay;
      }

      default:
        return Nav::Stay;
    }
  }

 private:
  enum class Phase : uint8_t { Load, Error, Theory, Reading, Exercise, Summary };

  const Exercise& cur() const { return lesson_.exercises[exIdx_]; }

  Phase firstContentPhase() {
    if (lesson_.exerciseCount > 0 && lesson_.exercises[0].type == ExType::Reading) {
      return Phase::Reading;
    }
    return Phase::Exercise;
  }

  // Sets phase state for the current exercise index (or the start phase).
  void enterExercise(Phase p) {
    phase_ = p;
    sel_ = 0;
    answered_ = false;
    chosen_ = 0;
    explainShown_ = false;
    pageIdx_ = 0;
    if (phase_ == Phase::Exercise && exIdx_ < lesson_.exerciseCount &&
        cur().type == ExType::Reading) {
      phase_ = Phase::Reading;
    }
  }

  void advanceToNextExercise() {
    exIdx_++;
    if (exIdx_ >= lesson_.exerciseCount) {
      phase_ = Phase::Summary;
      return;
    }
    enterExercise(Phase::Exercise);
  }

  void recordResult(const Exercise& E, bool correct) {
    if (!progress_) return;
    const uint8_t q = correct ? 5 : 1;
    for (uint8_t i = 0; i < E.srsCount && E.srsIds[i]; i++) {
      SrsItem* it = progress_->findOrCreate(E.srsIds[i]);
      if (it) SrsScheduler::grade(*it, q, progress_->time.day);
    }
    Skill sk = Skill::Vocabulary;
    if (E.type == ExType::Cloze || E.type == ExType::Mistake) sk = Skill::Grammar;
    if (E.type == ExType::Reading) sk = Skill::Reading;
    for (uint8_t i = 0; i < E.tagCount; i++) {
      if (E.tags[i] && strcmp(E.tags[i], "reading") == 0) {
        sk = Skill::Reading;
        break;
      }
    }
    progress_->mastery.record(sk, E.tags, E.tagCount, correct);
  }

  void renderExercise(Canvas& c, const Exercise& E) {
    const int M = cfg::MARGIN;
    const LgFont* ui = fontByRole(FontRole::UI);
    const LgFont* body = fontByRole(FontRole::Body);
    const LgFont* bold = fontByRole(FontRole::BodyBold);

    c.drawText(M, M + 2, ui, instructionFor(E));
    int y = M + ui->advanceY + 16;

    // Prompt: cloze keeps the raw sentence with "___"; others wrap bold.
    {
      const char* lines[3];
      int lens[3];
      int n = c.wrapText(bold, E.prompt, c.width() - 2 * M, lines, lens, 3);
      for (int i = 0; i < n; i++) {
        c.drawTextN(M, y, bold, lines[i], lens[i]);
        y += bold->advanceY + 4;
      }
    }
    if (E.promptSmall) {
      c.drawText(M, y, body, E.promptSmall);
      y += body->advanceY + 4;
    }
    y += 14;

    const int rowH = 46;
    for (uint8_t i = 0; i < E.optionCount; i++) {
      if (y + rowH > c.height() - 46) break;
      const bool selected = (!answered_) && (i == sel_);
      if (selected) {
        c.rect(M - 12, y - 6, c.width() - 2 * M + 24, rowH - 8, true);
        c.drawTextInv(M, y, body, E.options[i]);
      } else {
        c.drawText(M, y, body, E.options[i]);
      }
      if (answered_) {
        if (i == E.correct) {
          c.drawText(c.width() - M - 34, y, bold, "\xE2\x9C\x93"); // ✓
        } else if (i == chosen_) {
          c.drawText(c.width() - M - 34, y, bold, "\xE2\x9C\x97"); // ✗
        }
      }
      y += rowH;
    }

    // Long-press OK: explanation overlay box above the footer.
    if (explainShown_ && E.explain) {
      const int boxH = 110;
      int by = c.height() - 44 - boxH;
      c.rect(M - 10, by - 8, c.width() - 2 * M + 20, boxH, true);
      const char* lines[4];
      int lens[4];
      int n = c.wrapText(body, E.explain, c.width() - 2 * M - 20, lines, lens, 4);
      int ty = by + 6;
      for (int i = 0; i < n; i++) {
        c.drawTextInv(M + 4, ty, body, lines[i], lens[i]);
        ty += body->advanceY + 2;
      }
    }

    if (answered_) {
      const bool right = (chosen_ == E.correct);
      drawFooterHint(c, right ? "CORRECT \xE2\x80\x94 OK to continue"
                              : "WRONG \xE2\x80\x94 OK to continue");
    } else {
      drawFooterHint(c,
          "\xE2\x86\x90\xE2\x86\x92 select \xC2\xB7 OK answer \xC2\xB7 hold OK info");
    }
  }

  const char* instructionFor(const Exercise& E) const {
    switch (E.type) {
      case ExType::Cloze: return "FILL THE GAP";
      case ExType::Mistake: return "FIND THE MISTAKE";
      case ExType::Reading: return "READING";
      default:
        for (uint8_t i = 0; i < E.tagCount; i++) {
          if (E.tags[i] && strcmp(E.tags[i], "ru2en") == 0) return "TRANSLATE";
          if (E.tags[i] && strcmp(E.tags[i], "reading") == 0) return "QUESTION";
        }
        return "CHOOSE THE CORRECT MEANING";
    }
  }

  void drawFooterHint(Canvas& c, const char* s) {
    const LgFont* ui = fontByRole(FontRole::UI);
    c.hline(cfg::MARGIN, c.width() - cfg::MARGIN, c.height() - 34, true);
    c.drawText(cfg::MARGIN, c.height() - 28, ui, s);
  }

  void drawPageFoot(Canvas& c, const char* left, const char* right) {
    const LgFont* ui = fontByRole(FontRole::UI);
    c.hline(cfg::MARGIN, c.width() - cfg::MARGIN, c.height() - 34, true);
    c.drawText(cfg::MARGIN, c.height() - 28, ui, left);
    int w = c.textWidth(ui, right);
    c.drawText(c.width() - cfg::MARGIN - w, c.height() - 28, ui, right);
  }

  Lesson lesson_; // ~26KB — LessonScreen is statically allocated inside App
  LessonMeta meta_;
  ProgressStore* progress_ = nullptr;

  Phase phase_ = Phase::Load;
  char errText_[96] = "";

  uint8_t theoryIdx_ = 0;
  uint8_t theoryLine_ = 0;
  uint16_t exIdx_ = 0;
  uint8_t sel_ = 0;
  bool answered_ = false;
  uint8_t chosen_ = 0;
  bool explainShown_ = false;
  uint8_t pageIdx_ = 0;
  uint16_t correctCount_ = 0;
  uint16_t answerCount_ = 0;
};
