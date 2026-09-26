# LingoInk

**One device. One purpose. Learn a language.**

LingoInk — автономный офлайн языковой тренажёр для XTEINK X4
(ESP32-C3, 4.3" e-ink 800×480): короткие уроки, тесты, чтение, грамматика
и интервальное повторение. Без Wi-Fi, уведомлений и отвлекающих функций;
контент и прогресс живут на SD-карте.

**Версия: v0.3.0-beta** (2026-09-26) — ранняя бета: прошивка, формат
курсов и SRS ещё меняются.

Это отдельное приложение, а не форк читалки: из open-source экосистемы X4
берётся только hardware-слой [freeink-sdk](https://github.com/Free-Ink/freeink-sdk)
(MIT, вендорен в `lib/`).

## Документация

| Файл | О чём |
|---|---|
| `docs/PRODUCT.md` | идея продукта, аудитория, отличия от Anki/Duolingo |
| `docs/ARCHITECTURE.md` | слои, модули, зависимости |
| `docs/HARDWARE.md` | X4: экран, кнопки, память, ограничения |
| `docs/UX.md` | управление, экраны, refresh, lock screen |
| `docs/COURSE_FORMAT.md` | спецификация формата курсов (SD) |
| `docs/ROADMAP.md` | текущее состояние и планы версий |
| `docs/HARDWARE_VALIDATION.md` | чек-лист проверки на железе |
| `docs/TECHNICAL_PLAN.md` | исторический план (исследование 2026-09) |
| `AGENTS.md` | правила для AI-агентов |

## Сборка

PlatformIO, Python 3.10–3.13 (ограничение платформы pioarduino).
Системный Python 3.14 не подходит — используйте venv проекта:

```bat
%USERPROFILE%\lingoink-venv\Scripts\pio.exe run --project-dir . -e esp32c3
```

Если Python подходящей версии стоит системно:

```bash
pio run -e esp32c3              # сборка прошивки
pio test -e native              # тесты домена (SRS/mastery) — нужен host g++
```

## Прошивка

**esptool не работает из Git Bash (MSys) — шейте из cmd или PowerShell.**

Прошивается **только слот app0 @ 0x10000** (bootloader, partition table и
NVS с калибровкой панели не затрагиваются, `erase_flash` не используется).
**Внимание: `pio run -t upload` на этой плате перезаписывает ещё и
bootloader (0x0), partition table (0x8000) и otadata (0xE000)** — проверено
на железе 2026-09-26, `upload_offset_address` pioarduino игнорирует.
Санкционированная процедура — прямой esptool; после прошивки сверяй вывод:
ровно одна запись `Wrote ... at 0x00010000`:

```bat
esptool --chip esp32c3 --port COMx --baud 921600 --before default_reset --after hard_reset write-flash 0x10000 .pio\build\esp32c3\firmware.bin
```

Монитор: `pio device monitor -p COMx -b 115200`.

**Совместимость с флешерами CrossPoint-семейства:** таблица разделов
LingoInk не содержит app1 (ota_1), поэтому веб-флешеры CrossPoint /
CrossInk / InkPointX поверх LingoInk не ставятся. Сначала верните сток
(`write-flash 0 <полный бэкап>`), затем пользуйтесь любым штатным
флешером. Проверено на железе 2026-09-26: сток → CrossInk v1.6.0 и
InkPointX v2.3.6 ставятся штатно. План полной совместимости —
`docs/ROADMAP.md`.

### Backup перед первой прошивкой (обязательно)

```bat
esptool --chip esp32c3 --port COMx --baud 921600 read-flash 0 0x1000000 lingoink-x4-backup-full.bin
```

Восстановление стока: `write-flash 0 lingoink-x4-backup-full.bin`.

Если после прошивки грузится старая прошивка (стоял CrossPoint, otadata
указывает на app1) — затереть otadata файлом 0xFF размером 0x2000 по адресу
0xE000 (после снятого backup). Подробности и предупреждения об USB-locked
партиях AliExpress: `docs/TECHNICAL_PLAN.md` § Risks.

## Структура SD-карты

Карта FAT32, вставляется **до включения** (монтирование при старте):

```
X:\courses\english_ru\course.json          # манифест курса
X:\courses\english_ru\lessons\*.json       # уроки
X:\lingoink\progress.json                  # создаёт прошивка после первого урока
```

Быстрый старт: скопировать папку `data/sd/english_ru` в `courses` в корне
карты. Формат — `docs/COURSE_FORMAT.md`.

## Запуск

Включение кнопкой Power → splash → Home (CONTINUE / PROGRESS / SETTINGS).
Ориентация (landscape/portrait), очистка экрана и сброс прогресса — в
SETTINGS. Управление кнопками — `docs/UX.md`. Прогресс сохраняется в конце
урока и при выключении; resume подхватывает следующий урок.

## Лицензии

Собственный код — proprietary (© авторы LingoInk). Вендоренные компоненты:
freeink-sdk (MIT), ArduinoJson (MIT), шрифты Inter (OFL 1.1). Полная
ведомость — `LICENSE-NOTES.md`.

## Обновление обучения (2026-09-26)

Рабочие повторы, ручная календарная дата, 42 задания на самостоятельное вспоминание,
возврат ошибок, чтение без обрезки и сохранение места. Описание и проверки:
[docs/LEARNING_UPDATE.md](docs/LEARNING_UPDATE.md).
Для нового контента обновите **и прошивку, и папку курса** `data/sd/english_ru` на SD.
Папку `/lingoink` с личным прогрессом сохраняйте. До замены файлов выключите устройство.
