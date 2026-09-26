# LINGOINK TECHNICAL PLAN

**LingoInk** — специализированная офлайн-прошивка языкового тренажёра для
XTEINK X4. Не читалка: одна цель, один экран за раз, физические кнопки,
e-ink, ноль отвлекающих функций.

План основан на исследовании экосистемы X4 и рабочего кода проектов
CrossPoint Reader, eenk и freeink-sdk (сентябрь 2026).

---

## 1. Hardware (XTEINK X4)

| Компонент | Значение | Следствие для LingoInk |
|---|---|---|
| SoC | ESP32-C3, RISC-V 1 ядро 160 МГц | Никакой многопоточности в UI; всё в одном loop |
| RAM | 400 KB SRAM, **~380 KB heap**, без PSRAM | Один полноэкранный canvas 48 KB (BSS) + Lesson ~26 KB; никакого heap в steady state |
| Flash | 16 MB, раздел app0 6.4 MB @ 0x10000 | Шрифты и прошивка в flash; контент на SD |
| Дисплей | 4.3" e-ink **800×480**, контроллер **SSD1677** (основной; партии UC8179/UC8279 — определяется автоматически) | 1-bpp рендеринг; 3 режима refresh (FULL/HALF/FAST) |
| Touch / подсветка | нет / нет | Только физические кнопки, ч/б интерфейс |
| Кнопки | ADC-лестница GPIO1: **Back, OK, Left, Right**; GPIO2: **Up, Down** (боковые); Power = GPIO3 | Схема управления из ТЗ совпадает с железом 1:1 |
| Дисплей SPI | SCLK=8, MOSI=10, CS=21, DC=4, RST=5, BUSY=6 | Задано в BoardConfig freeink-sdk |
| SD | SPI: MISO=7, CS=12, шина общая с дисплеем | Контент и прогресс на карте |
| Батарея | 650 mAh, ADC-гейдж на GPIO0 | Индикатор через BatteryMonitor |
| Питание | power-latch GPIO13, deep sleep с пробуждением по кнопке Power | Автосон через PowerManager |
| USB | USB-C CDC serial | Логи + esptool прошивка |

X4 Pro (ESP32-S3 + PSRAM + touch) — отдельная цель, не входит в MVP;
архитектура HAL это допускает (как делает eenk).

## 2. Existing ecosystem

| Проект | Что это | Роль для LingoInk |
|---|---|---|
| **Free-Ink/freeink-sdk** | Аппаратный SDK: BoardConfig, FreeInkDisplay (драйверы SSD1677/UC8179/UC8253/UC8279/IT8951), InputManager, SDCardManager, PowerManager, BatteryMonitor, XteinkDetect | **Наш hardware-слой.** Используем 6 библиотек; reader-движок (FreeInkBook) и UI-фреймворк (FreeInkUI) не берём |
| **t0mg/eenk** | Прошивка интерактивной литературы (не-читалка!) на X3/X4/X4Pro поверх freeink-sdk | Архитектурный образец: HAL-паттерн, PlatformIO env'ы, partition layout. Идея «не-читалка на X4» уже проверена сообществом |
| **crosspoint-reader** | Главная open-source читалка X4 (8k+ звёзд) | Только референс; app-код не переносим |
| **bigbag/papyrix-reader** | Лёгкая альтернативная прошивка | Референс подхода «минимальный рендеринг» |
| Offline Anki на X4 / Pocket Daily | Комьюнити-проекты учёбы на X4 | Валидация спроса на обучающие приложения |

Способ интеграции: библиотеки freeink-sdk **вендорятся в `lib/`** проекта
(сохранены LICENSE и NOTICE) — тот же паттерн, что у CrossPoint/eenk через
submodule, но без сетевой зависимости при сборке.

## 3. Licenses

Проект потенциально коммерческий → проверен каждый компонент.

| Компонент | Лицензия | Коммерческое использование | Обязательства | Риск |
|---|---|---|---|---|
| freeink-sdk (Free-Ink) | MIT | ✅ да | Сохранить copyright-уведомление (сделано: `lib/freeink-sdk-LICENSE`) | нет |
| eenk (t0mg) | MIT | ✅ да (не используем код напрямую) | — | **⚠ файл `lib/GfxRenderer/src/OrderedDither.h` внутри eenk — GPL-3.0 (из bb_epaper). В LingoInk НЕ копируется** (дизеринг не нужен) |
| crosspoint-reader | MIT | ✅ да (код не используется) | — | нет |
| papyrix-reader | MIT | ✅ да (код не используется) | — | нет |
| epdiy (vroland) | LGPL-3.0 (код), MIT (утилиты) | ⚠ частично | — | Формат шрифтов epdiy-подобный, но **структуры `LgFont` написаны заново**; конвертер шрифтов наш (freetype-py). Код epdiy не копируется |
| bb_epaper (Larry Bank) | GPL-3.0 | ❌ | — | Не используем; файлы с ним (например, OrderedDither.h из eenk) не переносим |
| ArduinoJson (Benoit Blanchon) | MIT | ✅ да | attribution в NOTICE | нет |
| arduino-esp32 (Espressif) | LGPL-2.1 | ✅ да (стандартная практика) | LGPL относится к самой библиотеке (исходники публичны); app-код не обязан раскрываться | низкий; при желании перейти на чистый ESP-IDF (Apache-2.0) |
| ESP-IDF (Espressif) | Apache-2.0 | ✅ да | NOTICE | нет |
| PlatformIO | Apache-2.0 | ✅ да (tooling) | — | нет |
| Inter (Rasmus Andersson) | OFL 1.1 | ✅ да, встраивание субсетов | attribution + не использовать имя шрифта для производных; OFL-текст в `LICENSE-NOTES.md` | нет |
| Шрифты, сгенерированные `tools/make_font.py` | Наши данные (Inter субсеты) | ✅ | соблюсти OFL | нет |

