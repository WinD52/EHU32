# EHU32

ESP32-based Bluetooth audio integration for Opel/Vauxhall vehicles with CAN bus display support.

Основанная на ESP32, Bluetooth интеграция для автомобилей Opel/Vauxhall с поддержкой CAN вывода информации на дисплей БК.

> ⚠️ **IMPORTANT: Before compiling, set your Arduino IDE partition scheme to "Minimal SPIFFS"!** Without this, you will get a **"Sketch too big"** compilation error. See Compilation Notes for full details.
> ⚠️ **ВАЖНО: Перед компиляцией установите в Arduino IDE схему разделов "Minimal SPIFFS"!** Иначе вы получите ошибку **"Sketch too big"**. Подробнее см. в разделе "Примечания по компиляции".

---

## ⚠️ This fork modifications (v3)

This version adds two quality-of-life features to the original EHU32 project:

1. **Cyrillic transliteration:** Song tags in Russian are automatically transliterated to Latin characters, solving the issue of empty text on the display.
2. **Head unit track control:** You can switch tracks by **long-pressing (≈400 ms)** the **`<`** and **`>`** buttons on the car's radio panel. This works only in AUX mode and does not conflict with the standard button functions.

## ⚠️ Модификации этой версии (v3)

Данная версия добавляет две функции к оригинальному проекту EHU32:

1. **Транслитерация кириллицы:** Русские теги треков автоматически переводятся в латиницу, что решает проблему пустого текста на дисплее.
2. **Управление с магнитолы:** Переключение треков выполняется **долгим нажатием (≈400 мс)** на кнопки **`<`** и **`>`** на панели магнитолы. Работает только в режиме AUX и не конфликтует со штатными функциями кнопок.

---

## Table of Contents / Содержание

1. Features / Возможности
2. Compatibility / Совместимость
3. Quick Start / Быстрый старт
4. Demo / Демонстрация
5. Hardware — Building it yourself / Аппаратное обеспечение
6. How it works and usage tips / Принцип работы и советы
7. Installation Options / Варианты установки
8. Compilation Notes / Примечания по компиляции
9. OTA Updates / OTA обновления
10. Documentation / Документация
11. Credits / Благодарности
12. Disclaimer / Отказ от ответственности

---

## Features / Возможности

### Bluetooth Audio / Bluetooth Аудио

- **Bluetooth (A2DP) audio** — output to an external I2S DAC (PCM5102A)
- **Automatically reconnects** to your phone when the radio is started
- **Bluetooth (A2DP) аудио** — вывод на внешний I2S DAC (PCM5102A)
- **Автоматическое переподключение** к вашему телефону при включении магнитолы

### Display Integration / Интеграция с дисплеем

- Shows currently playing track on the center console display in **Aux mode**
  - Prints **Artist**, **Track title** and **Album**, just like regular CD playback
  - Default mode; also accessible by long-pressing **"1"**
- Отображает текущий трек на дисплее в **режиме AUX**
  - Выводит **Исполнителя**, **Название трека** и **Альбом**, как при воспроизведении CD
  - Режим по умолчанию; также доступен по долгому нажатию **"1"**

### Vehicle Controls / Управление автомобилем

- **Steering wheel buttons** — play/pause, previous/next track
- **AC macro** — toggle AC with a single long-press of the AC selector knob (held ≥ 0.5 s)
- **Кнопки на руле** — play/pause, предыдущий/следующий трек
- **Макрос AC** — включение/выключение кондиционера долгим нажатием ручки выбора режима AC (≥ 0.5 с)

### Diagnostic Data Display / Отображение диагностических данных

- Live vehicle data: speed, RPMs, coolant temperature, battery voltage 
  - Accessible by long-pressing **"2"** on the radio panel
  - For single-line displays: long-press **"3"** for coolant temperature only
  - Disable screen output entirely by holding **"9"** (hold 5 s to reset and clear all settings)
