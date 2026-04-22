# EHU32

**ESP32-based Bluetooth audio integration for Opel/Vauxhall vehicles with CAN bus display support.**  
**Bluetooth-интеграция на базе ESP32 для автомобилей Opel/Vauxhall с отображением информации на штатном дисплее через CAN-шину.**

## 📌 Fork Chain / Цепочка форков

| Fork | Author | Contribution |
|------|--------|--------------|
| [EHU32](https://github.com/PNKP237/EHU32) | **PNKP237** | Original project — CAN integration, A2DP, display control |
| [EHU32](https://github.com/BxnnyG/EHU32) | **BxnnyG** | Added detailed code comments throughout — much easier to understand |
| **[EHU32](https://github.com/WinD52/EHU32)** | **WinD52** | Additional features on top of BxnnyG's documented codebase |

**Why fork BxnnyG?**  
PNKP237's original code works perfectly but is sparsely commented. BxnnyG added extensive inline documentation, making modifications significantly easier. This fork builds on that foundation.

**Почему форк BxnnyG?**  
Оригинальный код PNKP237 работает отлично, но почти не содержит комментариев. BxnnyG добавил подробные пояснения в код, что значительно упрощает доработку. Этот форк построен именно на ней.

---

## 📌 This Fork Modifications / Модификации этого форка

**Version: v4.5**

This version adds the following quality-of-life features to BxnnyG's documented codebase:
Данная версия добавляет следующие улучшения в документированную кодовую базу BxnnyG:

| Feature / Функция | Description / Описание |
|-------------------|------------------------|
| **Cyrillic transliteration** | Russian song tags are automatically converted to Latin characters — no more empty lines on the display. |
| **Транслитерация кириллицы** | Русские теги треков автоматически переводятся в латиницу — пустых строк на экране больше не будет. |
| **Head unit track control** | Long‑press (≈400 ms) the **`<`** and **`>`** buttons on the radio panel to switch tracks. Works only in AUX mode. |
| **Управление треками с магнитолы** | Долгое нажатие (≈400 мс) на кнопки **`<`** и **`>`** на панели магнитолы переключает треки. Работает только в режиме AUX. |
| **OK button display cycling** | Long‑press the **left scroll wheel centre button (OK)** to cycle between **audio metadata** and **vehicle data** screens. Fires on hold (≥400 ms), not on release — more responsive. |
| **Кнопка OK на руле** | Долгое нажатие на **центр левой крутилки (OK)** переключает экраны: **информация о треке** ↔ **данные автомобиля**. Срабатывает по удержанию (≥400 мс), а не по отпусканию — более отзывчиво. |
| **Simplified display modes** | The single‑line coolant mode (mode 2) has been removed from the rotation — only the two most useful screens remain. |
| **Упрощённые режимы экрана** | Режим с одной строкой температуры (режим 2) исключён из цикла — остались только два основных экрана. |

All original functionality (Bluetooth A2DP, steering wheel controls, OTA updates, diagnostic data) is fully preserved.
Вся оригинальная функциональность (Bluetooth A2DP, кнопки на руле, OTA‑обновления, диагностические данные) полностью сохранена.

---

## 📖 Table of Contents / Содержание

1. [Features / Возможности](#features--возможности)
2. [Compatibility / Совместимость](#compatibility--совместимость)
3. [Quick Start / Быстрый старт](#quick-start--быстрый-старт)
4. [Hardware / Аппаратное обеспечение](#hardware--аппаратное-обеспечение)
5. [How it Works / Принцип работы](#how-it-works--принцип-работы)
6. [Installation Options / Варианты установки](#installation-options--варианты-установки)
7. [Flashing the Firmware / Прошивка устройства](#flashing-the-firmware--прошивка-устройства)
8. [Compilation Notes / Примечания по компиляции](#compilation-notes--примечания-по-компиляции)
9. [OTA Updates / OTA обновления](#ota-updates--ota-обновления)
10. [Documentation / Документация](#documentation--документация)
11. [Credits / Благодарности](#credits--благодарности)
12. [Disclaimer / Отказ от ответственности](#disclaimer--отказ-от-ответственности)

---

## Features / Возможности

### 🎵 Bluetooth Audio / Bluetooth Аудио

- **A2DP audio streaming** via external I2S DAC (PCM5102A).  
  **Потоковое аудио A2DP** через внешний I2S ЦАП (PCM5102A).
- **Automatic reconnection** when the radio powers on.  
  **Автоматическое переподключение** при включении магнитолы.

### 🖥️ Display Integration / Интеграция с дисплеем

- Shows **Artist**, **Track title** and **Album** on the centre console display when AUX is active.  
  Отображает **Исполнителя**, **Название трека** и **Альбом** на дисплее в режиме AUX.
- **Cyrillic transliteration** – Russian tags are converted to Latin automatically.  
  **Транслитерация кириллицы** – русские теги автоматически преобразуются в латиницу.
- **Cycle display modes** with a long press of the **OK** button on the steering wheel (left scroll wheel centre).  
  **Переключение режимов экрана** долгим нажатием кнопки **OK** на руле (центр левой крутилки).

### 🕹️ Vehicle Controls / Управление автомобилем

- **Steering wheel buttons:** Play/Pause, Next, Previous.  
  **Кнопки на руле:** Play/Pause, следующий, предыдущий.
- **Radio panel buttons:** Long‑press **`<`** / **`>`** to switch tracks (AUX only).  
  **Кнопки на магнитоле:** долгое нажатие **`<`** / **`>`** переключает треки (только AUX).
- **AC macro:** Long‑press the AC temperature knob to toggle the air conditioner.  
  **Макрос кондиционера:** долгое нажатие на ручку температуры включает/выключает кондиционер.

### 📊 Diagnostic Data / Диагностические данные

- Live values: **speed**, **RPM**, **coolant temperature**, **battery voltage**.  
  Живые данные: **скорость**, **обороты**, **температура охлаждающей жидкости**, **напряжение бортовой сети**.
- Accessible by long‑pressing **"2"** on the radio panel.  
  Доступно по долгому нажатию кнопки **"2"** на магнитоле.
- Single‑line coolant mode accessible by long‑pressing **"3"**.  
  Режим только с температурой доступен по долгому нажатию кнопки **"3"**.
- **Disable screen updates** by holding **"9"** (hold 5 seconds to factory reset).  
  **Отключение вывода на экран** удержанием кнопки **"9"** (5 секунд – сброс к заводским настройкам).

### 📡 OTA Updates / OTA обновления

- Update firmware wirelessly – hold **"8"** to start the Wi‑Fi hotspot.  
  Обновление прошивки по Wi‑Fi – удерживайте **"8"** для запуска точки доступа.

---

## Compatibility / Совместимость

| Component / Компонент | Supported Models / Поддерживаемые модели |
|-----------------------|------------------------------------------|
| **Vehicles / Автомобили** | Astra H, Corsa D, Vectra C, Zafira B, Meriva A, Signum |
| **Radios / Магнитолы** | CD30, CD30MP3, CD40USB, CDC40Opera, CD70Navi, DVD90Navi |
| **Displays / Дисплеи** | CID (3‑line), GID (3‑line), GID (1‑line), BID, TID |

> **Requirement:** Your radio must have an **AUX input**.  
> **Требование:** Магнитола должна иметь **вход AUX**.

### ✅ Tested Configuration / Протестированная конфигурация

This firmware is confirmed working on:
Данная прошивка проверена на:

| Component / Компонент | Model / Модель |
|-----------------------|----------------|
| Vehicle / Автомобиль | Opel Astra H |
| Radio / Магнитола | CD30 / CD30MP3 |
| Display / Дисплей | GID (Graphic Info Display) |

Other compatible vehicles and radios **should work**, but have not been explicitly tested with this fork.
Другие совместимые автомобили и магнитолы **должны работать**, но не тестировались с данным форком.

---

## Quick Start / Быстрый старт

1. **Gather components** – see [Hardware](#hardware--аппаратное-обеспечение).  
   **Соберите компоненты** – см. раздел [Аппаратное обеспечение](#hardware--аппаратное-обеспечение).
2. **Wire everything** according to `EHU32_wiring.pdf`.  
   **Подключите всё** согласно схеме `EHU32_wiring.pdf`.
3. **Flash the firmware** – see [Flashing the Firmware](#flashing-the-firmware--прошивка-устройства).  
   **Прошейте устройство** – см. [Прошивка устройства](#flashing-the-firmware--прошивка-устройства).
4. **Connect CAN bus** (OBD‑II pins 3 & 11) and **AUX cable**.  
   **Подключите CAN‑шину** (пины 3 и 11 OBD‑II) и **AUX кабель**.
5. **Turn on the radio** – wait for the welcome message on the display (first boot takes 30–40 seconds).  
   **Включите магнитолу** – дождитесь приветственного сообщения на дисплее (первый запуск занимает 30–40 секунд).
6. **Pair your phone** – the device appears as `"Astra H Bluetooth"`.  
   **Подключите телефон** – устройство отображается как `"Astra H Bluetooth"`.

---

## Hardware / Аппаратное обеспечение

### 🧰 Bill of Materials / Список компонентов

| Component / Компонент | Notes / Примечания |
|-----------------------|---------------------|
| **ESP32 module** | Prefer official Espressif modules with IPX antenna connector (look for "Espressif" on the RF shield). |
| **ESP32 модуль** | Желательно официальный модуль Espressif с разъёмом IPX (на экране должно быть выгравировано "Espressif"). |
| **IPX antenna** | Any, even salvaged from an old laptop. |
| **IPX антенна** | Любая, можно из старого ноутбука. |
| **PCM5102A DAC** | Ensure onboard jumpers are configured correctly before use. |
| **PCM5102A ЦАП** | Проверьте положение перемычек на плате перед использованием. |
| **CAN transceiver** | MCP2551 (5V), SN65HVD230 (3.3V) or equivalent. |
| **CAN трансивер** | MCP2551 (5В), SN65HVD230 (3.3В) или аналоги. |

### 🔌 Pinout / Распиновка

| ESP32 Pin | Function / Функция | Connect To / Подключение к |
|-----------|---------------------|-----------------------------|
| GPIO 4 | CAN RX | Transceiver RXD |
| GPIO 5 | CAN TX | Transceiver TXD |
| GPIO 22 | I2S Data | PCM5102A DIN |
| GPIO 25 | I2S WS | PCM5102A LCK |
| GPIO 26 | I2S BCK | PCM5102A BCK |
| GPIO 23 | Mute Control | PCM5102A XSMT |
| GPIO 27 | Power Enable | DAC VDD / CAN transceiver standby |

Full diagram: `EHU32_wiring.pdf`  
Полная схема: `EHU32_wiring.pdf`

### 🔋 CAN Bus Access / Доступ к CAN шине

MS‑CAN is available at **OBD‑II pins 3 (CAN‑H) and 11 (CAN‑L)**.  
MS‑CAN шина доступна на **пинах 3 (CAN‑H) и 11 (CAN‑L) разъёма OBD‑II**.

### ⚡ Power Notes / Примечания по питанию

- OBD‑II provides **unswitched 12V** – use a switched 5V USB adapter for external installations.  
  OBD‑II даёт **неотключаемые 12В** – при внешней установке используйте USB‑зарядку 5В с выключателем.
- If soldering inside the head unit, use its switched power supply.  
  При пайке внутри магнитолы используйте её коммутируемое питание.
- **Do not connect headphones** – the DAC output is line‑level only.  
  **Не подключайте наушники** – выход ЦАП линейный, только для AUX входа магнитолы.

---

## How it Works / Принцип работы

- **First boot / hard reset** takes 30–40 seconds — EHU32 probes your vehicle's display and modules for compatibility. **Turn on your headunit and wait** for the startup message before doing anything else.  
  **Первый запуск / сброс** занимает 30–40 секунд — EHU32 тестирует дисплей и модули автомобиля. **Включите магнитолу и ждите** приветственного сообщения.
- **Bluetooth activates only** after EHU32 detects the radio communicating with the display over CAN bus.  
  **Bluetooth включается только после** того, как EHU32 обнаружит общение магнитолы с дисплеем по CAN шине.
- **Set phone volume to maximum** to minimise noise floor. Adjust listening volume using the radio's knob or steering wheel buttons.  
  **Установите громкость на телефоне на максимум** для минимизации шумов. Регулируйте громкость ручкой магнитолы или кнопками на руле.
- **Aux mode detection:** EHU32 scans for "Aux" in CAN messages. After switching away from Aux, there may be a delay before the display updates.  
  **Определение режима AUX:** EHU32 сканирует сообщения CAN в поиске "Aux". После выхода из AUX может быть задержка перед обновлением экрана.
- **CD30/CD40 users:** Press **"SOUND" twice** to access bass/treble/balance settings. EHU32 must block messages before it knows their content, so the first press is consumed — the menu appears on the second press.  
  **Для CD30/CD40:** Нажмите **"SOUND" дважды** для доступа к настройкам баса/высоких/баланса. Первое нажатие "съедается" EHU32, меню появляется со второго.
- **Android audio issues (skipping/crackling):** In Bluetooth settings, select EHU32 from the device list and disable **"Keep volume consistent"**.  
  **Проблемы с Android (пропуски, треск):** В настройках Bluetooth выберите EHU32 и отключите **"Keep volume consistent"**.

---

## Installation Options / Варианты установки

### 🔌 External (Simplest) / Внешняя (проще)

- ESP32 + modules connected to the **OBD‑II port** (CAN bus) and **Aux input** of the radio.  
  ESP32 + модули подключаются к **разъёму OBD‑II** (CAN шина) и **AUX входу** магнитолы.
- Power from a switched 5V USB car charger plugged into the cigarette lighter.  
  Питание от USB зарядки 5В, включённой в прикуриватель.
- Everything stays outside the dashboard.  
  Всё остаётся снаружи торпедо.

> ⚠️ **РЕКОМЕНДУЮ ДЛЯ ПЕРВОЙ УСТАНОВКИ И НАСТРОЙКИ ВСЁ ПОДКЛЮЧАТЬ ВНЕШНЕ. CAN-L И CAN-H ЭТО 3 И 13 ПИНЫ OBD РАЗЬЁМА (3 ПО СЧЁТУ ЕСЛИ СЧИТАТЬ ПИНЫ ОТ ДВИГАТЕЛЯ)
ПИТАНИЕ ПОДАЁМ ЧЕРЕЗ USB РАЗЬЁМ ESP32. AUX ПОДКЛЮЧАЕМ В ПОРТ AUX.**

### 🧰 Internal (Cleanest) / Внутренняя (аккуратнее)

Installing inside the headunit gives a factory-clean result with no visible cables.  
Установка внутри магнитолы даёт заводской вид без видимых проводов.

| Radio / Магнитола | Guide / Руководство |
|-------------------|----------------------|
| **CD30MP3** (Delphi-Grundig) | Issue #3 comment |
| **CD70Navi** | Wiki: Hardware modification |
| **Other radios** | See `docs/` folder |

---

## Flashing the Firmware / Прошивка устройства

### Option 1: Pre‑compiled Binary (No Compilation) / Вариант 1: Готовый бинарник (без компиляции)

If you just want to use the firmware without setting up Arduino IDE, download the pre‑compiled `.bin` file from [Releases](https://github.com/WinD52/EHU32/releases) and flash it using one of the methods below.

Если вы просто хотите использовать прошивку без настройки Arduino IDE, скачайте готовый `.bin` файл из [Releases](https://github.com/WinD52/EHU32/releases) и прошейте его одним из способов ниже.

#### 📱 Via Android Smartphone + ESPflash (OTG) / Через Android смартфон + ESPflash (OTG)

This is the most convenient method for in‑car updates — no laptop needed.  
Самый удобный способ для обновления прямо в машине — ноутбук не нужен.

1. Install **ESPflash** from Google Play.  
   Установите **ESPflash** из Google Play.
2. Download `EHU32-v4.5.bin` to your phone.  
   Скачайте `EHU32-v4.5.bin` на телефон.
3. Connect ESP32 to the phone via **USB‑OTG adapter**.  
   Подключите ESP32 к телефону через **USB‑OTG переходник**.
4. Open ESPflash, select the `.bin` file.  
   Откройте ESPflash, выберите `.bin` файл.
5. **Important:** Set **Start Address** to `0x10000`.  
   **Важно:** Установите **Start Address** = `0x10000`.
6. Tap **Flash** and wait for completion.  
   Нажмите **Flash** и дождитесь завершения.

> 💡 **First‑time flash?** The bootloader and partition table must already be on the ESP32. If the board is brand new or was fully erased, use Option 2 (Arduino IDE) for the initial flash. Subsequent updates can use this method.  
> 💡 **Первая прошивка?** Загрузчик и таблица разделов уже должны быть на ESP32. Если плата новая или полностью стёрта, используйте Вариант 2 (Arduino IDE) для первой прошивки. Последующие обновления можно делать этим способом.

#### 💻 Via PC + esptool / Через ПК + esptool

1. Install [esptool](https://github.com/espressif/esptool) (`pip install esptool`).  
   Установите [esptool](https://github.com/espressif/esptool) (`pip install esptool`).
2. Connect ESP32 via USB and note the COM port.  
   Подключите ESP32 по USB и запомните COM порт.
3. Run / Выполните:
esptool.py --port COMx write_flash 0x10000 EHU32-v4.5.bin

### Option 2: Compile from Source (Arduino IDE) / Вариант 2: Скомпилировать из исходников

---

> ⚠️ **IMPORTANT — ВАЖНО**  
> Before compiling, set your Arduino IDE partition scheme to **"Minimal SPIFFS"**!  
> Перед компиляцией установите в Arduino IDE схему разделов **"Minimal SPIFFS"**!  
> Otherwise you will get a **"Sketch too big"** error.  
> Иначе получите ошибку **"Sketch too big"**.

---

Use this for the **initial flash** on a new ESP32, or if you want to modify the code.  
Используйте этот способ для **первой прошивки** новой ESP32 или если хотите изменить код.

1. Install **ESP32 Arduino Core** version **2.0.17**.  
Установите **ESP32 Arduino Core** версии **2.0.17**.
2. Install required libraries via Library Manager:  
Установите необходимые библиотеки через менеджер библиотек:
- **ESP32-A2DP** (1.8.7)
- **arduino-audio-tools** (1.1.1)
3. Open `EHU32.ino` in Arduino IDE.  
Откройте `EHU32.ino` в Arduino IDE.
4. **Tools → Partition Scheme → Minimal SPIFFS (1.9MB APP with OTA / 190KB SPIFFS)**  
**Инструменты → Partition Scheme → Minimal SPIFFS (1.9MB APP with OTA / 190KB SPIFFS)**
5. Select your board (e.g., `ESP32 Dev Module`) and COM port.  
Выберите плату (например, `ESP32 Dev Module`) и COM порт.
6. Click **Upload**.  
Нажмите **Загрузка**.

> ⚠️ **First boot after flash takes 30–40 seconds** — the system scans the vehicle's CAN bus. Do not interrupt.  
> ⚠️ **Первый запуск после прошивки занимает 30–40 секунд** — система сканирует CAN‑шину автомобиля. Не прерывайте.

### Option 3: OTA Update (Wi‑Fi) / Вариант 3: OTA обновление (по Wi‑Fi)

Once the firmware is running on the ESP32, you can update wirelessly.  
Когда прошивка уже работает на ESP32, можно обновляться по Wi‑Fi.

1. **Hold radio button "8" for ≥1 second** — the ESP32 starts a Wi‑Fi hotspot.  
**Удерживайте кнопку "8" на магнитоле ≥1 секунду** — ESP32 запускает точку доступа Wi‑Fi.
2. Connect your computer to the network:  
Подключите компьютер к сети:
- **SSID:** `EHU32-XXXX` (where XXXX = last 4 digits of MAC address / где XXXX = последние 4 цифры MAC адреса)
- **Password:** `ehu32updater`
3. In Arduino IDE, select the network port under **Tools → Port**.  
В Arduino IDE выберите сетевой порт в **Инструменты → Порт**.
4. Upload the new sketch.  
Загрузите новый скетч.
5. To exit OTA mode without updating, hold **"8" for 5 seconds**.  
Для выхода из OTA без обновления удерживайте **"8" 5 секунд**.

---

## Compilation Notes / Примечания по компиляции

### 📚 Required Library Versions / Требуемые версии библиотек

| Component / Компонент | Version / Версия |
|-----------------------|------------------|
| ESP32 Arduino Core | **2.0.17** |
| ESP32-A2DP | 1.8.7 |
| arduino-audio-tools | 1.1.1 |

### ⚙️ Arduino IDE Settings / Настройки Arduino IDE

1. **Tools → Events → Core 0**  
**Инструменты → Events → Core 0**
2. **Tools → Arduino → Core 1**  
**Инструменты → Arduino → Core 1**
3. **Tools → Partition Scheme → Minimal SPIFFS (1.9MB APP with OTA / 190KB SPIFFS)**  
**Инструменты → Partition Scheme → Minimal SPIFFS (1.9MB APP with OTA / 190KB SPIFFS)**

> ⚠️ **"Sketch too big"?** You forgot to set the partition scheme.  
> ⚠️ **Ошибка "Sketch too big"?** Вы не установили схему разделов.

### 🔧 sdkconfig TWAI Modification / Изменение sdkconfig для TWAI

The ESP-IDF TWAI driver requires a one‑time config change. Edit `sdkconfig` in the ESP32 core folder:  
Драйвер ESP-IDF TWAI требует однократного изменения конфигурации. Отредактируйте `sdkconfig` в папке ядра ESP32:

%USERPROFILE%\AppData\Local\Arduino15\packages\esp32\hardware\esp32\2.0.17\tools\sdk\esp32\

Find the `# TWAI configuration` section and set:  
Найдите секцию `# TWAI configuration` и установите:
CONFIG_TWAI_ISR_IN_IRAM=y
CONFIG_TWAI_ERRATA_FIX_TX_INTR_LOST=n

Full details in `docs/compilation-guide.md`.  
Подробнее в `docs/compilation-guide.md`.

---

## OTA Updates / OTA обновления

EHU32 supports over-the-air firmware updates:  
EHU32 поддерживает обновление прошивки "по воздуху":

1. Hold radio button **"8"** for ≥ 1 second — EHU32 starts a Wi‑Fi hotspot.  
   Удерживайте кнопку **"8"** на магнитоле ≥ 1 секунды — EHU32 запускает точку доступа Wi‑Fi.
2. Connect your computer to the **EHU32-XXXX** network (password: `ehu32updater`).  
   Подключите компьютер к сети **EHU32-XXXX** (пароль: `ehu32updater`).
3. Upload the new firmware via the Arduino IDE network port.  
   Загрузите новую прошивку через сетевой порт Arduino IDE.
4. To exit OTA mode without updating, hold **"8"** for 5 seconds — the board restarts.  
   Для выхода из режима OTA без обновления удерживайте **"8"** 5 секунд — плата перезагрузится.

---

## Documentation / Документация

| Resource / Ресурс | Description / Описание |
|--------------------|------------------------|
| `docs/` | Hardware guides, CAN details, troubleshooting |
| `EHU32_wiring.pdf` | Complete wiring diagram / Полная схема подключения |
| Wiki | CAN message database and vehicle‑specific notes |

---

## Credits / Благодарности

- **[PNKP237](https://github.com/PNKP237/EHU32)** — original author of the EHU32 project.  
  Оригинальный автор проекта EHU32.
- **[BxnnyG](https://github.com/BxnnyG/EHU32)** — detailed code comments and documentation improvements.  
  Подробные комментарии кода и улучшение документации.
- **[JJToB](https://github.com/JJToB/Car-CAN-Message-DB)** — reverse engineering of Opel/Vauxhall CAN bus messages.  
  Обратный инжиниринг сообщений CAN шины Opel/Vauxhall.
- **pschatzmann** — ESP32‑A2DP and arduino‑audio‑tools libraries.

---

## Disclaimer / Отказ от ответственности

**This project is provided "as is", without warranty of any kind. You are solely responsible for any damage to your vehicle, electronics, or anything else. Use at your own risk.**

**Проект предоставляется «как есть», без каких‑либо гарантий. Вы несёте полную ответственность за любой ущерб вашему автомобилю, электронике или чему‑либо ещё. Используйте на свой страх и риск.**яется «как есть», без каких‑либо гарантий. Вы несёте полную ответственность за любой ущерб вашему автомобилю, электронике или чему‑либо ещё. Используйте на свой страх и риск.**
