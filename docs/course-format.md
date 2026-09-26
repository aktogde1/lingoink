# LingoInk Course Format (v1)

Контент живёт на SD-карте и полностью отделён от прошивки. Один курс —
один каталог под `/courses/`. Прошивка сканирует `/courses/*/course.json`
и берёт первый валидный (в будущих версиях — выбор из списка).

```
/courses/english_ru/
  course.json           # манифест курса
  lessons/
    a2_01_vocab.json
    a2_02_grammar.json
    ...
/lingoink/              # создаётся прошивкой
  progress.json
```

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

- `id` — стабильный идентификатор курса (используется в прогрессе).
- `lessons[].id` — стабильный id урока (по нему работает resume).
- `kind` — `vocab | grammar | reading | mixed` (пока только для UI/статистики).

## Файл урока

```json
{
  "format": 1,
  "id": "a2_02",
  "title": "Present Perfect",
  "level": "A2",
  "kind": "grammar",
  "theory": [
    {
      "title": "PRESENT PERFECT",
      "lines": ["have / has + past participle", "", "Used when:", "• ..."]
    }
  ],
  "exercises": [ ... ]
}
```

### Упражнение

Общие поля всех типов:

| Поле | Тип | Значение |
|---|---|---|
| `type` | str | `choice` \| `cloze` \| `mistake` \| `reading` |
| `srs` | [str] | id learning items (до 4), например `["pp.form", "pp-vs-past"]` |
| `tags` | [str] | паттерны для статистики слабых мест (до 4), например `["present-perfect"]` |
| `explain` | str | текст по long-press OK |

**`choice`** — мультивыбор. Варианты использования: значение слова, RU→EN
перевод (добавьте тег `ru2en` — шапка покажет TRANSLATE), реплики диалога,
вопросы на понимание (тег `reading` → шапка QUESTION).

```json
{ "type": "choice", "prompt": "actually",
  "promptSmall": "Выберите правильный перевод",
  "options": ["на самом деле", "актуально", "случайно", "обычно"],
  "correct": 0, "srs": ["vp.actually"], "tags": ["vocab"],
  "explain": "actually = на самом деле..." }
```

**`cloze`** — заполнить пропуск. В `prompt` пишите предложение с `___`:

```json
{ "type": "cloze", "prompt": "Yesterday I ___ to the store.",
  "options": ["went", "go", "gone", "going"], "correct": 0,
  "srs": ["ps.went"], "tags": ["past-simple"] }
```

**`mistake`** — найди ошибку. Варианты пишите как `«ошибка → исправление»`:

```json
{ "type": "mistake", "prompt": "She don't like coffee.",
  "options": ["don't → doesn't", "like → likes", "coffee → coffees", "no mistake"],
  "correct": 0, "srs": ["gd.does"], "tags": ["do-does"] }
```

**`reading`** — многостраничный текст. Вопросы на понимание идут
**следующими** упражнениями (`choice`/`cloze` с тегом `reading`):

```json
{ "type": "reading", "title": "A DAY IN LONDON",
  "pages": ["Tom arrived...", "First, he walked...", "..."] }
```

### Лимиты движка (см. src/config.h)

- ≤ 48 упражнений и ≤ 24 KB строк на урок;
- ≤ 6 вариантов ответа; ≤ 12 страниц чтения; ≤ 4 theory-блоков.

### Планируемые типы (не в MVP)

- `build` — sentence builder: `tokens[]`, `answer`;
- `dialogue` — ветвящиеся диалоги: `turns[]` с choice-узлами;
- серверная генерация паков упражнений по слабым тегам — формат
  идентичен уроку, просто кладётся в `lessons/`.

## Конвенции id learning items

- `vp.*` — vocabulary pair (`vp.actually`);
- `pp.*` / `ps.*` — grammar patterns (present perfect / past simple);
- `gd.*` — common mistakes;
- `rd.*` — reading concepts.

Id — это контракт между контентом и SRS: один и тот же id в разных уроках
склеивается в один интервал повторения.
