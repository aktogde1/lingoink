#pragma once
// LessonScreen runs one lesson end to end:
//   theory pages → exercises (choice/cloze/mistake, reading + QA) → summary.
//
// v0.2 interaction contract:
//   - Options are shuffled per exercise (stable while the question is up);
//     grading always uses the original answer key (`Exercise::correct`).
//   - Options live in a sliding window: every option can be selected, long
//     ones wrap to two lines, nothing renders off-screen.
//   - Back never means Next: Theory/Reading page back, Exercise offers
//     "EXIT LESSON?" (OK exit / BACK cancel). After answering only OK moves
//     on. Aborting keeps lastLessonId untouched (App checks aborted()).
//   - Feedback shows status + correct answer + short explanation in a
//     rounded card; long OK opens the full explanation reader (paged, never
//     silently cut).
//
// Rendering goes through ui/Chrome.h: the instruction line sits in the
// standard header slot (with an exercise counter on the right), options get
// an A/B/C letter column, marks stay in a reserved right column, the exit
// and reset dialogs share the one modal style, and the summary is a large
// Display-font percentage.
//
// Grading flows into ProgressStore (SRS items + mastery). The App saves
// progress when the screen finishes.

#include "Screen.h"
#include "../ui/Canvas.h"
#include "../ui/Chrome.h"
#include "../ui/FontRegistry.h"
#include "../ui/Strings.h"
#include "../ui/Log.h"
#include "../course/LessonLoader.h"
#include "../progress/ProgressStore.h"

#include <Arduino.h>

class LessonScreen : public Screen {
 public:
  enum class Phase : uint8_t { Load, Error, Theory, Reading, Exercise, Summary, Gate };

  // Loads the lesson from SD. Returns false with a readable error.
  // `canvas` is the shared screen canvas — needed by key handling for
  // layout math (wrap widths, visible-row counts), not only rendering.
  bool start(const LessonMeta& meta, ProgressStore* progress, Canvas* canvas) {
    progress_ = progress;
    canvas_ = canvas;
    meta_ = meta;
    aborted_ = false;
    exitPrompt_ = false;
    explainView_ = false;
    explainPage_ = 0;
    LessonParseResult r = lessonLoadFile(lesson_, meta.file);
    if (!r.ok) {
      strncpy(errText_, r.error, sizeof(errText_) - 1);
      phase_ = Phase::Error;
      return false;
    }
    study_ = StudySession{};
    strncpy(study_.lesson, lesson_.id, sizeof(study_.lesson)-1);
    study_.fingerprint = fingerprint();
    for(uint8_t i=0;i<lesson_.exerciseCount;++i) study_.plan[study_.length++]=i;
    referenceMode_=0; readingOffset_=0; helped_=false; revealed_=false;
    theoryIdx_ = 0;
    theoryLine_ = 0;
    exIdx_ = 0;
    correctCount_ = 0;
    answerCount_ = 0;
    gateMode_=false; gatePassed_=false; restored_=false; chainNeeded_=false;
    gateSel_=0;
    rAnswer_=0; rCorrect_=0;
    // A brand-new, never-viewed lesson offers a short "already know" check
    // before the full lesson (skipped for reviews, resumed bookmarks and
    // lessons without exercises).
    enterExercise(lesson_.theoryCount > 0 ? Phase::Theory : firstContentPhase());
    if(!lesson_.exerciseCount && !lesson_.theoryCount) phase_=Phase::Summary;
    if(progress_ && strcmp(progress_->session.lesson, lesson_.id)==0 &&
        progress_->session.fingerprint==study_.fingerprint) restore(progress_->session);
    if(!restored_ && phase_!=Phase::Summary && progress_ &&
        progress_->stage(lesson_.id)==0 && lesson_.exerciseCount) phase_=Phase::Gate;
    return true;
  }

  // One bounded review batch from a lesson. App finds the first lesson with
  // eligible items. Rotate candidates by day; prefer free recall over recognition.
  bool startReview(const LessonMeta& meta, ProgressStore* progress, Canvas* canvas,
                   const char* weakTag=nullptr) {
    if(!start(meta,progress,canvas)) return false;
    StudySession fresh;
    strncpy(fresh.lesson,lesson_.id,sizeof(fresh.lesson)-1);
    fresh.fingerprint=fingerprint(); fresh.review=true;
    char selected[48][28] = {}; uint8_t selectedCount=0;
    for(int pass=0;pass<2;++pass) for(uint16_t n=0;n<lesson_.exerciseCount;++n) {
      uint8_t i=(n+progress->time.day)%lesson_.exerciseCount;
      const auto& e=lesson_.exercises[i];
      if(e.type==ExType::Reading || !e.reviewable || (pass==0)!=(e.type==ExType::Recall)) continue;
      bool tagMatch=!weakTag;
      for(uint8_t t=0;t<e.tagCount;++t) if(weakTag && strcmp(weakTag,e.tags[t])==0) tagMatch=true;
      if(!tagMatch) continue;
      bool eligible=false;
      for(uint8_t k=0;k<e.srsCount;++k) {
        auto it=progress->find(e.srsIds[k]);
        if(!it || (!weakTag && !SrsScheduler::isDue(*it,progress->time.day))) continue;
        bool seen=false;
        for(uint8_t z=0;z<selectedCount;++z) if(strcmp(selected[z],e.srsIds[k])==0) seen=true;
        if(!seen) eligible=true;
      }
      if(!eligible || fresh.length>=12) continue;
      fresh.plan[fresh.length++]=i;
      for(uint8_t k=0;k<e.srsCount && selectedCount<48;++k)
        strncpy(selected[selectedCount++],e.srsIds[k],27);
    }
    if(!fresh.length) return false;
    study_=fresh; exIdx_=study_.plan[0]; correctCount_=answerCount_=0;
    rAnswer_=0; rCorrect_=0; chainNeeded_=false;
    enterExercise(Phase::Exercise);
    return true;
  }

  // Build a cross-lesson review queue: scan the course lesson by lesson (one
  // Lesson in RAM — the shared buffer), collect up to `cap` exercises whose
  // SRS ids are due and not already queued; free recall is preferred over
  // recognition and the scan start rotates by day. Writes course lesson
  // indices into qLesson and exercise indices into qEx; returns the count.
  uint8_t buildReviewQueue(const CourseCatalog& c, ProgressStore* p,
                           uint8_t* qLesson, uint8_t* qEx, uint8_t cap) {
    if(!p || !c.course.lessonCount || !cap) return 0;
    char selected[48][28]={}; uint8_t selectedCount=0;
    uint8_t n=0;
    const uint8_t total=c.course.lessonCount;
    const uint8_t first=(uint8_t)(p->time.day%total);
    for(int pass=0;pass<2 && n<cap;++pass)
      for(uint8_t off=0;off<total && n<cap;++off) {
        const uint8_t li=(uint8_t)((first+off)%total);
        if(!lessonLoadFile(lesson_,c.course.lessons[li].file).ok) continue;
        for(uint16_t k=0;k<lesson_.exerciseCount && n<cap;++k) {
          const uint8_t i=(uint16_t)((k+p->time.day)%lesson_.exerciseCount);
          const auto& e=lesson_.exercises[i];
          if(e.type==ExType::Reading || !e.reviewable) continue;
          if((pass==0)!=(e.type==ExType::Recall)) continue;
          bool eligible=false;
          for(uint8_t s=0;s<e.srsCount;++s) {
            auto it=p->find(e.srsIds[s]);
            if(!it || !SrsScheduler::isDue(*it,p->time.day)) continue;
            bool seen=false;
            for(uint8_t z=0;z<selectedCount;++z) if(strcmp(selected[z],e.srsIds[s])==0) seen=true;
            if(!seen) eligible=true;
          }
          if(!eligible) continue;
          qLesson[n]=li; qEx[n]=i; ++n;
          for(uint8_t s=0;s<e.srsCount && selectedCount<48;++s)
            strncpy(selected[selectedCount++],e.srsIds[s],27);
        }
      }
    return n;
  }