- Живые данные автомобиля: скорость, обороты, температура охлаждающей жидкости, напряжение бортовой сети
  - Доступно по долгому нажатию **"2"** на панели магнитолы
  - Для однострочных дисплеев: долгое нажатие **"3"** показывает только температуру
  - Полное отключение вывода на экран — удержание **"9"** (удержание 5 секунд сбрасывает все настройки)

### OTA Updates / OTA обновления

- Update firmware over-the-air — hold **"8"** to enable the Wi-Fi hotspot; see OTA Updates
- Обновление прошивки "по воздуху" — удерживайте **"8"** для включения Wi-Fi точки доступа; см. OTA Updates

---

## Compatibility / Совместимость

| Component / Компонент | Supported Models / Поддерживаемые модели |
|-----------------------|------------------------------------------|
| **Vehicles / Автомобили** | Astra H, Corsa D, Vectra C, Zafira B, Meriva A, Signum |
| **Radios / Магнитолы** | CD30, CD30MP3, CD40USB, CDC40Opera, CD70Navi, DVD90Navi |
| **Displays / Дисплеи** | CID (3-line), GID (1-line), GID (3-line), BID, TID |

> **Requirement:** Your radio must have an **Aux input**.
> **Требование:** Ваша магнитола должна иметь **вход AUX**.

---

## ⚠️ Tested Configuration / Протестированная конфигурация

This fork has been **personally tested and is confirmed working** on the following setup:
Данная прошивка **лично протестирована и работает** на следующей конфигурации:

| Component / Компонент | Model / Модель |
|-----------------------|----------------|
| **Vehicle / Автомобиль** | Opel Astra H |
| **Radio / Магнитола** | CD30 (or CD30MP3) |
| **Display / Дисплей** | GID (Graphic Info Display) |

### What works / Что работает:

- ✅ Full Bluetooth audio streaming (A2DP) / Полноценная Bluetooth аудиотрансляция (A2DP)
- ✅ Automatic reconnection / Автоматическое переподключение
- ✅ Track info display (Artist, Title, Album) on the car's screen / Отображение информации о треке (Исполнитель, Название, Альбом) на экране автомобиля
- ✅ **Cyrillic transliteration** (Russian tags are converted to Latin) / **Транслитерация кириллицы** (русские теги преобразуются в латиницу)
- ✅ **Head unit buttons** (`<` and `>` arrows) — long press (~400 ms) to switch tracks / **Кнопки на магнитоле** (`<` и `>` стрелки) — долгое нажатие (~400 мс) для переключения треков
- ✅ Steering wheel controls (next/previous, play/pause) / Кнопки на руле (следующий/предыдущий, play/pause)
- ✅ OTA updates (button "8") / OTA обновления (кнопка "8")
- ✅ Полноценная Bluetooth аудиотрансляция (A2DP)

### Not tested (but should work) / Не протестировано (но должно работать):

The core functionality of EHU32 is unchanged, so this firmware **should** work on all vehicles, radios, and displays listed in the compatibility table above. However, the two new features (transliteration and head unit buttons) have only been tested on the configuration above.

Базовая функциональность EHU32 не изменена, поэтому данная прошивка **должна** работать на всех автомобилях, магнитолах и дисплеях, перечисленных в таблице совместимости выше. Однако две новые функции (транслитерация и кнопки на магнитоле) тестировались только на указанной выше конфигурации.

---

## Quick Start / Быстрый старт

1. **Get the components** — see the Hardware section for the bill of materials
2. **Wire them up** — follow EHU32_wiring.pdf
3. **Set up Arduino IDE** — install required libraries and the ESP32 core (see Compilation Notes)
4. ⚠️ **Set partition scheme to "Minimal SPIFFS"** — _without this you will get a "Sketch too big" error!_
5. **Flash** the firmware to your ESP32
6. **Connect CAN bus and AUX cable** to the OBD-II port (pins 3/11) and the radio's Aux input
7. **Pair your phone** — EHU32 appears as a Bluetooth device once the radio is on
1. **Получите компоненты** — см. раздел "Аппаратное обеспечение"
2. **Подключите всё** — следуйте EHU32_wiring.pdf
3. **Настройте Arduino IDE** — установите необходимые библиотеки и ядро ESP32 (см. "Примечания по компиляции")
4. ⚠️ **Установите схему разделов "Minimal SPIFFS"** — иначе получите ошибку "Sketch too big"!
5. **Загрузите** прошивку в ESP32
6. **Подключите CAN шину и AUX кабель** к OBD-II разъёму (пины 3/11) и AUX входу магнитолы
7. **Подключите телефон** — EHU32 появится как Bluetooth устройство после включения магнитолы

