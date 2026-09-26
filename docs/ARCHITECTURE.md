# LingoInk — архитектура

> Обновлено 2026-09-26: актуальные изменения учебного цикла, даты, повторов и прогресса описаны в [LEARNING_UPDATE.md](LEARNING_UPDATE.md). При расхождении старого описания с обновлением действует обновление.

Слои сверху вниз; нижний слой не знает о верхних, домен не знает о железе.

```
┌──────────────────────────────────────────────────────────┐
│ app/ App.h        — FSM экранов, event loop, power       │
│ screens/          — Home, Lessons, Lesson, Progress,     │
│                     Settings (+ Screen — контракт Nav)   │
├──────────────────────────────────────────────────────────┤
│ Domain (чистый C++, тестируется на хосте):               │
│   course/   Types.h, LessonLoader, CourseCatalog         │
│   srs/      SrsScheduler (SM-2 lite), Mastery (EMA)      │
│   progress/ ProgressStore (JSON на SD; зависит от SD.h)  │
├──────────────────────────────────────────────────────────┤
│ ui/    Canvas (1-bpp 800×480, статический буфер,         │
│        ориентация = трансформация координат),            │
│        LgFont + fonts/, FontRegistry, Chrome, BootCards, │
│        Presenter, Log                                     │
│ input/ Keys, InputLoop (long-press, idle)                │
├──────────────────────────────────────────────────────────┤
│ freeink-sdk (vendored, MIT): FreeInkDisplay(+драйверы),  │
│   InputManager, BoardConfig, PowerManager, XteinkDetect, │
│   BatteryMonitor (индикатор на Home)                     │
├──────────────────────────────────────────────────────────┤
│ Arduino-ESP32 / ESP-IDF (ESP32-C3)                       │
└──────────────────────────────────────────────────────────┘
```

## Hardware layer

`main.cpp` — boot в жёстком порядке (не менять):
1. USB serial;
2. `selectXteinkDevice()` — X3 vs X4 (I2C-fingerprint);
3. `applyXteinkDisplayController()` — probe партии панели
   (SSD1677 → UC8179/UC8279) **до** захвата SPI;
4. `SPI.begin` + `SD.begin` — **до** инициализации дисплея
   (общая шина; обратный порядок = баг на железе);
5. `display->begin()` — framebuffer ~48 KB аллоцируется **первым** с
   чистого heap (проверка: begin() должен съесть ≥45 KB);
6. `new App(...)` (~90 KB) → `app->run()` (не возвращается).

## Application layer (app/App.h)

FSM `Splash → Home ⇄ Lessons / Lesson / Progress / Settings`. Один цикл:
опрос `InputLoop` (≤1 клавиша за итерацию, `delay(15)` при пустых),
диспетчеризация в экран, редрав по `Nav`, счётчик активных минут (1/мин),
автосон через 10 мин простоя, Power (удержание ~1 с) → карточка
выключения + deep sleep; короткое нажатие Power работает как OK.
Ориентация — всегда портрет 180° (решение пользователя; поле `orient`
в progress.json игнорируется).

## UI

- **Canvas** — единственный полноэкранный 1-bpp буфер (48 KB, BSS).
  Бит: 1=белый, 0=чёрный. Примитивы: линии, рамки, залитые и
  скруглённые прямоугольники (`roundRect`/`roundFrame`), пунктир
  (`hlineDash`), blit 1-bpp битмапа (`bitmap1`), текст UTF-8, greedy
  word-wrap, инверсия строк, прогресс-бар. Ориентации: буфер всегда
  панельный 800×480; все 4 поворота (landscape/portrait ± 180°,
  логические 800×480 или 480×800) — трансформация координат в
  примитивах (`width()/height()` логические), второго framebuffer нет.
- **Chrome** (`ui/Chrome.h`) — общий визуальный язык: метрики шапки/низа/
  строк из `config.h` (один источник вместо магических чисел), шапка с
  линейкой и усечением «…», индикаторы скролла, скруглённое выделение,
  пунктирные разделители, строки меню/настроек (длинная строка в фокусе
  разворачивается на 2 строки), пилюли-прогресс, чипы, единственный стиль
  модалок (очистка экрана + карточка r=20 + pill-кнопки), виджет батареи.
  Экраны рисуют только через него — дрейф стилей исключён.
- **BootCards** (`ui/BootCards.h`) — чистые функции отрисовки splash и
  карточки выключения (используются App и host-превью).
- **Presenter** — политика refresh (см. ниже), единственный, кто трогает
  дисплей. Все интерактивные обновления — FAST; FULL — только по
  `fullNext()` (карточка выключения, Clean Screen Now, смена ориентации,
  RESET) или по включённому пользователем Auto Clean. `App` переводит
  Nav → Refresh одним правилом (`refreshFor`); фактическая волна —
  всегда за Presenter.