  // Start one slice of the cross-lesson review queue stored in
  // progress_->session: the next `count` entries (they must belong to
  // `meta`). firstSlice starts fresh totals; otherwise the queue-wide
  // counters carried in the bookmark continue. The queue position advances
  // with checkpoint().
  bool startReviewSlice(const LessonMeta& meta, uint8_t count,
                        ProgressStore* progress, Canvas* canvas, bool firstSlice) {
    if(!start(meta,progress,canvas)) return false;
    if(firstSlice && progress->session.lesson[0]) return false;  // stale bookmark guard
    StudySession fresh;
    strncpy(fresh.lesson,lesson_.id,sizeof(fresh.lesson)-1);
    fresh.fingerprint=fingerprint(); fresh.review=true;
    fresh.qLen=progress->session.qLen;
    fresh.qPos=firstSlice?0:progress->session.qPos;
    fresh.qBase=fresh.qPos;
    fresh.qAnswers=firstSlice?0:progress->session.qAnswers;
    fresh.qCorrect=firstSlice?0:progress->session.qCorrect;
    fresh.qUnresolved=firstSlice?0:progress->session.qUnresolved;
    for(uint8_t i=0;i<fresh.qLen;++i) {
      fresh.qLesson[i]=progress->session.qLesson[i];
      fresh.qEx[i]=progress->session.qEx[i];
    }
    const uint8_t base=fresh.qPos;
    for(uint8_t j=0;j<count && base+j<fresh.qLen;++j) {
      const uint8_t ex=fresh.qEx[base+j];
      if(ex>=lesson_.exerciseCount) return false;  // course changed under us
      fresh.plan[fresh.length++]=ex;
      qEntry_[fresh.length-1]=base+j;
    }
    if(!fresh.length) return false;
    fresh.qPos=(uint8_t)(base+fresh.length);
    study_=fresh;
    rAnswer_=fresh.qAnswers; rCorrect_=fresh.qCorrect;
    exIdx_=study_.plan[0]; correctCount_=answerCount_=0;
    chainNeeded_=false;
    enterExercise(Phase::Exercise);
    return true;
  }

  // When the finished slice has queue entries left, returns the next slice's
  // lesson and writes how many entries belong to it; nullptr otherwise.
  const LessonMeta* takeChain(const CourseCatalog* c, uint8_t* count) {
    if(!chainNeeded_ || !progress_ || !c) return nullptr;
    chainNeeded_=false;
    if(!study_.review || study_.qPos>=study_.qLen) return nullptr;
    const uint8_t li=study_.qLesson[study_.qPos];
    if(li>=c->course.lessonCount) return nullptr;
    uint8_t n=0;
    for(uint8_t i=study_.qPos;i<study_.qLen;++i) {
      if(study_.qLesson[i]!=li) break;
      ++n;
    }
    *count=n;
    return &c->course.lessons[li];
  }
  void reconcileReviewItems(const CourseCatalog& c,ProgressStore& p) {
    bool active[ProgressStore::MAX_SRS]={};
    for(uint8_t l=0;l<c.course.lessonCount;++l) {
      if(!lessonLoadFile(lesson_,c.course.lessons[l].file).ok) return; // never prune on a failed read
      for(uint8_t e=0;e<lesson_.exerciseCount;++e) {
        const auto& x=lesson_.exercises[e];if(!x.reviewable || x.type==ExType::Reading)continue;
        for(uint8_t k=0;k<x.srsCount;++k)for(uint16_t i=0;i<p.itemCount;++i)
          if(strcmp(x.srsIds[k],p.items[i].id)==0)active[i]=true;
      }
    }
    uint16_t count=0;for(uint16_t i=0;i<p.itemCount;++i)if(active[i])p.items[count++]=p.items[i];
    p.itemCount=count;
  }
  bool isReview() const {return study_.review;}
  uint16_t independentAnswers() const {return study_.review ? rAnswer_ : answerCount_;}
  uint8_t unresolvedCount() const {
    uint8_t n=0; uint64_t bits=study_.review ? study_.qUnresolved : study_.unresolved;
    while(bits) {n+=bits&1;bits>>=1;} return n;
  }
  void checkpoint() {
    if(!progress_ || phase_==Phase::Error) return;
    study_.phase=(uint8_t)phase_; study_.theory=theoryIdx_; study_.line=theoryLine_;
    study_.page=pageIdx_;study_.offset=readingOffset_;
    study_.answers=answerCount_;study_.correct=correctCount_;
    study_.answered=answered_;study_.chosen=chosen_;
    study_.helped=helped_;study_.revealed=revealed_;
    study_.gate=gateMode_;
    study_.qAnswers=rAnswer_;study_.qCorrect=rCorrect_;
    progress_->session=study_;
  }

  bool finished() const { return phase_ == Phase::Summary || phase_ == Phase::Error; }
  Phase phase() const { return phase_; }  // preview/diagnostics read-out
  bool wasError() const { return phase_ == Phase::Error; }
  bool aborted() const { return aborted_; }
  const char* lessonId() const { return lesson_.id; }
  uint8_t theoryCount() const { return lesson_.theoryCount; }
  uint16_t exerciseCount() const { return lesson_.exerciseCount; }
  const char* errorText() const { return errText_; }
  uint8_t accuracyPct() const {
    const uint16_t a = study_.review ? rAnswer_ : answerCount_;
    const uint16_t c = study_.review ? rCorrect_ : correctCount_;
    if (a == 0) return 0;
    return (uint8_t)((c * 100 + a / 2) / a);
  }

  void render(Canvas& c) override {
    if(referenceMode_) {renderReference(c);return;}
    switch (phase_) {
      case Phase::Error: renderError(c); break;
      case Phase::Theory: renderTheory(c); break;
      case Phase::Reading: renderReading(c); break;
      case Phase::Exercise: renderExercise(c, cur()); break;
      case Phase::Summary: renderSummary(c); break;
      case Phase::Gate: renderGate(c); break;
      case Phase::Load: break;
    }
    if (explainView_ && phase_ == Phase::Exercise) {
      renderExplain(c);  // full-screen explanation reader
      return;
    }
    if (exitPrompt_) {
      renderExitPrompt(c);  // modal dialog over the current phase
    }
  }

  Nav handleKey(Key k) override {
    if(referenceMode_) return handleReference(k);
    switch (phase_) {
      case Phase::Error:
        return Nav::Done;

      case Phase::Summary:
        if (gateMode_) {
          if (k == Key::Ok) {
            if (gatePassed_) return Nav::Done;   // finishLesson marks mastery
            return startFullLesson();            // weak check: take the lesson
          }
          return Nav::Stay;
        }
        if (k == Key::Ok || k == Key::Back) return Nav::Done;
        return Nav::Stay;

      case Phase::Theory:
        return handleTheoryKey(k);

      case Phase::Reading:
        return handleReadingKey(k);

      case Phase::Exercise:
        return handleExerciseKey(k);

      case Phase::Gate:
        return handleGateKey(k);

      default:
        return Nav::Stay;
    }
  }

 private:
  friend struct StudyTests;
  static constexpr int FB_CAP = 192;      // feedback line buffer
  static constexpr int EXPLAIN_LINES = 36; // max wrapped explain lines

  const Exercise& cur() const { return lesson_.exercises[exIdx_]; }

  Phase firstContentPhase() {
    if(!lesson_.exerciseCount) return Phase::Summary;
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
    optTop_ = 0;
    explainView_ = false;
    explainPage_ = 0;
    pageIdx_ = 0;
    readingOffset_=0; helped_=false;revealed_=false;
    if (phase_ == Phase::Exercise && exIdx_ < lesson_.exerciseCount) {
      if (cur().type == ExType::Reading) {
        phase_ = Phase::Reading;  // no options — nothing to shuffle
      } else {
        shuffleOptions(cur());
      }
    }
  }