> 💡 **First start:** The initial boot (or hard reset) takes 30–40 seconds while EHU32 probes your vehicle's display and modules. Turn on the headunit (ignition not required) and wait for the startup message before touching anything.
> 💡 **Первый запуск:** Первоначальная загрузка (или сброс) занимает 30–40 секунд, пока EHU32 тестирует дисплей и модули автомобиля. **Включите магнитолу (зажигание не обязательно)** и ждите появления приветственного сообщения, ничего не нажимая.

---

## Demo / Демонстрация

📺 Click to show demo videos and screenshots
📺 Нажмите, чтобы показать демо-видео и скриншоты

Extended demo / Расширенное демо: https://www.youtube.com/watch?v=8fi7kX9ci_o

Live diagnostic data demo (warning, contains music!) / Живые диагностические данные (осторожно, содержит музыку!): https://www.youtube.com/watch?v=uxLYr1c_TJA

---

## Hardware — Building it yourself / Аппаратное обеспечение

### Bill of Materials / Необходимые компоненты

- **ESP32 module** — preferably an official Espressif module with an IPX antenna connector. **Look for boards with "Espressif" etched on the RF shield** (boards without this have been found to cause I2S audio issues, especially with iPhones and Huawei phones)
- **IPX antenna** — any will do; recovered from an old laptop or bought cheaply online
- **PCM5102A DAC module** — with configurable jumpers on the bottom; **configure the jumpers correctly before use**
- **CAN transceiver module** — see table below
- **ESP32 модуль** — желательно официальный модуль Espressif с разъёмом для IPX антенны. **Ищите платы с гравировкой "Espressif" на экране RF shield**
- **IPX антенна** — подойдёт любая; можно восстановить из старого ноутбука или купить недорого
- **PCM5102A DAC модуль** — с настраиваемыми перемычками на нижней стороне; **настройте перемычки правильно перед использованием**
- **CAN трансивер** — см. таблицу ниже

### CAN Transceiver Options / Варианты CAN трансивера

| Module | VCC | Notes / Примечания |
|--------|-----|--------------------|
| MCP2551 | 5V | Most common, widely available / Наиболее распространённый |
| TDA104x / TDA1050 | 5V | |
| SN65HVD23x | 3.3V | Lower voltage option / Вариант с низким напряжением |

### Pin Assignment / Назначение пинов

| ESP32 Pin | Function / Функция | Connected To / Подключение к |
|-----------|--------------------|------------------------------|
| GPIO 4 | CAN RX | CAN Transceiver RXD |
| GPIO 5 | CAN TX | CAN Transceiver TXD |
| GPIO 22 | I2S Data | PCM5102A DIN |
| GPIO 25 | I2S Word Select | PCM5102A LCK |
| GPIO 26 | I2S Bit Clock | PCM5102A BCK |
| GPIO 23 | PCM Mute Control | PCM5102A XSMT |
| GPIO 27 | PCM Power Enable | Power switch (enables PCM5102A VDD and wakes CAN transceiver from standby) |

Refer to EHU32_wiring.pdf for the full wiring diagram.
Смотрите EHU32_wiring.pdf для полной схемы подключения.

### CAN Bus Access / Доступ к CAN шине

The MS-CAN bus is accessible via:
MS-CAN шина доступна через:

