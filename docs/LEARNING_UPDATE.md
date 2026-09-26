# Learning update — 2026-09-26

## Implemented

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
