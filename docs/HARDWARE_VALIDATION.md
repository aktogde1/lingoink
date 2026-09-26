# LingoInk — Hardware Validation Checklist (XTEINK X4)

Первая прошивка на реальное железо. Цель — подтвердить вертикальный MVP,
**не** новые функции. Заполняйте по ходу; всё, что не работает, фиксируйте
в разделе «Findings» внизу с фото/логом.

Условия: устройство разблокировано (куплено на xteink.com или проверено
web-flasher'ом CrossPoint), SD-карта (FAT32) подготовлена по README.

---

## 0. До прошивки

- [ ] Backup полного flash (16 MB) снят и сохранён в надёжное место
- [ ] Backup partition table + otadata снят
- [ ] COM-порт устройства виден в Device Manager
- [ ] Прошивка собрана: `.pio\build\esp32c3\firmware.bin` свежая

## 1. Прошивка

- [ ] `pio run -e esp32c3 -t upload --upload-port COMx` завершился без ошибок
- [ ] В выводе esptool указан **единственный** offset `0x10000` (app0)
- [ ] Устройство перезагрузилось (hard-reset после записи)

## 2. Boot

- [ ] Serial monitor подключён (`pio device monitor -p COMx -b 115200`)
- [ ] В логе: `BOOT boot start`
- [ ] В логе: `HW device: Xteink X4 (SSD1677 controller default)` — X4 распознан
- [ ] В логе: `HW panel controller probe: ...` (default kept ИЛИ UltraChip promoted — оба валидны)
- [ ] На экране: splash «LingoInk» + «Английский · Русский» (кириллица читается)
- [ ] В логе: `APP display+input initialized`
- [ ] Splash сменился на Home без зависания

## 3. SD и курс

- [ ] В логе: `SD SD mounted, type=… size=… MB` (type≠NONE)
- [ ] В логе: `CRS course 'english_ru' [ru->en]: 4 lessons`
- [ ] На Home видны: ENGLISH / A2 → B1 / Today / CONTINUE / REVIEW / PROGRESS

Если `no course found` — проверить `/courses/english_ru/course.json` на карте.

## 4. Кнопки (по одной, Home screen)

- [ ] **Left** — выделение вверх по меню, `[KEY] event 1` в логе
- [ ] **Right** — выделение вниз, `event 2`
- [ ] **Side Up** — тоже вверх, `event 6`
- [ ] **Side Down** — тоже вниз, `event 7`
- [ ] **OK** — вход в выбранный пункт, `event 3`
- [ ] **Back** — из Progress обратно на Home, `event 5`
- [ ] **Power (коротко)** — `event 8`, экран гаснет, deep sleep в логе
- [ ] **Power снова** — устройство просыпается, splash → Home
- [ ] Двойных срабатываний нет (один `event N` на одно нажатие)

Key-коды для сверки с логом:
`1=Left 2=Right 3=Ok 4=OkLong 5=Back 6=Up 7=Down 8=Power`

## 5. Вертикальный MVP (урок a2_01 → a2_02)

- [ ] CONTINUE → урок загружается: `CRS loading lesson` + `parsed: N exercises`
- [ ] Choice-упражнение: русский текст (promptSmall) читается, варианты в столбик
- [ ] Left/Right двигают инверсию выделения (FAST refresh, быстрый)
- [ ] OK на правильном → `EX answer … -> RIGHT`, ✓ у варианта, footer CORRECT
- [ ] OK на неправильном → `-> WRONG`, ✗ у выбранного, ✓ у правильного
- [ ] **Long-press OK** → `KEY long OK: explain shown`, рамка с объяснением
- [ ] Long-press OK снова → объяснение скрыто
- [ ] OK → следующее упражнение (HALF/Balanced refresh)
- [ ] RU→EN перевод: вариант с длинной строкой не вылезает за экран (clipping)
- [ ] Урок a2_02: экран THEORY с заголовком PRESENT PERFECT и списком
- [ ] OK листает теорию; после последнего экрана — первое упражнение
- [ ] Cloze: предложение с `___`, варианты, выбор, фидбек
- [ ] Mistake: «don't → doesn't» читается, стрелка → не превращается в кракозябру
- [ ] Последний ответ → SUMMARY: «N% CORRECT»
- [ ] В логе: `EX lesson 'a2_0X' done: N% correct`
- [ ] В логе: `PRG progress saved (N srs items)`
- [ ] Home вернулся, REVIEW n изменился (n = число due)

## 6. Чтение (a2_03)

- [ ] READING: заголовок A DAY IN LONDON, страница 1/3
- [ ] Side Down / OK — следующая страница (FULL refresh, чистый текст)
- [ ] Side Up — предыдущая страница
- [ ] После последней страницы — вопросы (QUESTION в шапке)
- [ ] Ответы на вопросы работают как choice

## 7. Персистентность и resume

- [ ] Выключение: Power → лог `power down (power button)` + `progress saved`
- [ ] Включение: boot → Home
- [ ] В логе: `PRG progress loaded: day=… srs=N lastLesson='a2_0X'` (N > 0)
- [ ] CONTINUE ведёт на урок **после** последнего пройденного (resume работает)
- [ ] На SD появился файл `/lingoink/progress.json` (проверить на компьютере)
- [ ] Today-минуты не обнулились после перезагрузки

## 8. E-ink качество

- [ ] Ориентация правильная (текст горизонтально, не зеркально, не вверх ногами)
- [ ] Полный экран используется (800×480), поля равномерные
- [ ] Чёрный — чёрный, белый — белый (полярность не инвертирована)
- [ ] FAST refresh при движении выбора — без сильных артефактов
- [ ] После 8+ FAST следует FULL (переключения чистые, ghosting исчезает)
- [ ] Смена экрана (Home→Lesson) — FULL, без «призраков» прошлого экрана
- [ ] Кириллица: «Английский · Русский», «Выберите правильный перевод» — читаемо
- [ ] Латиница: упражнения, футеры — читаемо
- [ ] Перенос длинных строк корректный (нет обрезанных слов на границе)
- [ ] Базовая линия шрифта ровная (строки не «прыгают»)

## 9. Recovery-путь подтверждён

- [ ] (По желанию) Прошивка стока/CrossPoint через их web flasher доступна
- [ ] Полный backup сохранён и его размер = 16 MB

---

## Findings (заполнять по факту)

| # | Что проверял | Результат | Примечание / лог |
|---|---|---|---|
|   |              |           |                  |

## Известные ограничения MVP (ожидаемое поведение, не баги)

- REVIEW пока запускает следующий урок, а не отдельную сессию due-items.
- Дни для SRS считаются по накопленному uptime (нет RTC/NTP) — при редком
  использовании интервалы растут медленнее календарных.
- Если курс пройден до конца, CONTINUE начинает его с первого урока.
