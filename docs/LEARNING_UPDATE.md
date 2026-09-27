# Learning update — 2026-09-26

## Quality update — 2026-09-27, released as v0.3.2-beta (branch `learning8`)

- **Date once per power-on.** The date screen appears before the first study
  action after boot; later lessons and reviews in the same session start
  immediately. The manual editor stays in Settings, a reboot asks again, and
  backwards dates stay rejected.
- **One review queue across lessons.** «Повторить» builds a single queue of up
  to 12 due exercises from ALL studied lessons (due items only, free recall
  preferred, SRS ids deduped, scan order rotated by day). Lessons are loaded
  one at a time; slices chain without menus; one combined summary at the end;
  power-off mid-queue restores the bookmark (additive progress v2 fields).
  Weak-area practice from Progress keeps the single-lesson path.
- **Three-way honest recall grading** (`SrsScheduler::gradeRecall`):
  *Recalled* — success (grade 5, retention tracking), only without opened help
  (help downgrades it); *With hint* — partial knowledge: never grows the
  interval (retry tomorrow), half the ease penalty of a fail, not counted as a
  lapse; early/same-day hints are practice; *Not recalled* — full lapse.
  The menu cursor starts on the conservative option. Grades are unit-tested
  for all three outcomes and their same-day guards.
- **Course expanded to 40%+ active recall.** 65 new author-written recall
  tasks in new contexts across all 21 lessons, placed after their related
  recognition blocks. Repeatable-exercise recall share: 42/181 (23%) →
  107/266 (40.2%); SRS ids covered by recall: 35/116 (30%) → 93/117 (79%).
  The remaining 24 ids are minor or derivative; no filler was written for
  them. `tools/course_check.py` validates limits and prints these numbers.
- **«Already know» check.** A never-viewed lesson offers a short probe (up to
  5 key tasks, recall-first, no theory). Pass (≥80% independent) marks the
  lesson mastered; a weak result falls back to the full lesson. SRS items are
  created either way; resumes and reviews skip the gate.
- **Honest positioning kept.** Levels remain content labels («English:
  A2 / B1 practice»); no interface text promises a level jump or a deadline.

## Implemented (earlier the same day)

- Home → Review builds a batch of up to 12 due exercises from the first eligible
  lesson. Repeated taps process the remaining lessons. Known items only; free
  recall is preferred and examples rotate by calendar day. Weak-area practice is
  started with OK in Progress; up/down chooses a topic.
- Date is confirmed before study (year/month/day; left/right selects a field,
  up/down changes it). No RTC, network or uptime-based pretend dates. First date
  anchors old relative SRS days; later changes advance by actual elapsed dates.
  A backwards date is rejected. Settings also opens the date editor.
- Same-day successes and early practice cannot expand an interval. Failure takes
  precedence over an immediate retry. Ease storage now fits the full 130–280 range.
- `recall`: produce/say an answer, reveal with OK, then self-assess. This is an
  explicit self-assessment, not automatic speech recognition. Answer alternatives
  are explained. Assistance is recorded and cannot earn a successful SRS grade.
- Errors return after up to three intervening exercises, or at the end when fewer
  remain; at most two retries. The summary scores independent first attempts and
  shows unresolved questions. Immediate retry success does not erase an SRS lapse.
- Reading wraps every source page into as many screenfuls as required. Up/back
  goes to the previous screenful. On a question, Back opens text/rules/vocabulary
  and exit actions. Hold OK while reading opens the lesson vocabulary. Returning
  leaves the question intact. Story comprehension is not used as vocabulary SRS.
- 21 lessons: revised explanations and ambiguous choices, explanations for all
  215 questions, 42 free-recall tasks in new contexts, objectives for each module.
  Past Simple precedes Present Perfect; A2/B1 are content labels, not a measured
  level or a promise to reach B1 in seven days.
- Progress: recent answer accuracy, viewed/practised lesson counts, and items
  recalled independently on a later due day. Practised means >=80% first attempts;
  retention is a separate item-level measure. Weak tags require >=3 samples and
  <80% recent accuracy; capacity 64. Lesson marks: ~ viewed, check = practised.
- A persisted bookmark contains theory position, reading byte offset, question,
  feedback, assisted state and retry queue. Switching to a different lesson or
  review batch replaces the single active bookmark. Updating a lesson invalidates
  only its bookmark, using a content fingerprint; SRS remains keyed by stable IDs.
- Save after meaningful state changes and power-down. Cursor movement alone does
  not write SD. A backup file supports recovery after interrupted replacement.
  Save errors are visible. Course ordering uses stable lesson IDs; `legacyIndex`
  migrates old positional completion marks without resetting previous results.

## Limits and verification

Offline operation still requires the person to set the real date. Reading and
text exercises do not assess pronunciation or listening. Self-assessment depends
on honest comparison with the model answer. Old courses without `recall` keep
working; they can repeat recognition questions but do not earn free-recall
retention badges. The course is a compact practice pack, not complete CEFR coverage.

Run `pio test -e native`, `powershell -File tools/preview/test-learning.ps1`,
`bash tools/preview/run.sh`, and `pio run -e esp32c3`. Host integration checks cover
all course files, all reading text in four orientations, due selection, retries,
assistance, bookmarks, legacy migration, calendar arithmetic, save round-trip and
backup recovery. On-device button, SD removal and power-cycle checks remain manual.

## Content references

- [Cambridge: forget or leave](https://dictionary.cambridge.org/grammar/british-grammar/forget-or-leave)
- [Cambridge: always](https://dictionary.cambridge.org/us/grammar/british-grammar/always)
- [Cambridge: past simple or present perfect](https://dictionary.cambridge.org/grammar/british-grammar/past-simple-or-present-perfect)
- [Cambridge: going to](https://dictionary.cambridge.org/grammar/british-grammar/future-be-going-to-i-am-going-to-work)
- [Cambridge: dishes](https://dictionary.cambridge.org/dictionary/english/dishes)
