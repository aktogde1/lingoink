# LingoInk

**One device. One purpose. Learn a language.**

LingoInk превращает XTEINK X4 (ESP32-C3, 4.3" e-ink 800×480) в автономный
языковой тренажёр: без соцсетей, браузера, уведомлений и библиотеки книг.
Короткие уроки, тесты, чтение, грамматика и интервальное повторение —
полностью офлайн, контент на SD-карте.

Это отдельное приложение, а не форк читалки: reader-функционал не
переносится; из open-source экосистемы X4 берётся только hardware-слой
[freeink-sdk](https://github.com/Free-Ink/freeink-sdk) (MIT).

## Сборка и прошивка

Требуется PlatformIO (Python 3.10–3.13 — ограничение платформы pioarduino).
**esptool не работает из Git Bash (MSys) — выполняйте прошивку из cmd или
PowerShell.**

```bash
pio run -e esp32c3              # сборка
pio run -e esp32c3 -t upload    # прошивка (USB-C)
pio device monitor              # логи 115200
```

### Что именно записывает прошивка (safety)

Проверено по коду платформы pioarduino 55.03.37 (Arduino framework не задаёт
`FLASH_EXTRA_IMAGES`): `pio upload` и ручная команда ниже пишут **только
слот app0 @ 0x10000**. Не затрагиваются: bootloader @ 0x0, partition table @
0x8000, **NVS @ 0x9000 (там калибровка панели)**, otadata @ 0xE000, вся
остальная flash. `erase_flash` не используется.

Ручная прошивка тем же единственным offset:

```bat
esptool --chip esp32c3 --port COMx --baud 921600 ^
    write-flash 0x10000 .pio\build\esp32c3\firmware.bin
```

⚠️ Устройства с AliExpress могут быть USB-locked — см. раздел Risks в
`docs/TECHNICAL_PLAN.md`. Если на устройстве стоял CrossPoint и оно
загружалось из слота app1, после прошивки LingoInk в app0 старая прошивка
может продолжить грузиться (otadata указывает на app1) — см. «otadata» в
`docs/HARDWARE_VALIDATION.md`.

### Backup перед первой прошивкой (обязательно)

Из cmd/PowerShell (esptool из venv: `%USERPROFILE%\lingoink-venv\Scripts\`):

```bat
:: полный образ flash (16 MB) — ~3–5 минут, полная обратимость
esptool --chip esp32c3 --port COMx --baud 921600 read-flash 0 0x1000000 lingoink-x4-backup-full.bin

:: отдельно критические разделы
esptool --chip esp32c3 --port COMx read-flash 0x0    0x10000 lingoink-bootloader-region.bin
esptool --chip esp32c3 --port COMx read-flash 0x8000 0x1000  lingoink-partitions.bin
esptool --chip esp32c3 --port COMx read-flash 0x9000 0x5000  lingoink-nvs.bin
esptool --chip esp32c3 --port COMx read-flash 0xE000 0x2000  lingoink-otadata.bin
```

Восстановление любой предыдущей прошивки — запись полного бэкапа назад
(или web flasher CrossPoint / стока, они пишут полный образ с 0x0):

```bat
esptool --chip esp32c3 --port COMx --baud 921600 write-flash 0 lingoink-x4-backup-full.bin
```

### otadata (только если после прошивки грузится старая прошивка)

Прочитать и посмотреть: все `FF` в первых секторах — ок, LingoInk загрузится.
Если там валидная запись об app1 (стоял CrossPoint) — затереть otadata
(backup уже снят; write-flash стирает только записываемые секторы):

```bat
:: подготовить файл 0xFF размером 0x2000 и записать его в otadata
python -c "open('blank_otadata.bin','wb').write(b'\xFF'*0x2000)"
esptool --chip esp32c3 --port COMx write-flash 0xE000 blank_otadata.bin
```

## Подготовка SD-карты

Карта FAT32. Из `data/sd/english_ru/` сделать на карте корневой каталог
`courses` (Windows: карта — это диск `X:`):

```
X:\courses\english_ru\course.json
X:\courses\english_ru\lessons\a2_01_vocab_everyday.json
X:\courses\english_ru\lessons\a2_02_present_perfect.json
X:\courses\english_ru\lessons\a2_03_reading_london.json
X:\courses\english_ru\lessons\a2_04_mixed_review.json
```

То есть: скопировать папку `english_ru` в созданную папку `courses` в
корне карты. Вставить карту **до включения** устройства (монтирование —
при старте). Прогресс появится в `X:\lingoink\progress.json` после первого
урока.

Чек-лист проверки на железе: `docs/HARDWARE_VALIDATION.md`.

## Управление

| Кнопка | Действие |
|---|---|
| Left / Right (или Up / Down) | выбор варианта / навигация |
| OK | подтвердить ответ / далее |
| OK (удержание) | показать объяснение |
| Back | назад |
| Power | выключение (deep sleep) |

## Тесты домена

```bash
pio test -e native    # SRS (SM-2), mastery — нужен host-компилятор
```

## Структура и планы

- `docs/TECHNICAL_PLAN.md` — архитектура, исследование железа, лицензии, roadmap.
- `docs/course-format.md` — спецификация формата курса.
- `LICENSE-NOTES.md` — лицензионная ведомость третьих сторон.

## Лицензии

Собственный код LingoInk — proprietary (© авторы проекта). Вендоренные
компоненты: freeink-sdk (MIT), ArduinoJson (MIT), шрифты Inter (OFL 1.1).
Полная ведомость — `LICENSE-NOTES.md`.