**Вывод: ядро LingoInk строится только на MIT/Apache/OFL компонентах.
GPL-кода в дереве проекта нет** (проверено; единственный GPL-файл в
экосистеме — `OrderedDither.h` у eenk — осознанно не переносится).

Собственный код LingoInk: рекомендуемая стратегия — закрытый license на
app-слой (`src/`, кроме вендоренного) + открытые copyright-уведомления
третьих сторон (файл `LICENSE-NOTES.md` уже ведётся).

## 4. Proposed architecture

```
┌────────────────────────────────────────────────────┐
│ LingoInk application                               │
│  screens/ (Home, Lesson, Progress)  app/ (App FSM) │
├────────────────────────────────────────────────────┤
│ Domain: course/ (types+parser)  srs/ (SM-2,        │
│  mastery)  progress/ (ProgressStore, SD JSON)      │
│ Rendering: ui/ (Canvas 1bpp, LgFont, Presenter,    │
│  refresh policy)                                   │
├────────────────────────────────────────────────────┤
│ input/InputLoop (Keys + long-press + idle)         │
├────────────────────────────────────────────────────┤
│ freeink-sdk (MIT): EInkDisplay, InputManager,      │
│  BoardConfig, PowerManager, BatteryMonitor,        │
│  XteinkDetect                                      │
├────────────────────────────────────────────────────┤
│ Arduino-ESP32 / ESP-IDF (ESP32-C3)                 │
└────────────────────────────────────────────────────┘
```

Принципы:
- **Домен не знает про железо**: `course/`, `srs/` — чистый C++ без Arduino-типов, тестируется на хосте (`pio test -e native`).
- **Один canvas**: вся отрисовка в один 1-bpp буфер 48 KB (static), затем один `displayBuffer(mode)`.
- **Screen = класс** с `render(Canvas&)` + `handleKey(Key) → Nav`; экраны статически размещены в App, heap в рантайме не используется (кроме временного буфера чтения JSON).
- **Никаких анимаций**: единственный «motion» — инверсия выбранной строки.

## 5. Folder structure

```
lingoink/
  platformio.ini          # env: esp32c3 (X4), native (tests)
  partitions.csv          # CrossPoint-совместимый layout (app0 @ 0x10000)
  docs/                   # этот план + спецификация формата курса
  tools/make_font.py      # TTF → LgFont .h (freetype-py)
  lib/                    # вендоренный freeink-sdk (6 библиотек, MIT) + ArduinoJson
  src/
    main.cpp              # boot: detect → display → App::run
    config.h              # все константы продукта
    app/App.h             # машина состояний + event loop + power
    ui/                   # Canvas, LgFont, fonts/, FontRegistry, Presenter
    input/                # Keys, InputLoop (long-press, idle)
    course/               # Types, LessonLoader (ArduinoJson), CourseCatalog
    exercises             # (типы упражнений живут в course/Types.h + LessonScreen)
    srs/                  # SrsScheduler (SM-2 lite), Mastery (EMA)
    progress/             # ProgressStore (JSON на SD, атомарная запись)
    screens/              # Home, Lesson, Progress (+ Screen base)
  test/                   # Unity-тесты домена (native)
  data/sd/english_ru/     # сэмпл-курс → копируется на SD в /courses/english_ru
```

## 6. Course data format

Контент отделён от прошивки; движок не знает языка курса. Полная
спецификация — `docs/course-format.md`. Кратко:

```
/courses/english_ru/
  course.json              # manifest: id, title, from/to, lessons[]
  lessons/*.json           # теория + упражнения
/lingoink/
  progress.json            # пользовательское состояние (создаёт прошивка)
```

Урок = массив упражнений; типы: `choice` (мультивыбор / RU→EN / диалог),
`cloze` (пропуск), `mistake` (найди ошибку), `reading` (многостраничный
текст; вопросы идут следующими упражнениями с тегом `reading`).
Каждое упражнение несёт **`srs`** (id learning items) и **`tags`**
(грамматические паттерны). Зарезервированы, но не в MVP: `build`
(sentence builder), ветвящиеся диалоги.

