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

```bash
pio run -e esp32c3              # сборка
pio run -e esp32c3 -t upload    # прошивка (USB-C)
pio device monitor              # логи 115200
```

Прошивка занимает только слот **app0 @ 0x10000** (bootloader и NVS не
трогаются), поэтому её можно ставить поверх стоковой прошивки или
CrossPoint и так же возвращать их обратно:

```bash
esptool.py --chip esp32c3 --port <PORT> --baud 921600 \
    write_flash 0x10000 .pio/build/esp32c3/firmware.bin
```

⚠️ Устройства с AliExpress могут быть USB-locked — см. раздел Risks в
`docs/TECHNICAL_PLAN.md`.

## Подготовка SD-карты

Скопируйте `data/sd/english_ru/` на карту как `/courses/english_ru/`:

```
/courses/english_ru/course.json
/courses/english_ru/lessons/*.json
```

Прогресс появится в `/lingoink/progress.json` после первого урока.

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