  void advanceToNextExercise() {
    ++study_.cursor;
    if(study_.cursor>=study_.length) {
      if(gateMode_) gatePassed_ = answerCount_>0 && accuracyPct()>=80;
      // A chained review continues straight into the next lesson's slice;
      // App picks it up via takeChain() before the next render.
      if(study_.review && study_.qPos<study_.qLen) chainNeeded_=true;
      phase_=Phase::Summary;
      return;
    }
    exIdx_=study_.plan[study_.cursor];
    enterExercise(Phase::Exercise);
  }

  // ------------------------------------------------------------------
  // Option shuffle: display order -> real option index. Grading compares
  // against the original answer key, so JSON semantics are unchanged.
  // ------------------------------------------------------------------
  void shuffleOptions(const Exercise& E) {
    for (uint8_t i = 0; i < cfg::MAX_OPTIONS; i++) order_[i] = i;
    if (E.optionCount < 2) return;  // nothing to shuffle (guard: 0-1 options)
    uint32_t seed = millis() ^ ((uint32_t)exIdx_ * 0x9E3779B9u) ^ 0x85297A4Du;
    for (uint8_t i = E.optionCount - 1; i > 0; i--) {
      seed = seed * 1664525u + 1013904223u;
      const uint8_t j = (uint8_t)((seed >> 16) % (i + 1));
      const uint8_t t = order_[i];
      order_[i] = order_[j];
      order_[j] = t;
    }
  }

  uint8_t rowOfOption(uint8_t realIdx) const {
    for (uint8_t r = 0; r < cfg::MAX_OPTIONS; r++) {
      if (order_[r] == realIdx) return r;
    }
    return 0;
  }

  // ------------------------------------------------------------------
  // Shared layout helpers (used by BOTH render and key handling so the
  // sliding option window matches what is actually drawn). All geometry
  // comes from chrome::metrics — one source of truth with the other
  // screens.
  // ------------------------------------------------------------------
  chrome::Metrics cmt() const { return chrome::metrics(*canvas_); }

  int bodyLineH() const { return fontByRole(FontRole::Body)->advanceY + 4; }
  int uiLineH() const { return fontByRole(FontRole::UI)->advanceY + 3; }

  int promptMaxLines() const { return canvas_->portrait() ? 5 : 3; }

  int promptLines(const Exercise& E) const {
    const char* l[6];
    int lens[6];
    return canvas_->wrapText(fontByRole(FontRole::BodyBold), E.prompt,
                             canvas_->width() - 2 * canvas_->margin(), l, lens,
                             promptMaxLines());
  }

  int smallLines(const Exercise& E) const {
    if (!E.promptSmall) return 0;
    const char* l[2];
    int lens[2];
    return canvas_->wrapText(fontByRole(FontRole::Body), E.promptSmall,
                             canvas_->width() - 2 * canvas_->margin(), l, lens, 2);
  }

  // First option row y: header + prompt + optional small line.
  int exTopY(const Exercise& E) const {
    const chrome::Metrics mt = cmt();
    int y = mt.top;
    y += promptLines(E) * (fontByRole(FontRole::BodyBold)->advanceY + 4);
    y += smallLines(E) * (fontByRole(FontRole::Body)->advanceY + 4);
    return y + 12;
  }

  // Options wrap with a reserved right column for the ✓/✗ marks.
  static constexpr int MARK_COL = 44;  // reserved right column

  int optionWrapW() const {
    const chrome::Metrics mt = cmt();
    return mt.w - 2 * mt.m - MARK_COL;
  }

  int optionRowH(const Exercise& E, uint8_t realIdx) const {
    const LgFont* body = fontByRole(FontRole::Body);
    const char* l[3];
    int lens[3];
    const int n = canvas_->wrapText(body, E.options[realIdx], optionWrapW(), l, lens, 3);
    // The row must contain the whole text block (+ vertical padding) — the
    // selection card hugs this height, so a wrapped line can never fall out
    // of the card and disappear (white on white).
    const int textH = n * (body->advanceY + 4) - 4;
    const int need = textH + 20;
    return need > cmt().rowH ? need : cmt().rowH;
  }

  // Feedback card text budget: wrapped answer lines + explain lines, capped
  // like the renderer. Single source for measurement AND drawing.
  void feedbackLineCounts(const Exercise& E, int& ansN, int& expN) const {
    ansN = expN = 0;
    const bool wrong = answered_ && chosen_ != E.correct;
    const LgFont* ui = fontByRole(FontRole::UI);
    const int W = canvas_->width() - 2 * canvas_->margin() - 32;  // card inner
    const int maxTotal = canvas_->portrait() ? 5 : 3;
    if (wrong) {
      char buf[FB_CAP];
      snprintf(buf, sizeof(buf), S(AnswerFmt), E.options[E.correct]);
      const char* la[2];
      int lensa[2];
      ansN = canvas_->wrapText(ui, buf, W, la, lensa, 2);
    }
    if (E.explain && E.explain[0]) {
      const char* le[2];
      int lense[2];
      expN = canvas_->wrapText(ui, E.explain, W, le, lense, 2);
    }
    int budget = maxTotal;
    if (ansN > budget) ansN = budget;
    budget -= ansN;
    if (expN > budget) expN = budget;
  }

  int feedbackCardH(const Exercise& E) const {
    if (!answered_) return 0;
    int ansN, expN;
    feedbackLineCounts(E, ansN, expN);
    return 32 + fontByRole(FontRole::BodyBold)->advanceY + 6 +
           (ansN + expN) * uiLineH();
  }

  int optionsBottom(const Exercise& E) const {
    const chrome::Metrics mt = cmt();
    return answered_ ? (mt.bottom - feedbackCardH(E) - 10) : mt.bottom;
  }

  // How many option rows are visible starting at optTop_ (at least 1).
  int visibleRows(const Exercise& E) const {
    int y = exTopY(E);
    const int bottom = optionsBottom(E);
    int n = 0;
    for (uint8_t r = optTop_; r < E.optionCount; r++) {
      const int h = optionRowH(E, order_[r]);
      if (y + h > bottom && n > 0) break;
      y += h;
      n++;
    }
    return n > 0 ? n : 1;
  }

  void ensureVisible(const Exercise& E) {
    while (sel_ < optTop_ && optTop_ > 0) optTop_--;
    while (sel_ >= optTop_ + visibleRows(E) && optTop_ < E.optionCount - 1) {
      optTop_++;
    }
  }

  // After answering: show the chosen AND the correct row (the window ends at
  // the farther of the two; only a window smaller than their spread can hide
  // the chosen row, the correct answer stays visible).
  void ensureAnsweredVisible(const Exercise& E) {
    const uint8_t rc = rowOfOption(chosen_);
    const uint8_t rr = rowOfOption(E.correct);
    const uint8_t hi = rc > rr ? rc : rr;
    optTop_ = 0;
    while (hi >= optTop_ + visibleRows(E) &&
           optTop_ + visibleRows(E) < E.optionCount) {
      optTop_++;
    }
  }

  // ------------------------------------------------------------------
  // Grading
  // ------------------------------------------------------------------
  static Skill skillOf(const Exercise& E) {
    if (E.type == ExType::Cloze || E.type == ExType::Mistake) return Skill::Grammar;
    if (E.type == ExType::Reading) return Skill::Reading;
    for (uint8_t i = 0; i < E.tagCount; i++) {
      if (E.tags[i] && strcmp(E.tags[i], "reading") == 0) return Skill::Reading;
    }
    return Skill::Vocabulary;
  }

  void recordResult(const Exercise& E, bool correct) {
    if (!progress_) return;
    if (E.type == ExType::Recall) {
      // Honest three-way self-assessment (chosen_: 0 miss / 1 hint /
      // 2 recalled). Assistance recorded earlier downgrades a recall to a
      // hint: help can never earn a successful SRS grade.
      uint8_t outcome = chosen_ <= 2 ? chosen_ : (uint8_t)0;
      if (outcome == 2 && helped_) outcome = 1;
      for (uint8_t i = 0; E.reviewable && i < E.srsCount && E.srsIds[i]; i++) {
        SrsItem* it = study_.review ? progress_->find(E.srsIds[i]) : progress_->findOrCreate(E.srsIds[i]);
        if (it) SrsScheduler::gradeRecall(*it, outcome, progress_->time.day);
      }
      if(study_.attempts[exIdx_]==1)
        progress_->mastery.record(skillOf(E), E.tags, E.tagCount, outcome == 2);
      return;
    }
    const uint8_t q = correct && !helped_ ? 5 : 1;
    for (uint8_t i = 0; E.reviewable && i < E.srsCount && E.srsIds[i]; i++) {
      SrsItem* it = study_.review ? progress_->find(E.srsIds[i]) : progress_->findOrCreate(E.srsIds[i]);
      if (it) SrsScheduler::grade(*it, q, progress_->time.day, false);
    }
    if(study_.attempts[exIdx_]==1)
      progress_->mastery.record(skillOf(E), E.tags, E.tagCount, correct && !helped_);
  }