## 7. Exercise engine

Упражнение — плоская структура (`Exercise` в `course/Types.h`):
type + prompt/promptSmall + options[] + correct + explain + srs[] + tags[].
`LessonScreen` — конечный автомат фаз `Theory → Reading → Exercise → Summary`;
рендеринг типа упражнения отделён от оценки ответа. Оценка → `recordResult()`:
- каждому `srs` id — SM-2 grade (5 верно / 1 ошибка);
- mastery по навыку (Vocabulary/Grammar/Reading/Production) и по каждому тегу.

Новизна против «карточек Anki»: теги позволяют системе при ошибках в
паттерне (например `pp-vs-past`) **чаще выдавать новые упражнения с этим
тегом** (см. §8) — разные карточки на одну слабость.

## 8. Persistence

`/lingoink/progress.json` (ArduinoJson, запись tmp→rename):
`lastLesson, goalMin, todayMin, streak, day, uptimeMin, srs[400], skills, tags`.

**Счёт дней без RTC/сети**: копим активные минуты (`uptimeMin`),
день = uptime/24ч, сохраняется при каждом save и восстановлении. При
появлении NTP (Wi-Fi milestone) — переключение на wall-clock день.

## 9. E-ink rendering strategy

- Весь экран рисуется в canvas → один `drawImage` + один `displayBuffer`.
- `Presenter` политика: **FAST** — движение курсора выбора; **HALF** —
  переходы фидбека/фаз; **FULL** — смена экрана; принудительный FULL после
  8 FAST подряд (анти-ghosting), плюс FULL на splash.
- Инверсия выбранной строки вместо миганий/анимаций.
- Теория/чтение — постраничная разбивка, полный refresh на страницу
  (как в читалках — ghosting недопустим на тексте).

## 10. Spaced repetition

SM-2 lite (`srs/SrsScheduler.h`): ease 1.3–2.8 (uint8×100), интервалы
1 → 6 → interval×ease, провал → reps=0, interval=1, ease−0.2. Learning item —
что угодно со стабильным id (`vp.actually`, `pp.form`, `gd.does`).
`Mastery`: EMA (α=0.2) по навыкам и тегам; weak areas = теги с EMA<порога
и ≥5 ответов. Планировщик REVIEW (milestone 5) собирает сессию из due-items
+ **новых упражнений слабых тегов**.

## 11. MVP roadmap

| # | Milestone | Статус |
|---|---|---|
| M1 | Boot → splash → main menu | ✅ код написан |
| M2 | Course JSON с SD → Choice/Cloze упражнения | ✅ код написан |
| M3 | Progress persistence + resume | ✅ код написан |
| M4 | Grammar theory + Reading + comprehension | ✅ код написан |
| M5 | SRS ядро (SM-2 + mastery + weak tags) | ✅ код написан; REVIEW-сессия из due items — next |
| — | Тестирование на железе, полировка refresh-политики | после сборки |
| — | Sentence builder, диалоги | post-MVP |
| — | Wi-Fi: загрузка курсов, бэкап прогресса, OTA | post-MVP (SecureNet/wolfSSL уже в SDK) |
| — | Сервер + генерация упражнений по слабостям | post-MVP; формат srs/tags это уже поддерживает |

Порядок верификации: `pio test -e native` (домен) → `pio run -e esp32c3`
(прошивка) → прошивка app0 @ 0x10000 на X4 → проход сэмпл-курса →
перезагрузка → resume.

## 12. Risks

| Риск | Митигция |
|---|---|
| USB-locked устройства (AliExpress партии): кастомный bootloader/partitions могут окирпичить; unlocker официально поддерживает только CrossPoint/CrossInk | Прошивка **только app0 @ 0x10000** поверх стока/CrossPoint (bootloader и NVS не трогаем). Устройства покупать на xteink.com. Для дистрибуции — договориться о добавлении LingoInk в unlocker / поставлять предустановленные устройства |
| Партии панелей X4 различаются (SSD1677/UC8179/UC8279) | Используем `XteinkDetect` + `selectXteinkDevice()` из SDK (NVS hw_calib + проб bus) |
| ~380 KB heap: Lesson 26 KB + canvas 48 KB + JSON буферы | Всё статическое; JSON-буферы короткоживущие; пулы ограничены в config.h |
| Нет RTC/NTP → нет календарных дней для SRS | Uptime-based day counter (§8); NTP позже |
| Ghosting при частых FAST | Политика Presenter (§9) + FULL на текстовых страницах |
| Клавиши на ADC: дребезг/шум при загрузке | InputManager SDK уже фильтрует (eenk дополнительно семплирует 5×) — наш InputLoop наследует это |
| Лицензионная чистота при коммерции | Только MIT/Apache/OFL в дереве; GPL-файлы экосистемы не переносятся (§3); LICENSE-NOTES.md ведётся |
```
