# EHU32

**ESP32-based Bluetooth audio integration for Opel/Vauxhall vehicles with CAN bus display support.**  
**Bluetooth-интеграция на базе ESP32 для автомобилей Opel/Vauxhall с отображением информации на штатном дисплее через CAN-шину.**

---

> ⚠️ **IMPORTANT — ВАЖНО**  
> Before compiling, set your Arduino IDE partition scheme to **"Minimal SPIFFS"**!  
> Перед компиляцией установите в Arduino IDE схему разделов **"Minimal SPIFFS"**!  
> Otherwise you will get a **"Sketch too big"** error.  
> Иначе получите ошибку **"Sketch too big"**.  
> See [Compilation Notes](#compilation-notes--примечания-по-компиляции) for full details.

---

## 📌 About This Fork / Об этом форке

This fork (v4.5) extends the original [EHU32 by PNKP237](https://github.com/PNKP237/EHU32) with several quality‑of‑life improvements:

| Feature / Функция | Description / Описание |
|-------------------|------------------------|
| **Cyrillic transliteration** | Russian song tags are automatically converted to Latin characters, so no more empty lines on the display. |
| **Транслитерация кириллицы** | Русские теги треков автоматически переводятся в латиницу — пустых строк на экране больше не будет. |
| **Head unit track control** | Long‑press (≈400 ms) the **`<`** and **`>`** buttons on the radio panel to switch tracks (AUX mode only). |
| **Управление треками с магнитолы** | Долгое нажатие (≈400 мс) на кнопки **`<`** и **`>`** на панели магнитолы переключает треки (только в режиме AUX). |
| **Steering wheel OK button** | Long‑press the **left scroll wheel centre button (OK)** to cycle between **audio metadata** and **vehicle data** screens. |
| **Кнопка OK на руле** | Долгое нажатие на **центр левой крутилки (OK)** переключает экраны: **информация о треке** ↔ **данные автомобиля**. |
| **Simplified display modes** | The single‑line coolant mode (mode 2) has been removed from the rotation – only the two most useful screens remain. |
| **Упрощённые режимы экрана** | Режим с одной строкой температуры (режим 2) исключён из цикла – остались только два основных экрана. |

All original functionality (Bluetooth A2DP, steering wheel controls, OTA updates, diagnostic data) is fully preserved.

Вся оригинальная функциональность (Bluetooth A2DP, кнопки на руле, OTA‑обновления, диагностические данные) полностью сохранена.

---

## 📖 Table of Contents / Содержание

- [Features / Возможности](#features--возможности)
- [Compatibility / Совместимость](#compatibility--совместимость)
- [Quick Start / Быстрый старт](#quick-start--быстрый-старт)
- [Hardware / Аппаратное обеспечение](#hardware--аппаратное-обеспечение)
- [How It Works / Принцип работы](#how-it-works--принцип-работы)
- [Installation Options / Варианты установки](#installation-options--варианты-установки)
- [Compilation Notes / Примечания по компиляции](#compilation-notes--примечания-по-компиляции)
- [OTA Updates / OTA обновления](#ota-updates--ota-обновления)
- [Documentation / Документация](#documentation--документация)
- [Credits / Благодарности](#credits--благодарности)
- [Disclaimer / Отказ от ответственности](#disclaimer--отказ-от-ответственности)

---

## Features / Возможности

### 🎵 Bluetooth Audio

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
- **Disable screen updates** by holding **"9"** (hold 5 seconds to factory reset).  
  **Отключение вывода на экран** удержанием кнопки **"9"** (5 секунд – сброс к заводским настройкам).

### 📡 OTA Updates / OTA обновления

- Update firmware wirelessly – hold **"8"** to start the Wi‑Fi hotspot.  
  Обновление прошивки по Wi‑Fi – удерживайте **"8"** для запуска точки доступа.

---

## Compatibility / Совместимость

| Component | Supported Models |
|-----------|------------------|
| **Vehicles** | Astra H, Corsa D, Vectra C, Zafira B, Meriva A, Signum |
| **Radios** | CD30, CD30MP3, CD40USB, CDC40Opera, CD70Navi, DVD90Navi |
| **Displays** | CID (3‑line), GID (3‑line), GID (1‑line), BID, TID |

> **Requirement:** Your radio must have an **AUX input**.  
> **Требование:** Магнитола должна иметь **вход AUX**.

### ✅ Tested Configuration / Протестированная конфигурация

This firmware is confirmed working on:

| Component | Model |
|-----------|-------|
| Vehicle | Opel Astra H |
| Radio | CD30 / CD30MP3 |
| Display | GID (Graphic Info Display) |

Данная прошивка проверена на:

| Компонент | Модель |
|-----------|--------|
| Автомобиль | Opel Astra H |
| Магнитола | CD30 / CD30MP3 |
| Дисплей | GID |

Other compatible vehicles and radios **should work**, but have not been explicitly tested with this fork.  
Другие совместимые автомобили и магнитолы **должны работать**, но не тестировались с данным форком.

---

## Quick Start / Быстрый старт

1. **Gather components** – see [Hardware](#hardware--аппаратное-обеспечение).  
   **Соберите компоненты** – см. раздел [Аппаратное обеспечение](#hardware--аппаратное-обеспечение).
2. **Wire everything** according to `EHU32_wiring.pdf`.  
   **Подключите всё** согласно схеме `EHU32_wiring.pdf`.
3. **Set up Arduino IDE** with ESP32 core **2.0.17** and required libraries (see [Compilation Notes](#compilation-notes--примечания-по-компиляции)).  
   **Настройте Arduino IDE** с ядром ESP32 **2.0.17** и необходимыми библиотеками.
4. ⚠️ **Set Partition Scheme → "Minimal SPIFFS"**.  
   ⚠️ **Установите Partition Scheme → "Minimal SPIFFS"**.
5. **Flash** the firmware.  
   **Загрузите** прошивку.
6. **Connect CAN bus** (OBD‑II pins 3 & 11) and **AUX cable**.  
   **Подключите CAN‑шину** (пины 3 и 11 OBD‑II) и **AUX кабель**.
7. **Turn on the radio** – wait for the welcome message on the display.  
   **Включите магнитолу** – дождитесь приветственного сообщения на дисплее.
8. **Pair your phone** – the device appears as `"Astra H Bluetooth"`.  
   **Подключите телефон** – устройство отображается как `"Astra H Bluetooth"`.

> 💡 First boot or factory reset takes **30–40 seconds** while the system probes the vehicle's modules. Do not interrupt it.  
> 💡 Первый запуск или сброс занимает **30–40 секунд** – идёт опрос модулей автомобиля. Не прерывайте процесс.

---

## Hardware / Аппаратное обеспечение

### 🧰 Bill of Materials / Список компонентов

| Component | Notes |
|-----------|-------|
| **ESP32** | Prefer official Espressif modules with IPX antenna connector (look for "Espressif" on the RF shield). |
| **IPX antenna** | Any, even salvaged from an old laptop. |
| **PCM5102A DAC** | Ensure onboard jumpers are configured correctly (see module documentation). |
| **CAN transceiver** | MCP2551 (5V), SN65HVD230 (3.3V) or equivalent. |

| Компонент | Примечания |
|-----------|------------|
| **ESP32** | Желательно официальный модуль Espressif с разъёмом IPX (на экране должно быть выгравировано "Espressif"). |
| **IPX антенна** | Любая, можно из старого ноутбука. |
| **PCM5102A ЦАП** | Проверьте положение перемычек на плате (см. документацию модуля). |
| **CAN трансивер** | MCP2551 (5В), SN65HVD230 (3.3В) или аналоги. |

### 🔌 Pinout / Распиновка

| ESP32 Pin | Function | Connect To |
|-----------|----------|------------|
| GPIO 4 | CAN RX | Transceiver RXD |
| GPIO 5 | CAN TX | Transceiver TXD |
| GPIO 22 | I2S Data | PCM5102A DIN |
| GPIO 25 | I2S WS | PCM5102A LCK |
| GPIO 26 | I2S BCK | PCM5102A BCK |
| GPIO 23 | Mute Control | PCM5102A XSMT |
| GPIO 27 | Power Enable | DAC VDD / CAN transceiver standby |

Full diagram: `EHU32_wiring.pdf`  
Полная схема: `EHU32_wiring.pdf`

### 🔋 Power Notes / Питание

- OBD‑II provides **unswitched 12V** – use a switched 5V USB adapter for external installations.  
  OBD‑II даёт **неотключаемые 12В** – при внешней установке используйте USB‑зарядку 5В с выключателем.
- If soldering inside the head unit, use its switched power supply.  
  При пайке внутри магнитолы используйте её коммутируемое питание.
- **Do not connect headphones** – the DAC output is line‑level only.  
  **Не подключайте наушники** – выход ЦАП линейный, только для AUX входа магнитолы.

---

## How It Works / Принцип работы

- **First boot** takes ~30–40 seconds – the system scans for the display and available modules.  
  **Первый запуск** длится ~30–40 секунд – система сканирует дисплей и модули автомобиля.
- Bluetooth activates **only after** the radio starts communicating over CAN.  
  Bluetooth включается **только после** начала обмена данными по CAN.
- **Set phone volume to maximum** – this minimises background noise. Adjust listening volume with the car's controls.  
  **Громкость телефона – на максимум** – это снижает уровень шума. Громкость прослушивания регулируйте штатными средствами.
- **AUX detection:** The system scans CAN messages for the word "Aux". After switching away from AUX, there may be a short delay before the display reverts.  
  **Определение AUX:** Система ищет слово "Aux" в CAN‑сообщениях. После выхода из AUX может быть небольшая задержка перед возвратом экрана.
- **CD30/CD40 sound menu:** Press **"SOUND" twice** – the first press is intercepted, the second opens the menu.  
  **Меню звука на CD30/CD40:** Нажмите **"SOUND" дважды** – первое нажатие перехватывается, второе открывает меню.
- **Android audio issues:** In Bluetooth settings, disable **"Keep volume consistent"** for the EHU32 device.  
  **Проблемы с Android:** В настройках Bluetooth для устройства EHU32 отключите опцию **"Keep volume consistent"**.

---

## Installation Options / Варианты установки

### 🔌 External (Simplest) / Внешняя (проще)

- Connect everything to the OBD‑II port (CAN) and AUX input.  
  Подключите всё к разъёму OBD‑II (CAN) и AUX входу.
- Power from a switched USB car charger.  
  Питание от USB‑зарядки с выключателем.
- All components remain outside the dashboard.  
  Все компоненты остаются снаружи.

### 🧰 Internal (Cleanest) / Внутренняя (аккуратнее)

- Solder the board inside the head unit for a factory‑look installation.  
  Впаяйте плату внутрь магнитолы для заводского вида.
- Guides for specific radios are available in the `docs/` folder and the project Wiki.  
  Инструкции для конкретных магнитол есть в папке `docs/` и в Wiki проекта.

---

## Compilation Notes / Примечания по компиляции

### 📚 Required Libraries / Требуемые библиотеки

| Library | Version |
|---------|---------|
| ESP32 Arduino Core | **2.0.17** |
| ESP32-A2DP | 1.8.7 |
| arduino-audio-tools | 1.1.1 |

### ⚙️ Arduino IDE Settings / Настройки Arduino IDE

1. **Tools** → **Events** → **Core 0**  
2. **Tools** → **Arduino** → **Core 1**  
3. **Tools** → **Partition Scheme** → **Minimal SPIFFS (1.9MB APP with OTA / 190KB SPIFFS)**  

> ⚠️ **"Sketch too big"?** You forgot to set the partition scheme.  
> ⚠️ **Ошибка "Sketch too big"?** Вы не установили схему разделов.

### 🔧 sdkconfig TWAI Modification / Изменение sdkconfig для TWAI

Edit `sdkconfig` in the ESP32 core folder (path varies by OS) and ensure the TWAI section matches:
CONFIG_TWAI_ISR_IN_IRAM=y
CONFIG_TWAI_ERRATA_FIX_BUS_OFF_REC=y
CONFIG_TWAI_ERRATA_FIX_TX_INTR_LOST=n
CONFIG_TWAI_ERRATA_FIX_RX_FRAME_INVALID=y
CONFIG_TWAI_ERRATA_FIX_RX_FIFO_CORRUPT=y


Detailed instructions are in `docs/compilation-guide.md`.  
Подробная инструкция – в `docs/compilation-guide.md`.

---

## OTA Updates / OTA обновления

1. **Hold radio button "8" for ≥1 second** – the ESP32 creates a Wi‑Fi hotspot.  
   **Удерживайте кнопку "8" ≥1 секунду** – ESP32 создаст точку доступа Wi‑Fi.
2. Connect your computer to the network (SSID and password are in `config.h`).  
   Подключите компьютер к сети (имя и пароль в `config.h`).
3. Upload the new firmware via Arduino IDE (network port).  
   Загрузите новую прошивку через Arduino IDE (сетевой порт).
4. To exit without updating, hold **"8" for 5 seconds**.  
   Для выхода без обновления удерживайте **"8" 5 секунд**.

---

## Documentation / Документация

| Resource | Description |
|----------|-------------|
| `docs/` | Hardware guides, CAN details, troubleshooting. |
| `EHU32_wiring.pdf` | Complete wiring diagram. |
| Wiki | CAN message database and vehicle‑specific notes. |

---

## Credits / Благодарности

- **[PNKP237](https://github.com/PNKP237/EHU32)** – original author of EHU32.  
- **[BxnnyG](https://github.com/BxnnyG/EHU32)** – detailed code comments and documentation improvements.  
- **JJToB** – [Car‑CAN‑Message‑DB](https://github.com/JJToB/Car-CAN-Message-DB) for Opel/Vauxhall CAN reverse engineering.  
- **pschatzmann** – ESP32‑A2DP and arduino‑audio‑tools libraries.

---

## Disclaimer / Отказ от ответственности

**This project is provided "as is", without warranty of any kind. You are solely responsible for any damage to your vehicle, electronics, or anything else. Use at your own risk.**

**Проект предоставляется «как есть», без каких‑либо гарантий. Вы несёте полную ответственность за любой ущерб вашему автомобилю, электронике или чему‑либо ещё. Используйте на свой страх и риск.**