  // ------------------------------------------------------------------
  // Key handling
  // ------------------------------------------------------------------
  Nav handleTheoryKey(Key k) {
    if (exitPrompt_) return handleExitPromptKey(k);
    const TheoryBlock& tb = lesson_.theory[theoryIdx_];
    if (k == Key::Ok || k == Key::Down || k == Key::Right) {
      theoryLine_ += theoryPageSize(tb, theoryLine_);
      if (theoryLine_ >= theoryVisualTotal(tb)) {
        theoryIdx_++;
        theoryLine_ = 0;
        if (theoryIdx_ >= lesson_.theoryCount) {
          enterExercise(firstContentPhase());
        }
      }
      return Nav::RedrawFull;
    }
    if (k == Key::Back || k == Key::Up || k == Key::Left) {
      theoryPageBack();
      return Nav::RedrawFull;
    }
    return Nav::Stay;
  }

  // Lines of `tb` that fit on one screenful starting at `start`. Theory
  // source lines wrap to at most 2 visual lines; paging, rendering and
  // Back-navigation all go through theoryVisual() so they cannot drift.
  static constexpr int THEORY_VIS_MAX = 240;

  // Wraps the whole block into visual lines (nullptr + 0 = blank gap row).
  int theoryVisual(const TheoryBlock& tb, const char* vis[], int vlens[]) const {
    const LgFont* body = fontByRole(FontRole::Body);
    const int W = canvas_->width() - 2 * canvas_->margin();
    int n = 0;
    for (uint8_t i = 0; i < tb.lineCount && n < THEORY_VIS_MAX; i++) {
      if (tb.lines[i][0] == 0) {
        vis[n] = nullptr;
        vlens[n] = 0;
        n++;
        continue;
      }
      const char* l[24];
      int lens[24];
      const int k = canvas_->wrapText(body, tb.lines[i], W, l, lens, 24);
      for (int j = 0; j < k && n < THEORY_VIS_MAX; j++) {
        vis[n] = l[j];
        vlens[n] = lens[j];
        n++;
      }
    }
    return n;
  }

  // Total visual lines of a block (small helper over theoryVisual).
  int theoryVisualTotal(const TheoryBlock& tb) const {
    const char* vis[THEORY_VIS_MAX];
    int vlens[THEORY_VIS_MAX];
    return theoryVisual(tb, vis, vlens);
  }

  uint8_t theoryPageSize(const TheoryBlock& tb, uint8_t start) const {
    const chrome::Metrics mt = cmt();
    const LgFont* body = fontByRole(FontRole::Body);
    const char* vis[THEORY_VIS_MAX];
    int vlens[THEORY_VIS_MAX];
    const int total = theoryVisual(tb, vis, vlens);
    int y = mt.top;
    uint8_t shown = 0;
    for (int i = start; i < total; i++) {
      if (y + body->advanceY > mt.bottom) break;
      y += bodyLineH();
      shown++;
    }
    return shown > 0 ? shown : 1;
  }

  void theoryPageBack() {
    const TheoryBlock& tb = lesson_.theory[theoryIdx_];
    if (theoryLine_ > 0) {
      // Walk page starts from the block head and step to the previous one.
      uint8_t starts[THEORY_VIS_MAX];
      uint8_t n = 0;
      uint8_t s = 0;
      while (n < THEORY_VIS_MAX) {
        starts[n++] = s;
        const uint8_t step = theoryPageSize(tb, s);
        if (s + step >= theoryVisualTotal(tb)) break;
        s += step;
      }
      uint8_t target = 0;
      for (uint8_t i = 0; i < n; i++) {
        if (starts[i] < theoryLine_) target = starts[i];
        if (starts[i] >= theoryLine_) break;
      }
      theoryLine_ = target;
      return;
    }
    if (theoryIdx_ > 0) {
      theoryIdx_--;
      const TheoryBlock& pb = lesson_.theory[theoryIdx_];
      uint8_t s = 0, last = 0;
      while (s < theoryVisualTotal(pb)) {
        last = s;
        const uint8_t step = theoryPageSize(pb, s);
        if (step == 0) break;
        s += step;
      }
      theoryLine_ = last;
      return;
    }
    exitPrompt_ = true;  // first page of the lesson
  }

  Nav handleReadingKey(Key k) {
    if(exitPrompt_) return handleExitPromptKey(k);
    if(k==Key::OkLong && lesson_.theoryCount) {openRules();return Nav::RedrawFull;}
    if(k==Key::Ok || k==Key::Down || k==Key::Right) {
      if(!textNext(cur(),pageIdx_,readingOffset_)) advanceToNextExercise();
      return Nav::RedrawFull;
    }
    if(k==Key::Back || k==Key::Up || k==Key::Left) {
      if(!textBack(cur(),pageIdx_,readingOffset_)) exitPrompt_=true;
      return Nav::RedrawFull;
    }
    return Nav::Stay;
  }

  Nav handleExerciseKey(Key k) {
    if (exitPrompt_) return handleExitPromptKey(k);
    if (explainView_) return handleExplainKey(k);
    const Exercise& E = cur();
    if (k == Key::OkLong) {
      if(!answered_) helped_=true;
      if (E.explain && E.explain[0]) {
        explainView_ = true;
        explainPage_ = 0;
        return Nav::RedrawFull;
      }
      return Nav::Stay;
    }
    if(E.type==ExType::Recall && !answered_ && !revealed_) {
      if(k==Key::Ok) {revealed_=true;sel_=0;return Nav::RedrawFull;}
      if(k==Key::Back) {referenceMode_=1;referenceSel_=0;return Nav::RedrawFull;}
      return Nav::Stay;
    }
    if (!answered_) {
      // Recall after reveal: three self-assessment rows (miss / hint /
      // recalled). An accidental press lands on the conservative default —
      // two deliberate presses are always needed, and a stray confirm marks
      // the item for retry instead of faking a success.
      const uint8_t rows = E.type==ExType::Recall ? 3 : E.optionCount;
      switch (k) {
        case Key::Left:
        case Key::Up:
          sel_ = (sel_ + rows - 1) % rows;  // cyclic
          ensureVisible(E);
          return Nav::RedrawFast;
        case Key::Right:
        case Key::Down:
          sel_ = (sel_ + 1) % rows;  // cyclic
          ensureVisible(E);
          return Nav::RedrawFast;
        case Key::Ok: {
          answered_ = true;
          chosen_ = E.type==ExType::Recall ? sel_ : order_[sel_];  // display row -> real option index
          const bool correct = E.type==ExType::Recall ? (chosen_==2) : (chosen_ == E.correct);
          recordAttempt(E.type==ExType::Recall ? (chosen_==2 && !helped_) : (correct && !helped_));
          LOGI("EX", "answer ex=%u type=%u chosen=%u correct=%u -> %s",
               (unsigned)exIdx_, (unsigned)E.type, (unsigned)chosen_, (unsigned)E.correct,
               correct ? "RIGHT" : "WRONG");
          recordResult(E, correct);
          ensureAnsweredVisible(E);
          return Nav::RedrawFull;
        }
        case Key::Back:
          referenceMode_=1;referenceSel_=0;
          return Nav::RedrawFull;
        default:
          return Nav::Stay;
      }
    }
    // Feedback phase: ONLY OK advances — no accidental skips.
    if (k == Key::Ok) {
      advanceToNextExercise();
      return Nav::RedrawFull;
    }
    return Nav::Stay;
  }

