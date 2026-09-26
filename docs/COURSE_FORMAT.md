# LingoInk Course Format (v1)

> Обновлено 2026-09-26: актуальные изменения учебного цикла, даты, повторов и прогресса описаны в [LEARNING_UPDATE.md](LEARNING_UPDATE.md). При расхождении старого описания с обновлением действует обновление.

Контент живёт на SD-карте и отделён от прошивки: движок не знает языка
курса. Один курс — один каталог под `/courses/`. Прошивка сканирует
`/courses/*/course.json` и берёт первый валидный (мультивыбор — v0.3).

```
/courses/english_ru/
  course.json           # манифест курса
  lessons/*.json        # уроки
/lingoink/              # создаёт прошивка
  progress.json
```

**Правило совместимости**: поля неизвестной версии/типа игнорируются, но
не ломают загрузку; добавлять поля можно, менять смысл существующих —
нельзя (см. AGENTS.md).

## course.json (манифест)

```json
{
  "format": 1,
  "id": "english_ru",
  "title": "English A2 → B1",
  "from": "ru",
  "to": "en",
  "levels": ["A2", "B1"],
  "lessons": [
    { "id": "a2_01", "file": "lessons/a2_01_vocab.json",
      "level": "A2", "kind": "vocab", "title": "Everyday words" }
  ]
}
```

- `id` — стабильный id курса; `lessons[].id` — стабильный id урока (по
  нему работает resume; переименовывать нельзя).
- `kind` — `vocab | grammar | reading | mixed` (сейчас только UI/статистика).
- `format`, `levels`, `from`, `to` — парсером сегодня не читаются
  (резерв); обязательны `id` и непустой `lessons[]` с `id`+`file`.
- Манифест ≤16 KB, ≤24 уроков (`MAX_LESSONS`).

## Файл урока

```json
{
  "format": 1,
  "id": "a2_02",
  "title": "Present Perfect",
  "level": "A2",
  "kind": "grammar",
  "theory": [
    { "title": "PRESENT PERFECT",
      "lines": ["have / has + past participle", "", "Used when:", "• ..."] }
  ],
  "exercises": [ ... ]
}
```

Теория: ≤4 блоков, ≤20 строк на блок. Урок без `exercises`, но с
`theory` валиден (и наоборот).

### Упражнение — общие поля

| Поле | Тип | Значение |
|---|---|---|
| `type` | str | `choice` \| `cloze` \| `mistake` \| `reading` |
| `srs` | [str] | id learning items (≤4), напр. `["pp.form"]` |
| `tags` | [str] | паттерны для weak-areas (≤4), напр. `["present-perfect"]` |
| `explain` | str | пояснение по long-press OK |

**`choice`** — мультивыбор: слово, RU→EN (тег `ru2en` → шапка
TRANSLATE), реплика диалога, вопрос на понимание (тег `reading` →
шапка QUESTION).

```json
{ "type": "choice", "prompt": "actually",
  "promptSmall": "Выберите правильный перевод",
  "options": ["на самом деле", "актуально", "случайно", "обычно"],
  "correct": 0, "srs": ["vp.actually"], "tags": ["vocab"],
  "explain": "actually = на самом деле..." }
```

**`cloze`** — заполнить пропуск (`___` в `prompt`):

```json
{ "type": "cloze", "prompt": "Yesterday I ___ to the store.",
  "options": ["went", "go", "gone", "going"], "correct": 0,
  "srs": ["ps.went"], "tags": ["past-simple"] }
```

**`mistake`** — найди ошибку (варианты вида `«ошибка → исправление»`):

```json
{ "type": "mistake", "prompt": "She don't like coffee.",
  "options": ["don't → doesn't", "like → likes", "no mistake"],
  "correct": 0, "srs": ["gd.does"], "tags": ["do-does"] }
```

**`reading`** — многостраничный текст; вопросы идут **следующими**
упражнениями (`choice`/`cloze` с тегом `reading`):

```json
{ "type": "reading", "title": "A DAY IN LONDON",
  "pages": ["Tom arrived...", "First, he walked..."] }
```

### Лимиты движка (`src/config.h`)

- Урок: ≤48 упражнений, ≤18 KB строк (пул), файл JSON ≤36 KB.
- Упражнение: ≤6 вариантов (на экран помещается ~4 — остальные
  обрезаются; планируйте ≤4), чтение ≤12 страниц.
- SRS-таблица прогресса: ≤250 items на курс.

### Зарезервировано (не в v0.1)

- `build` — sentence builder: `tokens[]`, `answer`;
- `dialogue` — ветвящиеся диалоги: `turns[]` с choice-узлами;
- серверные паки упражнений по слабым тегам — формат идентичен уроку.

## Конвенции id learning items

- `vp.*` — vocabulary pair (`vp.actually`);
- `pp.*` / `ps.*` — grammar patterns (present perfect / past simple);
- `gd.*` — common mistakes; `rd.*` — reading concepts.

Id — контракт между контентом и SRS: один id в разных уроках
склеивается в один интервал повторения.

## Additive fields in the learning update

`recall` exercises require `prompt`, `answer`, `explain`, `srs` and `tags`.
They have no options/correct in JSON; the UI provides reveal and self-assessment.
Optional `skill`: `grammar`, `vocabulary`, `reading` overrides inference by type.
Optional `reviewable`: false excludes story comprehension from item scheduling.
Default true keeps old packs compatible. All new sample questions have explanations.
`objective` is authoring metadata; its visible equivalent is the first theory block.
`lessons[].legacyIndex` preserves the original completion-mask position on reorder.
Existing IDs remain stable. Progress v2 includes date, stable lesson states and a
single session bookmark; missing new fields are accepted when loading v1.
Capacity remains 48 exercises, 4 theory blocks, 18 KB string pool and 36 KB JSON.
New recall prompts/answers must fit six body lines each in portrait. Reading source
pages are automatically subdivided; question text/options remain bounded by layout.