- **Fonts** — LgFont (flash-субсеты Inter: title36/ui22/body28/body28b/
  display56 — крупный шрифт постерных моментов; Latin+Cyrillic).
  Генератор: `tools/make_font.py`.

## Exercise engine

Типы в `course/Types.h` (`Exercise`): `choice | cloze | mistake |
reading`. Рендер типа и логика ответа живут в `LessonScreen` (фазы
`Theory → Reading → Exercise → Summary`). Контракт v0.2: варианты
перемешиваются при входе в упражнение (стабильны до конца вопроса,
проверка по исходному `correct`); Back никогда не означает «далее»
(выход из урока — через «EXIT LESSON?», aborted-урок не двигает
`lastLessonId`); после ответа feedback пропускается только OK. Ответ →
`recordResult()`: каждый `srs[]` id получает SM-2 grade (5 верно /
1 ошибка), mastery обновляется по навыку и тегам.

## Course engine

`CourseCatalog` сканирует `/courses/*/course.json`, берёт первый
валидный манифест (≤24 уроков). `LessonLoader` парсит урок (ArduinoJson)
в статический `Lesson` со строковым пулом 18 KB: ≤48 упражнений,
≤6 вариантов, ≤12 страниц чтения, ≤4 theory-блоков, JSON файла ≤36 KB.
Ошибки парсинга показываются экраном LESSON ERROR.

## Progress

`ProgressStore` — всё состояние пользователя в `/lingoink/progress.json`
(atomic: tmp → rename): courseId, lastLessonId (resume), goal/today
minutes, streak, day, uptimeMin, srs[≤250], skills, tags + настройки
(orient, cleanMode, cleanEvery; старые прошивки их игнорируют).
`reset()` возвращает всё к заводским значениям (RESET PROGRESS).
День продвигается по вручную подтверждённой календарной дате. Сохранение — после значимых изменений занятия и при выключении; есть резервный progress.bak.

## SRS

SM-2 lite (`srs/SrsScheduler.h`): ease 1.3–2.8, интервалы 1→6→×ease,
провал → reps=0, interval=1, ease−0.2; cap 365 дней.
`Mastery` — EMA α=0.2 по 4 навыкам и ≤24 тегам; weak tags = EMA-топ с
≥5 ответами.

## Storage

| Что | Где | Лимит |
|---|---|---|
| Курсы | SD `/courses/<id>/` | 1 активный курс (первый найденный) |
| Прогресс | SD `/lingoink/progress.json` | ≤48 KB, ≤250 SRS items |
| Шрифты | flash (`src/ui/fonts/`) | 5 субсетов |
| Прошивка | flash app0 @ 0x10000 | 6.4 MB (занято ~0.6 MB) |

## Зависимости модулей

- `App` → все экраны, CourseCatalog, ProgressStore, InputLoop, Presenter.
- Экраны → Canvas/FontRegistry, домен (Lesson, ProgressStore) — но не
  дисплей и не SD напрямую (кроме CourseCatalog/ProgressStore).
- `course/`, `srs/` — без Arduino-типов, собираются в `env:native`.
- `progress/` — единственный доменный модуль, знающий SD.
- Всё статическое; heap в steady state не используется.

## Refresh-политика (утверждена на железе)

- **Никаких multi-second full wipe во время сессии.** Все обновления —
  FAST DU (~85 ms, `FREEINK_X4_FAST_DU_SHORTCUT`).
- FULL refresh — только: карточка выключения (артворк сплэша
  «LingoInk / Английский · Русский / Один экран. Одна цель», она же
  lock screen и session-end чистка), CLEAN SCREEN NOW (запрос
  пользователя), смена ориентации, RESET PROGRESS и авто-режим
  Auto Clean, который пользователь включает сам в Settings
  (каждые 20/40 fast-обновлений; по умолчанию выключен).
- Boot без full refresh: splash → быстрый переход в меню.
- Правило для новых фич: без анимаций и миганий; «движение» =
  инверсия строки.

## Тесты

`pio test -e native` — Unity-тесты SRS и Mastery (нужен host g++;
на этой машине работает MinGW из `%USERPROFILE%\mingw64`, добавьте его
bin в PATH). Проверка на железе — `docs/HARDWARE_VALIDATION.md`.

**Host-превью UI** — `bash tools/preview/run.sh` из корня репозитория:
рендерит все экраны и состояния (те же Canvas/Chrome/экраны, что в
прошивке) в `out/preview/*.png` во всех 4 ориентациях; грузит реальные
уроки из `data/sd/`. Шимы Arduino/SD/BatteryMonitor/EInkDisplay в
`tools/preview/shim/` в прошивку не попадают. Контакт-лист для обзора:
`python tools/preview/contact_sheet.py <0..3>`.