  Nav handleExitPromptKey(Key k) {
    if (k == Key::Ok) {
      aborted_ = true;
      return Nav::Done;  // App skips the resume pointer for aborted lessons
    }
    if (k == Key::Back) {
      exitPrompt_ = false;
      return Nav::RedrawFull;
    }
    return Nav::Stay;
  }

  Nav handleExplainKey(Key k) {
    if (k == Key::Back) {
      explainView_ = false;
      return Nav::RedrawFull;
    }
    if (k == Key::Down || k == Key::Right || k == Key::Ok) {
      if (explainPage_ + 1 < explainPageCount()) {
        explainPage_++;
        return Nav::RedrawFull;
      }
      explainView_ = false;  // OK on the last page closes
      return Nav::RedrawFull;
    }
    if ((k == Key::Up || k == Key::Left) && explainPage_ > 0) {
      explainPage_--;
      return Nav::RedrawFull;
    }
    return Nav::Stay;
  }

  // ------------------------------------------------------------------
  // Rendering
  // ------------------------------------------------------------------
  // ------------------------------------------------------------------
  // "Already know" gate: a brand-new lesson offers a short self-check
  // (recall-first, up to 5 key exercises) before the full lesson. The probe
  // is graded like a normal lesson, so SRS items are created either way;
  // a passed check finishes the lesson as mastered (>=80% independent).
  // ------------------------------------------------------------------
  void renderGate(Canvas& c) {
    const chrome::Metrics mt = chrome::metrics(c);
    chrome::header(c, mt, U("ALREADY KNOW?","УЖЕ ЗНАЕТЕ?"));
    int y = mt.top;
    y = paragraph(c, U("Take the full lesson, or check yourself with a few key tasks first. If the check goes well, the lesson is marked as mastered.",
      "Пройдите урок целиком или сначала проверьте себя на нескольких ключевых заданиях. Если проверка пройдена, урок отмечается освоенным."), y, fontByRole(FontRole::UI), 6) + 12;
    chrome::menuRow(c, mt, y, U("TAKE THE LESSON","ПРОЙТИ УРОК"),
                    U("Theory and all exercises","Теория и все задания"), gateSel_==0);
    y += chrome::menuRowHeight(1) + 10;
    chrome::menuRow(c, mt, y, U("ALREADY KNOW: CHECK","УЖЕ ЗНАЮ: ПРОВЕРИТЬ"),
                    U("A few key tasks, no theory","Несколько ключевых заданий без теории"), gateSel_==1);
    c.drawText(mt.m, mt.widgetY, fontByRole(FontRole::UI), U("Back: exit","Назад: выход"));
  }

  Nav handleGateKey(Key k) {
    if (exitPrompt_) return handleExitPromptKey(k);
    if (k == Key::Up || k == Key::Left || k == Key::Down || k == Key::Right) {
      gateSel_=(uint8_t)(gateSel_+1)%2;  // two rows — one key flips the choice
      return Nav::RedrawFast;
    }
    if (k == Key::Ok) {
      if (gateSel_==0) return startFullLesson();
      startGateCheck();
      return Nav::RedrawFull;
    }
    if (k == Key::Back) {exitPrompt_=true;return Nav::RedrawFull;}
    return Nav::Stay;
  }

  Nav startFullLesson() {
    study_=StudySession{};
    strncpy(study_.lesson,lesson_.id,sizeof(study_.lesson)-1);
    study_.fingerprint=fingerprint();
    for(uint8_t i=0;i<lesson_.exerciseCount;++i) study_.plan[study_.length++]=i;
    gateMode_=false;gatePassed_=false;helped_=false;revealed_=false;
    theoryIdx_=0;theoryLine_=0;exIdx_=0;correctCount_=0;answerCount_=0;
    rAnswer_=0;rCorrect_=0;referenceMode_=0;readingOffset_=0;
    enterExercise(Phase::Theory);
    return Nav::RedrawFull;
  }

  void startGateCheck() {
    StudySession fresh;
    strncpy(fresh.lesson,lesson_.id,sizeof(fresh.lesson)-1);
    fresh.fingerprint=fingerprint();
    for(int pass=0;pass<2 && fresh.length<5;++pass)
      for(uint16_t n=0;n<lesson_.exerciseCount && fresh.length<5;++n) {
        const uint8_t i=(uint8_t)((n+progress_->time.day)%lesson_.exerciseCount);
        const auto& e=lesson_.exercises[i];
        if(e.type==ExType::Reading) continue;
        if((pass==0)!=(e.type==ExType::Recall)) continue;
        bool dup=false;
        for(uint8_t j=0;j<fresh.length;++j) if(fresh.plan[j]==i) dup=true;
        if(!dup) fresh.plan[fresh.length++]=i;
      }
    if(!fresh.length) {startFullLesson();return;}  // nothing to probe
    study_=fresh;
    gateMode_=true;gatePassed_=false;
    theoryIdx_=0;theoryLine_=0;exIdx_=study_.plan[0];
    correctCount_=0;answerCount_=0;rAnswer_=0;rCorrect_=0;
    referenceMode_=0;readingOffset_=0;helped_=false;revealed_=false;
    enterExercise(Phase::Exercise);
  }

  void renderError(Canvas& c) {
    const chrome::Metrics mt = chrome::metrics(c);
    const LgFont* body = fontByRole(FontRole::Body);
    chrome::header(c, mt, S(ErrorTitle));
    const char* l[4];
    int lens[4];
    const int n = c.wrapText(body, errText_, c.width() - 2 * mt.m, l, lens, 4);
    int y = mt.top + 4;
    for (int i = 0; i < n; i++) {
      c.drawTextN(mt.m, y, body, l[i], lens[i]);
      y += bodyLineH();
    }
  }

  void renderTheory(Canvas& c) {
    const chrome::Metrics mt = chrome::metrics(c);
    const LgFont* body = fontByRole(FontRole::Body);
    const TheoryBlock& tb = lesson_.theory[theoryIdx_];
    char p[24];
    snprintf(p, sizeof(p), S(RuleFmt), (unsigned)(theoryIdx_ + 1),
             (unsigned)lesson_.theoryCount);
    chrome::header(c, mt, tb.title ? tb.title : "", p);
    const char* vis[THEORY_VIS_MAX];
    int vlens[THEORY_VIS_MAX];
    const int total = theoryVisual(tb, vis, vlens);
    int y = mt.top;
    for (int i = theoryLine_; i < total; i++) {
      if (y + body->advanceY > mt.bottom) break;
      if (vis[i]) c.drawTextN(mt.m, y, body, vis[i], vlens[i]);
      y += bodyLineH();
    }
  }

  void renderReading(Canvas& c) {
    renderText(c,cur(),pageIdx_,readingOffset_);
  }

