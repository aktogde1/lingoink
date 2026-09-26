# LingoInk — лицензионная ведомость третьих сторон

Дата: 2026-09-26. Проект потенциально коммерческий — ведомость обновлять
при каждом изменении зависимостей.

## Собственный код

`src/**` (кроме сгенерированных шрифтов), `tools/make_font.py`,
`docs/**`, `data/**` — © LingoInk authors. Все права защищены
(proprietary; при публикации OSS — сменить этот файл и заголовки).

## Вендоренные компоненты

| Компонент | Путь | Лицензия | Файл лицензии | Обязательства |
|---|---|---|---|---|
| freeink-sdk (Free-Ink) | `lib/{BoardConfig,FreeInkDisplay,InputManager,BatteryMonitor,PowerManager,XteinkDetect}` | MIT | `lib/freeink-sdk-LICENSE` | Сохранить уведомление о авторских правах в распространяемых формах (исходники и binary distribution notes) |
| ArduinoJson (Benoit Blanchon) | ставится PlatformIO (`lib_deps`) | MIT | в пакете | Attribution: упомянуть в NOTICE при релизе |
| arduino-esp32 (Espressif) | платформа pioarduino | LGPL-2.1 | в платформе | LGPL: исходники библиотеки публичны; obligations касаются самой библиотеки, app-код не раскрывается. Альтернатива без LGPL — чистый ESP-IDF (Apache-2.0) |
| pioarduino platform + ESP-IDF | toolchain | Apache-2.0 | в платформе | NOTICE |
| Inter Regular/SemiBold (Rasmus Andersson) | субсеты в `src/ui/fonts/*.h` (сгенерированы `tools/make_font.py`) | SIL OFL 1.1 | см. ниже | Встраивание субсетов разрешено; attribution в документации; не продавать шрифт отдельно; производные не называть «Inter» |

## SIL Open Font License 1.1 — Inter

Copyright (c) Rasmus Andersson (https://rsms.me/inter/).

Шрифты встроены как растровые субсеты (Latin + Cyrillic + пунктуация),
сгенерированные инструментом проекта. Полный текст OFL:
https://openfontlicense.org/open-font-license-official-text/

## Осознанно НЕ используем (copyleft / конфликт)

| Компонент | Лицензия | Причина отказа |
|---|---|---|
| `OrderedDither.h` из eenk (источник bb_epaper, Larry Bank) | GPL-3.0-or-later | Дизеринг не нужен (ч/б UI); GPL-файл не копируется |
| epdiy (vroland) — код | LGPL-3.0 | Структуры формата шрифтов написаны заново (`LgFont.h`); код epdiy не копируется |
| FreeInkBook, FreeInkUI, SecureNet (freeink-sdk) | MIT | Допустимы, но не нужны MVP; не включены в сборку |
| eenk / crosspoint-reader / papyrix-reader код | MIT | Только как референс; код не переносился |

## Проверка

- `grep -ri "GPL" lib/ src/` — только в комментариях-отсылках, GPL-кода нет.
- Все файлы freeink-sdk сохранены без модификаций (vendored as-is).