- **OBD-II diagnostic port** — pins 3 (CAN-H) and 11 (CAN-L)
- The headunit, display, climate control panel, or factory Bluetooth hands-free module
- **Диагностический разъём OBD-II** — пины 3 (CAN-H) и 11 (CAN-L)
- Магнитолу, дисплей, панель климат-контроля или штатный Bluetooth модуль

### Power Notes / Примечания по питанию

- The OBD-II port provides **unswitched 12V only** — if installed externally, power from a switched 5V USB car charger
- If soldering inside the headunit, use the radio's switched power supply
- **Do not connect headphones** to the DAC output — it is line-level only (connect to the radio's AUX input)
- Разъём OBD-II выдаёт **только невыключаемые 12В** — при внешней установке питайте от прикуривателя через USB зарядку 5В
- При пайке внутри магнитолы используйте её выключаемое питание
- **Не подключайте наушники** к выходу DAC — это линейный выход (подключайте только к AUX входу магнитолы)

---

## How it works and usage tips / Принцип работы и советы

- **First start / hard reset** takes 30–40 seconds — EHU32 probes your vehicle's display and modules for compatibility. **Turn on your headunit and wait** for the startup message before doing anything else.
- **Bluetooth activates only** after EHU32 detects the radio communicating with the display over CAN bus.
- **Set audio source volume to maximum** on your phone to minimise noise floor. Adjust listening volume using the radio's knob or steering wheel buttons. Reduce phone volume only if you hear obvious clipping/distortion.
- **Aux mode detection:** EHU32 scans for "Aux" in CAN messages. After switching away from Aux, there may be a delay before the display updates to FM/CD mode.
- **CD30/CD40 users:** Press **"SOUND" twice** to access bass/treble/balance settings. EHU32 must block messages before it knows their content, so the first press is consumed — the menu appears on the second press.
- **Android audio issues (skipping/crackling):** In Bluetooth settings, select EHU32 from the device list and disable **"Keep volume consistent"**.
- **Первый запуск / сброс** занимает 30–40 секунд — EHU32 тестирует дисплей и модули автомобиля. **Включите магнитолу и ждите** приветственного сообщения.
- **Bluetooth включается только после** того, как EHU32 обнаружит общение магнитолы с дисплеем по CAN шине.
- **Установите громкость на телефоне на максимум** для минимизации шумов. Регулируйте громкость ручкой магнитолы или кнопками на руле.
- **Определение режима AUX:** EHU32 сканирует сообщения CAN в поиске "Aux". После выхода из AUX может быть задержка перед обновлением экрана.
- **Для CD30/CD40:** Нажмите **"SOUND" дважды** для доступа к настройкам баса/высоких/баланса. Первое нажатие "съедается" EHU32, меню появляется со второго.
- **Проблемы с Android (пропуски, треск):** В настройках Bluetooth выберите EHU32 и отключите **"Keep volume consistent"**.

For CAN message details and vehicle-specific reverse engineering notes, see the wiki and the `docs/` folder.
Подробности о сообщениях CAN и обратном инжиниринге см. в wiki и папке `docs/`.

---

## Installation Options / Варианты установки

### External (simplest — no disassembly) / Внешняя (проще — без разбора)

- ESP32 + modules connected to the **OBD-II port** (CAN bus) and **Aux input** of the radio
- Power from a switched 5V USB car charger plugged into the cigarette lighter
- Everything stays outside the dashboard
- ESP32 + модули подключаются к **OBD-II разъёму** (CAN шина) и **AUX входу** магнитолы
- Питание от USB зарядки 5В, включённой в прикуриватель
- Всё остаётся снаружи торпедо

### Internal (cleanest — inside the headunit) / Внутренняя (аккуратнее — внутри магнитолы)

Installing inside the headunit gives a factory-clean result with no visible cables.
Установка внутри магнитолы даёт заводской вид без видимых проводов.

| Radio / Магнитола | Guide / Руководство |
|-------|-------|
| **CD30MP3** (Delphi-Grundig) | Issue #3 comment |
| **CD70Navi** | Wiki: Hardware modification |
| **Other radios / Другие магнитолы** | See `docs/` folder / См. папку `docs/` |

---

## Compilation Notes / Примечания по компиляции

### Required Library Versions / Требуемые версии библиотек

| Component / Компонент | Version / Версия |
|-----------|---------|
| ESP32 Arduino Core | **2.0.17** (newer versions not stable enough / новые версии недостаточно стабильны) |
| ESP32-A2DP | 1.8.7 |
| arduino-audio-tools | 1.1.1 |

### Arduino IDE Settings / Настройки Arduino IDE

1. Open **Tools** menu / Откройте меню **Инструменты**
2. Set **Events** → Core 0 / Установите **Events** → Core 0
3. Set **Arduino** → Core 1 / Установите **Arduino** → Core 1
4. Set **Partition Scheme** → **Minimal SPIFFS** ← ⚠️ _see warning below_ / Установите **Partition Scheme** → **Minimal SPIFFS** ← ⚠️ _см. предупреждение ниже_

> ### ⚠️ CRITICAL: Partition Scheme Must Be "Minimal SPIFFS"
> 
> If you see a **"Sketch too big"** compilation error, you have not set the partition scheme correctly.
> 
> **Fix:** In Arduino IDE → Tools → Partition Scheme → select **"Minimal SPIFFS (1.9MB APP with OTA / 190KB SPIFFS)"**
> 
> This is the single most common setup problem. The sketch is too large for the default partition layout.
> 
> ### ⚠️ КРИТИЧНО: Схема разделов должна быть "Minimal SPIFFS"
> 
> Если вы видите ошибку компиляции **"Sketch too big"**, значит схема разделов установлена неверно.
> 
> **Исправление:** В Arduino IDE → Инструменты → Partition Scheme → выберите **"Minimal SPIFFS (1.9MB APP with OTA / 190KB SPIFFS)"**
> 
> Это самая частая проблема при настройке. Скетч слишком велик для стандартной схемы разделов.

### sdkconfig TWAI Modification / Изменение sdkconfig для TWAI

The ESP-IDF TWAI (CAN) driver requires a one-time config change to work reliably. Edit **`sdkconfig`** located at:
Драйвер ESP-IDF TWAI (CAN) требует однократного изменения конфигурации для стабильной работы. Отредактируйте **`sdkconfig`** по пути:


```
%USERPROFILE%\AppData\Local\Arduino15\packages\esp32\hardware\esp32\<version>\tools\sdk\esp32\
```

Find the `# TWAI configuration` section and make it match exactly:

```
#
# TWAI configuration
#
CONFIG_TWAI_ISR_IN_IRAM=y
CONFIG_TWAI_ERRATA_FIX_BUS_OFF_REC=y
CONFIG_TWAI_ERRATA_FIX_TX_INTR_LOST=n
CONFIG_TWAI_ERRATA_FIX_RX_FRAME_INVALID=y
CONFIG_TWAI_ERRATA_FIX_RX_FIFO_CORRUPT=y
# CONFIG_TWAI_ERRATA_FIX_LISTEN_ONLY_DOM is not set
# end of TWAI configuration
```

Changes: `CONFIG_TWAI_ISR_IN_IRAM` must be **enabled** (`y`) and `CONFIG_TWAI_ERRATA_FIX_TX_INTR_LOST` must be **disabled** (`n`).
Изменения: `CONFIG_TWAI_ISR_IN_IRAM` должна быть **включена** (`y`), а `CONFIG_TWAI_ERRATA_FIX_TX_INTR_LOST` должна быть **отключена** (`n`).

### Common Compilation Errors / Частые ошибки компиляции

| Error / Ошибка | Cause / Причина | Fix / Исправление |
|-------|-------|-----|
| `Sketch too big` | Wrong partition scheme / Неверная схема разделов | Set **Partition Scheme → Minimal SPIFFS** |
| Missing library / Отсутствует библиотека | ESP32-A2DP or audio-tools not installed / ESP32-A2DP или audio-tools не установлены | Install via Arduino Library Manager / Установите через менеджер библиотек |
| Version mismatch / Несоответствие версий | Wrong ESP32 core version / Неверная версия ядра ESP32 | Downgrade to core **2.0.17** / Откатитесь до версии **2.0.17** |

For more details, see `docs/compilation-guide.md`.
Подробнее см. `docs/compilation-guide.md`.

---

## OTA Updates / OTA обновления

EHU32 supports over-the-air firmware updates:
EHU32 поддерживает обновление прошивки "по воздуху":

1. Hold radio button **"8"** for ≥ 1 second — EHU32 starts a Wi-Fi hotspot
2. Connect your computer to the **EHU32-OTA** network (see `OTA.ino` for credentials)
3. Upload the new firmware via the Arduino IDE network port
4. To exit OTA mode without updating, hold **"8"** for 5 seconds — the board restarts
1. Удерживайте кнопку **"8"** на магнитоле ≥ 1 секунды — EHU32 запускает Wi-Fi точку доступа
2. Подключите компьютер к сети **EHU32-OTA** (учётные данные см. в `OTA.ino`)
3. Загрузите новую прошивку через сетевой порт Arduino IDE
4. Для выхода из режима OTA без обновления удерживайте **"8"** 5 секунд — плата перезагрузится

---

## Documentation / Документация

| Resource / Ресурс | Description / Описание |
|----------|-------------|
| `docs/` | Full hardware installation guides, CAN bus details, troubleshooting / Полные руководства по установке, детали CAN шины, устранение неполадок |
| `docs/hardware-overview.md` | Component details and pin reference / Детали компонентов и назначение пинов |
| `docs/compilation-guide.md` | Detailed build setup instructions / Подробные инструкции по настройке компиляции |
| `docs/troubleshooting.md` | Common problems and solutions / Частые проблемы и их решения |
| `docs/faq.md` | Frequently asked questions / Часто задаваемые вопросы |
| `EHU32_wiring.pdf` | Full wiring diagram / Полная схема подключения |
| Wiki | CAN message reference, vehicle-specific notes / Справочник по сообщениям CAN, примечания по конкретным автомобилям |
| `CONTRIBUTING.md` | How to contribute to the project / Как внести вклад в проект |

---

## Credits / Благодарности

- **PNKP237** — original author of the EHU32 project  
  Оригинальный автор проекта EHU32  
  https://github.com/PNKP237/EHU32

- **BxnnyG** — fork with detailed code comments and documentation improvements  
  Форк с подробными комментариями кода и улучшенной документацией  
  https://github.com/BxnnyG/EHU32

- **ESP32-A2DP** and **arduino-audio-tools** by pschatzmann — Bluetooth A2DP and I2S audio libraries  
  Библиотеки Bluetooth A2DP и I2S аудио

- **Car-CAN-Message-DB** by JJToB — reverse engineering of Opel/Vauxhall CAN bus messages  
  Обратный инжиниринг сообщений CAN шины Opel/Vauxhall

---

## Disclaimer / Отказ от ответственности

This project comes with **absolutely no warranty of any kind**. I am not responsible for any damage to your vehicle, electronics, or anything else. Use at your own risk.
Этот проект предоставляется **без каких-либо гарантий**. Я не несу ответственности за любой ущерб вашему автомобилю, электронике или чему-либо ещё. Используйте на свой страх и риск.

---

## About / О проекте

ESP32-based bluetooth audio integration for Opel/Vauxhall vehicles. Control bluetooth audio source with steering wheel buttons, show coolant temperature and battery voltage on the integrated display and more!
ESP32-based Bluetooth интеграция для автомобилей Opel/Vauxhall. Управление Bluetooth аудио с кнопок на руле, отображение температуры охлаждающей жидкости и напряжения бортовой сети на штатном дисплее и многое другое!

This project comes with **absolutely no warranty of any kind**. I am not responsible for any damage to your vehicle, electronics, or anything else. Use at your own risk.