  void renderExercise(Canvas& c, const Exercise& E) {
    if(E.type==ExType::Recall) {renderRecall(c,E);return;}
    const chrome::Metrics mt = chrome::metrics(c);
    const LgFont* ui = fontByRole(FontRole::UI);
    const LgFont* body = fontByRole(FontRole::Body);
    const LgFont* bold = fontByRole(FontRole::BodyBold);

    // The instruction sits in the standard header slot (UI font), with the
    // exercise counter on the right — same geometry as every other screen.
    // A chained review counts across the whole queue.
    char ex[24];
    snprintf(ex, sizeof(ex), S(ExOfFmt),
             (unsigned)((study_.review ? rAnswer_ : answerCount_) + 1),
             (unsigned)(study_.review ? study_.qLen : study_.length));
    c.drawText(mt.m, mt.headerTextY, ui, S(instructionFor(E)));
    chrome::drawRight(c, mt.w - mt.m, mt.headerTextY, ui, ex);
    c.hline(mt.m, mt.w - mt.m, mt.ruleY);

    // Prompt: cloze keeps the raw sentence with "___"; others wrap bold.
    int y = mt.top;
    {
      const char* lines[6];
      int lens[6];
      int n = c.wrapText(bold, E.prompt, c.width() - 2 * mt.m, lines, lens, promptMaxLines());
      for (int i = 0; i < n; i++) {
        c.drawTextN(mt.m, y, bold, lines[i], lens[i]);
        y += bold->advanceY + 4;
      }
    }
    if (E.promptSmall) {
      const char* sl[2];
      int slens[2];
      int sn = c.wrapText(body, E.promptSmall, c.width() - 2 * mt.m, sl, slens, 2);
      for (int i = 0; i < sn; i++) {
        c.drawTextN(mt.m, y, body, sl[i], slens[i]);
        y += body->advanceY + 4;
      }
    }
    y += 12;

    // Option window (sliding): draw fully-fitting rows from optTop_.
    const int bottom = optionsBottom(E);
    const int optW = optionWrapW();
    const int textX = mt.m;
    int drawn = 0;
    uint8_t row = optTop_;
    for (; row < E.optionCount; row++) {
      const uint8_t oi = order_[row];
      // Wrap first: the row height and the selection card derive from the
      // real text block, so a wrapped line can never fall out of the card.
      const char* l[3];
      int lens[3];
      const int n = c.wrapText(body, E.options[oi], optW, l, lens, 3);
      const int linePitch = body->advanceY + 4;
      const int textH = n * linePitch - 4;
      const int h = (textH + 20 > mt.rowH) ? textH + 20 : mt.rowH;
      if (y + h > bottom && drawn > 0) break;
      const bool selected = (!answered_) && (row == sel_);
      const bool chosenRow = answered_ && (oi == chosen_);
      const bool correctRow = answered_ && (oi == E.correct);
      const bool inv = selected || chosenRow;
      if (inv) chrome::selection(c, mt, y + (h - textH) / 2, textH);

      // Option text: up to 3 wrapped lines — never silently cut; vertically
      // centered in the row.
      int ty = y + (h - textH) / 2;
      const int firstLineY = ty;
      for (int i = 0; i < n; i++) {
        if (inv) c.drawTextInv(textX, ty, body, l[i], lens[i]);
        else c.drawTextN(textX, ty, body, l[i], lens[i]);
        ty += linePitch;
      }

      // Feedback marks sit in the reserved right column of the first line.
      if (answered_) {
        const int mx = c.width() - mt.m - 34;
        if (correctRow) {
          if (chosenRow) c.drawTextInv(mx, firstLineY, bold, chrome::kCheck);
          else c.drawText(mx, firstLineY, bold, chrome::kCheck);
        } else if (chosenRow) {
          c.drawTextInv(mx, firstLineY, bold, chrome::kCross);
        }
      }
      y += h;
      drawn++;
    }

    const int hiddenBelow = (int)E.optionCount - (optTop_ + drawn);
    if (answered_) {
      renderFeedback(c, E, bottom + 10);
    }
    // Sliding-window hints live in the bottom-right corner so they can
    // never overlap an option row.
    chrome::scrollHints(c, mt, hiddenBelow, optTop_ > 0);
    c.drawText(mt.m,mt.widgetY,ui,answered_ ? U("Hold OK: explanation","Удерж. OK: объяснение") : U("Back: text / rules / exit","Назад: текст / правила / выход"));
  }

  void renderFeedback(Canvas& c, const Exercise& E, int yTop) {
    const chrome::Metrics mt = chrome::metrics(c);
    const LgFont* bold = fontByRole(FontRole::BodyBold);
    const LgFont* ui = fontByRole(FontRole::UI);
    const bool wrong = chosen_ != E.correct;

    const int cardH = feedbackCardH(E);
    const int cardW = mt.w - 2 * mt.m;
    c.roundRect(mt.m, yTop, cardW, cardH, 12, false);  // white card
    c.roundFrame(mt.m, yTop, cardW, cardH, 12, true, 2);

    int y = yTop + 16;
    c.drawText(mt.m + 16, y, bold, wrong ? S(Wrong) : (helped_ ? U("WITH HELP","С ПОДСКАЗКОЙ") : S(Correct)));
    y += bold->advanceY + 6;

    // Line budgets come from the same helper that sizes the card, so the
    // drawn text always fits by construction.
    int ansN, expN;
    feedbackLineCounts(E, ansN, expN);
    const int W = cardW - 32;
    if (wrong && ansN > 0) {
      char buf[FB_CAP];
      snprintf(buf, sizeof(buf), S(AnswerFmt), E.options[E.correct]);
      const char* l[2];
      int lens[2];
      const int n = c.wrapText(ui, buf, W, l, lens, 2);
      for (int i = 0; i < n && i < ansN; i++) {
        c.drawTextN(mt.m + 16, y, ui, l[i], lens[i]);
        y += uiLineH();
      }
    }
    if (E.explain && E.explain[0] && expN > 0) {
      const char* l[2];
      int lens[2];
      const int n = c.wrapText(ui, E.explain, W, l, lens, 2);
      for (int i = 0; i < n && i < expN; i++) {
        c.drawTextN(mt.m + 16, y, ui, l[i], lens[i]);
        y += uiLineH();
      }
    }
  }

  // ------------------------------------------------------------------
  // Full explanation reader (long OK): paged, nothing silently cut.
  // ------------------------------------------------------------------
  int explainLinesPerPage() const {
    const chrome::Metrics mt = cmt();
    const int n = (mt.bottom - mt.top) / bodyLineH();
    return n > 1 ? n : 1;
  }

  uint8_t explainPageCount() const {
    const Exercise& E = cur();
    if (!E.explain) return 1;
    const char* l[EXPLAIN_LINES];
    int lens[EXPLAIN_LINES];
    const int n = canvas_->wrapText(fontByRole(FontRole::Body), E.explain,
                                    canvas_->width() - 2 * canvas_->margin(), l, lens, EXPLAIN_LINES);
    const int per = explainLinesPerPage();
    uint8_t pages = (uint8_t)((n + per - 1) / per);
    return pages > 0 ? pages : 1;
  }

  void renderExplain(Canvas& c) {
    const chrome::Metrics mt = chrome::metrics(c);
    const LgFont* body = fontByRole(FontRole::Body);
    const Exercise& E = cur();

    c.fillWhite();
    char p[32];
    snprintf(p, sizeof(p), S(PageFmt), (unsigned)(explainPage_ + 1),
             (unsigned)explainPageCount());
    chrome::header(c, mt, S(Explain), p);
    if (E.explain && E.explain[0]) {
      const char* l[EXPLAIN_LINES];
      int lens[EXPLAIN_LINES];
      int n = c.wrapText(body, E.explain, c.width() - 2 * mt.m, l, lens, EXPLAIN_LINES);
      const int per = explainLinesPerPage();
      int first = explainPage_ * per;
      int y = mt.top;
      for (int i = first; i < n && i < first + per; i++) {
        c.drawTextN(mt.m, y, body, l[i], lens[i]);
        y += bodyLineH();
      }
    }
  }

  void renderExitPrompt(Canvas& c) {
    const chrome::Metrics mt = chrome::metrics(c);
    chrome::modal(c, mt, S(ExitTitle), S(ExitNote1), S(ExitNote2), S(ExitDo),
                  S(Cancel));
  }

  void renderSummary(Canvas& c) {
    const chrome::Metrics mt = chrome::metrics(c);
    const LgFont* disp = fontByRole(FontRole::Display);
    const LgFont* title = fontByRole(FontRole::Title);
    const LgFont* body = fontByRole(FontRole::Body);

    // Poster finish: large Display-font percentage, centered column.
    char pct[8];
    snprintf(pct, sizeof(pct), "%u%%", (unsigned)accuracyPct());
    int w = c.textWidth(disp, pct);
    int y = (mt.h - disp->advanceY) / 2 - 30;
    c.drawText((mt.w - w) / 2, y, disp, pct);
    y += disp->advanceY + 4;
    w = c.textWidth(title, S(SumLabel));
    c.drawText((mt.w - w) / 2, y, title, S(SumLabel));
    y += title->advanceY + 14;
    char line[64];
    snprintf(line, sizeof(line), S(SumCountFmt), (unsigned)(study_.review ? rCorrect_ : correctCount_),
             (unsigned)(study_.review ? rAnswer_ : answerCount_));
    w = c.textWidth(body, line);
    c.drawText((mt.w - w) / 2, y, body, line);
    y+=body->advanceY+16;
    if (gateMode_) {
      paragraph(c, gatePassed_ ? U("Check passed — the lesson is marked as mastered. OK: continue.",
                                  "Проверка пройдена — урок отмечен освоенным. OK: дальше.")
                               : U("Check not passed. OK: take the full lesson.",
                                  "Проверка не пройдена. OK: пройти урок целиком."), y, body, 4);
      return;
    }
    snprintf(line,sizeof(line),U("Still to practise: %u","Ещё потренировать: %u"),unresolvedCount());
    c.drawText(mt.m,y,fontByRole(FontRole::UI),line);
    y+=body->advanceY;
    paragraph(c,U("First attempts without help. Review on another day to confirm retention.",
      "Первые ответы без помощи. Чтобы проверить память, повторите в другой день."),y,fontByRole(FontRole::UI),6);
  }

  StrId instructionFor(const Exercise& E) const {
    switch (E.type) {
      case ExType::Cloze: return InsCloze;
      case ExType::Mistake: return InsMistake;
      case ExType::Reading: return InsReading;
      default:
        for (uint8_t i = 0; i < E.tagCount; i++) {
          if (E.tags[i] && strcmp(E.tags[i], "ru2en") == 0) return InsTranslate;
          if (E.tags[i] && strcmp(E.tags[i], "reading") == 0) return InsQuestion;
        }
        return InsChoose;
    }
  }

  uint32_t fingerprint() const {
    uint32_t h=2166136261u;
    for(uint16_t i=0;i<lesson_.poolUsed;++i) h=(h^(uint8_t)lesson_.pool[i])*16777619u;
    for(uint16_t i=0;i<lesson_.exerciseCount;++i) {
      h=(h^lesson_.exercises[i].correct)*16777619u;
      h=(h^(uint8_t)lesson_.exercises[i].type)*16777619u;
    }
    return h;
  }
  void restore(const StudySession& s) {
    if(!s.length || s.cursor>s.length || (s.cursor==s.length && s.phase!=5) || s.phase<2 || s.phase>5) return;
    for(uint8_t i=0;i<s.length;++i) if(s.plan[i]>=lesson_.exerciseCount) return;
    restored_=true;
    if(s.phase==(uint8_t)Phase::Theory && s.theory>=lesson_.theoryCount) return;
    study_=s; exIdx_=s.plan[s.cursor<s.length?s.cursor:s.length-1]; enterExercise((Phase)s.phase);
    theoryIdx_=s.theory;theoryLine_=s.line;pageIdx_=s.page;readingOffset_=s.offset;
    if(phase_==Phase::Reading && (pageIdx_>=cur().pageCount || readingOffset_>strlen(cur().pages[pageIdx_]))) {
      pageIdx_=0;readingOffset_=0;
    }
    correctCount_=s.correct;answerCount_=s.answers;
    answered_=s.answered;chosen_=s.chosen;helped_=s.helped;revealed_=s.revealed;
    rAnswer_=s.qAnswers;rCorrect_=s.qCorrect;
    gateMode_=s.gate;
    gatePassed_ = s.gate && s.phase==(uint8_t)Phase::Summary && s.answers>0 &&
                  (uint8_t)((s.correct*100+s.answers/2)/s.answers)>=80;
    if(s.review) {
      // Rebuild the plan-slot -> queue-entry map: retries duplicate an
      // exercise index and share their first occurrence's entry.
      uint8_t next=s.qBase;
      for(uint8_t i=0;i<s.length;++i) {
        qEntry_[i]=next;
        for(uint8_t j=0;j<i;++j) if(s.plan[j]==s.plan[i]) {qEntry_[i]=qEntry_[j];break;}
        if(qEntry_[i]==next) next++;
      }
    }
    if(phase_==Phase::Exercise) {
      // Recall self-assessment results are 0..2; quizzes are option indices.
      const bool rc = cur().type==ExType::Recall;
      if(rc ? chosen_>2 : chosen_>=cur().optionCount) {answered_=false;chosen_=0;}
    }
    if(phase_==Phase::Exercise && answered_) ensureAnsweredVisible(cur());
    // A summary restored with queue entries left still chains on OK.
    chainNeeded_ = s.review && (s.qPos<s.qLen) && s.phase==(uint8_t)Phase::Summary;
  }
  void recordAttempt(bool independent) {
    // Review batches key first-attempt bits by queue entry (they survive the
    // lesson switch); plain lessons key them by exercise index.
    const uint64_t bit = study_.review ? (1ULL<<qEntry_[study_.cursor]) : (1ULL<<exIdx_);
    if(!(study_.first&bit)) {
      study_.first|=bit;++answerCount_;if(independent)++correctCount_;
      if(study_.review){++rAnswer_;if(independent)++rCorrect_;}
    }
    ++study_.attempts[exIdx_];
    if(independent) {
      study_.unresolved &= ~bit;
      if(study_.review) study_.qUnresolved &= ~bit;
    } else {
      study_.unresolved |= bit;
      if(study_.review) study_.qUnresolved |= bit;
      // At most two retries; intervening questions before seeing it again.
      if(study_.attempts[exIdx_]<3 && study_.length<96) {
        uint8_t at=study_.cursor+4;
        if(at>study_.length) at=study_.length;
        for(int j=study_.length;j>at;--j) {study_.plan[j]=study_.plan[j-1];qEntry_[j]=qEntry_[j-1];}
        study_.plan[at]=(uint8_t)exIdx_;
        qEntry_[at]=qEntry_[study_.cursor];
        ++study_.length;
      }
    }
  }
  int paragraph(Canvas& c,const char* text,int y,const LgFont* font,int cap=16) {
    if(!text) return y;
    const char* lines[28];int lens[28];
    int n=c.wrapText(font,text,c.width()-2*c.margin(),lines,lens,cap>28?28:cap);
    for(int i=0;i<n && y+font->advanceY<=cmt().bottom;++i,y+=font->advanceY+4)
      c.drawTextN(c.margin(),y,font,lines[i],lens[i]);
    return y;
  }
  // Return the first unread byte. No fixed whole-page line cap: each screen
  // wraps only its own suffix, including arbitrarily long JSON pages.
  uint16_t textEnd(const char* text,uint16_t offset,Canvas* draw=nullptr) {
    const char* lines[28];int lens[28];auto mt=cmt();auto body=fontByRole(FontRole::Body);
    int per=(mt.bottom-mt.top-body->advanceY)/bodyLineH()+1;
    if(per<1)per=1;
    if(per>28)per=28;
    const int n=canvas_->wrapText(body,text+offset,mt.w-2*mt.m,lines,lens,per);
    int y=mt.top;
    if(draw) for(int i=0;i<n;++i,y+=bodyLineH()) draw->drawTextN(mt.m,y,body,lines[i],lens[i]);
    const char* end=n?lines[n-1]+lens[n-1]:text+offset;
    while(*end==' ' || *end=='\n' || *end=='\r') ++end;
    return (uint16_t)(end-text);
  }
  bool textNext(const Exercise& e,uint8_t& page,uint16_t& offset) {
    uint16_t next=textEnd(e.pages[page],offset);
    if(e.pages[page][next]) {offset=next;return true;}
    if(page+1<e.pageCount) {++page;offset=0;return true;}
    return false;
  }
  bool textBack(const Exercise& e,uint8_t& page,uint16_t& offset) {
    if(!offset) {
      if(!page)return false;
      --page;offset=(uint16_t)strlen(e.pages[page]);
    }
    uint16_t at=0;
    while(true) {uint16_t next=textEnd(e.pages[page],at);if(next>=offset || next<=at)break;at=next;}
    offset=at;return true;
  }
  void renderText(Canvas& c,const Exercise& e,uint8_t page,uint16_t offset) {
    auto mt=cmt(); char count[32];
    unsigned current=1,total=0;
    for(uint8_t p=0;p<e.pageCount;++p) {
      uint16_t at=0;
      do {++total;if(p==page && at==offset)current=total;
        uint16_t next=textEnd(e.pages[p],at);if(next<=at)break;at=next;
      } while(e.pages[p][at]);
    }
    snprintf(count,sizeof(count),S(PageFmt),current,total);
    chrome::header(c,mt,e.readingTitle,count);textEnd(e.pages[page],offset,&c);
    c.drawText(mt.m,mt.widgetY,fontByRole(FontRole::UI),U("Hold OK: vocabulary","Удерж. OK: словарь"));
  }
  void renderRecall(Canvas& c,const Exercise& e) {
    auto mt=cmt();auto body=fontByRole(FontRole::Body);auto ui=fontByRole(FontRole::UI);
    chrome::header(c,mt,U("RECALL","ВСПОМНИТЕ"));
    int y=mt.top;
    if(!revealed_ || c.portrait()) y=paragraph(c,e.prompt,y,body,6)+16;
    if(!revealed_) {
      paragraph(c,U("Say the answer before revealing it. OK: show answer. Back: reference / exit.",
        "Сначала произнесите ответ. OK: показать ответ. Назад: справка / выход."),y,ui,8);
      return;
    }
    y=paragraph(c,e.answer,y,body,6)+18;
    if(!answered_) {
      // Three honest outcomes; the cursor starts on the conservative one.
      if(c.portrait()) {
        chrome::menuRow(c,mt,y,U("NOT YET","НЕ ВСПОМНИЛ"),U("Wrong or incomplete","Ошиблись или неполно"),sel_==0);
        y+=mt.menuRowH+8;
        chrome::menuRow(c,mt,y,U("WITH HINT","С ПОДСКАЗКОЙ"),U("Partly, or after help","Частично или с помощью"),sel_==1);
        y+=mt.menuRowH+8;
        chrome::menuRow(c,mt,y,U("RECALLED","ВСПОМНИЛ"),U("Correct before revealing","Верно до показа ответа"),sel_==2);
      } else {
        chrome::settingRow(c,mt,y,U("NOT YET","НЕ ВСПОМНИЛ"),"",sel_==0);
        chrome::settingRow(c,mt,y+mt.rowH+8,U("WITH HINT","С ПОДСКАЗКОЙ"),"",sel_==1);
        chrome::settingRow(c,mt,y+2*(mt.rowH+8),U("RECALLED","ВСПОМНИЛ"),"",sel_==2);
      }
    } else {
      const char* msg =
        chosen_==2 ? (helped_ ? U("With help — we will practise again. OK: next.","С подсказкой — повторим ещё раз. OK: дальше.")
                              : U("Recorded. OK: next.","Записано. OK: дальше."))
                   : chosen_==1 ? U("Partial — it comes back tomorrow. OK: next.","Частично — вернёмся завтра. OK: дальше.")
                                : U("It comes back soon. OK: next.","Вернёмся к этому скоро. OK: дальше.");
      y=paragraph(c,msg,y,ui,4)+12;
      paragraph(c,e.explain,y,ui,8);
    }
  }
  int readingIndex() const {
    for(int i=exIdx_;i>=0;--i) if(lesson_.exercises[i].type==ExType::Reading) return i;
    return -1;
  }
  void openRules() {
    referenceMode_=3;theoryIdx_=0;theoryLine_=0;
    for(uint8_t i=0;i<lesson_.theoryCount;++i) {
      const char* title=lesson_.theory[i].title;
      if(title && (strstr(title,"СЛОВАР") || strstr(title,"VOCAB"))) {theoryIdx_=i;break;}
    }
  }
  void renderReference(Canvas& c) {
    auto mt=cmt();
    if(referenceMode_==2) {renderText(c,lesson_.exercises[readingIndex()],refPage_,refOffset_);return;}
    if(referenceMode_==3) {renderTheory(c);return;}
    chrome::header(c,mt,U("REFERENCE","СПРАВКА"));
    const char* labels[]={U("RETURN TO QUESTION","К ВОПРОСУ"),U("READ TEXT","ПРОЧИТАТЬ ТЕКСТ"),
      U("RULES / VOCABULARY","ПРАВИЛА / СЛОВАРЬ"),U("EXIT LESSON","ВЫЙТИ ИЗ УРОКА")};
    for(int i=0;i<4;++i) chrome::settingRow(c,mt,mt.top+i*(mt.rowH+12),labels[i],"",referenceSel_==i);
  }
  Nav handleReference(Key k) {
    if(referenceMode_==1) {
      if(k==Key::Back) {referenceMode_=0;return Nav::RedrawFull;}
      if(k==Key::Up || k==Key::Left) referenceSel_=(referenceSel_+3)%4;
      if(k==Key::Down || k==Key::Right) referenceSel_=(referenceSel_+1)%4;
      if(k==Key::Ok) {
        if(referenceSel_==0) referenceMode_=0;
        if(referenceSel_==1 && readingIndex()>=0) {referenceMode_=2;refPage_=0;refOffset_=0;}
        if(referenceSel_==2 && lesson_.theoryCount) {openRules();helped_=true;}
        if(referenceSel_==3) {referenceMode_=0;exitPrompt_=true;}
      }
      return Nav::RedrawFull;
    }
    if(k==Key::Back) {referenceMode_=phase_==Phase::Reading?0:1;return Nav::RedrawFull;}
    if(referenceMode_==2) {
      auto& e=lesson_.exercises[readingIndex()];
      if(k==Key::Up || k==Key::Left) textBack(e,refPage_,refOffset_);
      if(k==Key::Down || k==Key::Right || k==Key::Ok)
        if(!textNext(e,refPage_,refOffset_)) referenceMode_=1;
      if(k==Key::OkLong && lesson_.theoryCount) {openRules();helped_=true;}
    } else {
      if(k==Key::Up || k==Key::Left) {
        if(theoryLine_ || theoryIdx_) theoryPageBack();
      }
      if(k==Key::Down || k==Key::Right || k==Key::Ok) {
        theoryLine_+=theoryPageSize(lesson_.theory[theoryIdx_],theoryLine_);
        if(theoryLine_>=theoryVisualTotal(lesson_.theory[theoryIdx_])) {
          theoryLine_=0;++theoryIdx_;
          if(theoryIdx_>=lesson_.theoryCount) {theoryIdx_=0;referenceMode_=phase_==Phase::Reading?0:1;}
        }
      }
    }
    return Nav::RedrawFull;
  }

  StudySession study_;
  uint16_t readingOffset_=0,refOffset_=0;
  uint16_t rAnswer_=0,rCorrect_=0;   // whole-queue review totals
  uint8_t qEntry_[96]={};            // plan slot -> queue entry (review)
  bool chainNeeded_=false;
  uint8_t referenceMode_=0,referenceSel_=0,refPage_=0;
  bool helped_=false,revealed_=false;
  Lesson lesson_;  // ~26KB — LessonScreen is statically allocated inside App
  LessonMeta meta_;
  ProgressStore* progress_ = nullptr;
  Canvas* canvas_ = nullptr;

  Phase phase_ = Phase::Load;
  char errText_[96] = "";

  uint8_t theoryIdx_ = 0;
  uint8_t theoryLine_ = 0;
  uint16_t exIdx_ = 0;
  uint8_t sel_ = 0;
  uint8_t order_[cfg::MAX_OPTIONS];  // display row -> real option index
  uint8_t optTop_ = 0;               // first visible display row
  bool answered_ = false;
  uint8_t chosen_ = 0;               // real option index (post-shuffle)
  uint8_t pageIdx_ = 0;
  bool exitPrompt_ = false;
  bool explainView_ = false;
  uint8_t explainPage_ = 0;
  bool aborted_ = false;
  bool restored_ = false;
  bool gateMode_ = false, gatePassed_ = false;
  uint8_t gateSel_ = 0;
  uint16_t correctCount_ = 0;
  uint16_t answerCount_ = 0;
};
