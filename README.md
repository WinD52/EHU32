# EHU32

ESP32-based Bluetooth audio integration for Opel/Vauxhall vehicles with CAN bus display support.
## ⚠️ This fork modifications (v3)

This version adds two quality-of-life features to the original EHU32 project:

1.  **Cyrillic transliteration:** Song tags in Russian are automatically transliterated to Latin characters, solving the issue of empty text on the display.
2.  **Head unit track control:** You can switch tracks by **long-pressing (≈400 ms)** the **`<`** and **`>`** buttons on the car's radio panel. This works only in AUX mode and does not conflict with the standard button functions.


> ⚠️ **IMPORTANT: Before compiling, set your Arduino IDE partition scheme to "Minimal SPIFFS"!**
> Without this, you will get a **"Sketch too big"** compilation error. See [Compilation Notes](#compilation-notes) for full details.

---

## Table of Contents

1. [Features](#features)
2. [Compatibility](#compatibility)
3. [Quick Start](#quick-start)
4. [Demo](#demo)
5. [Hardware — Building it yourself](#hardware--building-it-yourself)
6. [How it works and usage tips](#how-it-works-and-usage-tips)
7. [Installation Options](#installation-options)
8. [Compilation Notes](#compilation-notes)
9. [OTA Updates](#ota-updates)
10. [Documentation](#documentation)
11. [Credits](#credits)
12. [Disclaimer](#disclaimer)

---

## Features

### Bluetooth Audio
- **Bluetooth (A2DP) audio** — output to an external I2S DAC (PCM5102A)
- **Automatically reconnects** to your phone when the radio is started

### Display Integration
- Shows currently playing track on the center console display in **Aux mode**
  - Prints **Artist**, **Track title** and **Album**, just like regular CD playback
  - Default mode; also accessible by long-pressing **"1"**

### Vehicle Controls
- **Steering wheel buttons** — play/pause, previous/next track
- **AC macro** — toggle AC with a single long-press of the AC selector knob (held ≥ 0.5 s)

### Diagnostic Data Display
- Live vehicle data: speed, RPMs, coolant temperature, battery voltage
  - Accessible by long-pressing **"2"** on the radio panel
  - For single-line displays: long-press **"3"** for coolant temperature only
  - Disable screen output entirely by holding **"9"** (hold 5 s to reset and clear all settings)

### OTA Updates
- Update firmware over-the-air — hold **"8"** to enable the Wi-Fi hotspot; see [OTA Updates](#ota-updates)

---

## Compatibility

| Component | Supported Models |
|---|---|
| **Vehicles** | Astra H, Corsa D, Vectra C, Zafira B, Meriva A, Signum |
| **Radios** | CD30, CD30MP3, CD40USB, CDC40Opera, CD70Navi, DVD90Navi |
| **Displays** | CID (3-line), GID (1-line), GID (3-line), BID, TID |

> **Requirement:** Your radio must have an **Aux input**.

---

## Quick Start

1. **Get the components** — see the [Hardware section](#hardware--building-it-yourself) for the bill of materials
2. **Wire them up** — follow [EHU32_wiring.pdf](https://github.com/PNKP237/EHU32/blob/main/EHU32_wiring.pdf)
3. **Set up Arduino IDE** — install required libraries and the ESP32 core (see [Compilation Notes](#compilation-notes))
4. ⚠️ **Set partition scheme to "Minimal SPIFFS"** — *without this you will get a "Sketch too big" error!*
5. **Flash** the firmware to your ESP32
6. **Connect CAN bus and AUX cable** to the OBD-II port (pins 3/11) and the radio's Aux input
7. **Pair your phone** — EHU32 appears as a Bluetooth device once the radio is on

> 💡 **First start:** The initial boot (or hard reset) takes 30–40 seconds while EHU32 probes your vehicle's display and modules. Turn on the headunit (ignition not required) and wait for the startup message before touching anything.

---

## Demo

<details>
<summary>📺 Click to show demo videos and screenshots</summary>

[![EHU32 demo on YouTube](https://img.youtube.com/vi/CZvhz3yvV1g/0.jpg)](https://www.youtube.com/watch?v=CZvhz3yvV1g) [![EHU32 demo on YouTube](https://img.youtube.com/vi/cj5L4aGAB5w/0.jpg)](https://www.youtube.com/watch?v=cj5L4aGAB5w)

Extended demo: [https://www.youtube.com/watch?v=8fi7kX9ci_o](https://www.youtube.com/watch?v=8fi7kX9ci_o)

Live diagnostic data demo (warning: contains music!): [https://www.youtube.com/watch?v=uxLYr1c_TJA](https://www.youtube.com/watch?v=uxLYr1c_TJA)

![EHU32 in action](https://github.com/user-attachments/assets/ea93fcec-3e86-4963-869a-c7194ca0c965)

![EHU32 hardware photo](https://github.com/PNKP237/EHU32/assets/153071841/46e31e0d-70b7-423b-9a04-b4522eb96506)

![EHU32 display screenshot](https://github.com/PNKP237/EHU32/assets/153071841/030defa7-99e6-42d9-bbc5-f6a6a656e597)

</details>

---

## Hardware — Building it yourself

### Bill of Materials

- **ESP32 module** — preferably an official Espressif module with an IPX antenna connector. **Look for boards with "Espressif" etched on the RF shield** (boards without this have been found to cause I2S audio issues, especially with iPhones and Huawei phones)
- **IPX antenna** — any will do; recovered from an old laptop or bought cheaply online
- **PCM5102A DAC module** — with configurable jumpers on the bottom; **configure the jumpers correctly before use**
- **CAN transceiver module** — see table below

### CAN Transceiver Options

| Module | VCC | Notes |
|---|---|---|
| MCP2551 | 5V | Most common, widely available |
| TDA104x / TDA1050 | 5V | |
| SN65HVD23x | 3.3V | Lower voltage option |

### Pin Assignment

| ESP32 Pin | Function | Connected To |
|---|---|---|
| GPIO 4 | CAN RX | CAN Transceiver RXD |
| GPIO 5 | CAN TX | CAN Transceiver TXD |
| GPIO 22 | I2S Data | PCM5102A DIN |
| GPIO 25 | I2S Word Select | PCM5102A LCK |
| GPIO 26 | I2S Bit Clock | PCM5102A BCK |
| GPIO 23 | PCM Mute Control | PCM5102A XSMT |
| GPIO 27 | PCM Power Enable | Power switch (enables PCM5102A VDD and wakes CAN transceiver from standby) |

Refer to [EHU32_wiring.pdf](https://github.com/PNKP237/EHU32/blob/main/EHU32_wiring.pdf) for the full wiring diagram.

### CAN Bus Access

The MS-CAN bus is accessible via:
- **OBD-II diagnostic port** — pins 3 (CAN-H) and 11 (CAN-L)
- The headunit, display, climate control panel, or factory Bluetooth hands-free module

### Power Notes

- The OBD-II port provides **unswitched 12V only** — if installed externally, power from a switched 5V USB car charger
- If soldering inside the headunit, use the radio's switched power supply
- **Do not connect headphones** to the DAC output — it is line-level only (connect to the radio's AUX input)

---

## How it works and usage tips

- **First start / hard reset** takes 30–40 seconds — EHU32 probes your vehicle's display and modules for compatibility. **Turn on your headunit and wait** for the startup message before doing anything else.
- **Bluetooth activates only** after EHU32 detects the radio communicating with the display over CAN bus.
- **Set audio source volume to maximum** on your phone to minimise noise floor. Adjust listening volume using the radio's knob or steering wheel buttons. Reduce phone volume only if you hear obvious clipping/distortion.
- **Aux mode detection:** EHU32 scans for "Aux" in CAN messages. After switching away from Aux, there may be a short delay before the display updates to FM/CD mode.
- **CD30/CD40 users:** Press **"SOUND" twice** to access bass/treble/balance settings. EHU32 must block messages before it knows their content, so the first press is consumed — the menu appears on the second press.
- **Android audio issues (skipping/crackling):** In Bluetooth settings, select EHU32 from the device list and disable **"Keep volume consistent"**.

For CAN message details and vehicle-specific reverse engineering notes, see the [wiki](https://github.com/PNKP237/EHU32/wiki) and the [`docs/`](docs/) folder.

---

## Installation Options

### External (simplest — no disassembly)
- ESP32 + modules connected to the **OBD-II port** (CAN bus) and **Aux input** of the radio
- Power from a switched 5V USB car charger plugged into the cigarette lighter
- Everything stays outside the dashboard

### Internal (cleanest — inside the headunit)
Installing inside the headunit gives a factory-clean result with no visible cables.

| Radio | Guide |
|---|---|
| **CD30MP3** (Delphi-Grundig) | [Issue #3 comment](https://github.com/PNKP237/EHU32/issues/3#issuecomment-2121866276) |
| **CD70Navi** | [Wiki: Hardware modification](https://github.com/PNKP237/EHU32/wiki/Hardware-modification-%E2%80%90-EHU32-installation-in-CD70-Navi) |
| **Other radios** | See [`docs/`](docs/) folder |

---

## Compilation Notes

### Required Library Versions

| Component | Version |
|---|---|
| ESP32 Arduino Core | **2.0.17** (newer versions not stable enough) |
| ESP32-A2DP | 1.8.7 |
| arduino-audio-tools | 1.1.1 |

### Arduino IDE Settings

1. Open **Tools** menu
2. Set **Events** → Core 0
3. Set **Arduino** → Core 1
4. Set **Partition Scheme** → **Minimal SPIFFS** ← ⚠️ *see warning below*

> ### ⚠️ CRITICAL: Partition Scheme Must Be "Minimal SPIFFS"
>
> If you see a **"Sketch too big"** compilation error, you have not set the partition scheme correctly.
>
> **Fix:** In Arduino IDE → Tools → Partition Scheme → select **"Minimal SPIFFS (1.9MB APP with OTA / 190KB SPIFFS)"**
>
> This is the single most common setup problem. The sketch is too large for the default partition layout.

### sdkconfig TWAI Modification

The ESP-IDF TWAI (CAN) driver requires a one-time config change to work reliably. Edit **`sdkconfig`** located at:

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

### Common Compilation Errors

| Error | Cause | Fix |
|---|---|---|
| `Sketch too big` | Wrong partition scheme | Set **Partition Scheme → Minimal SPIFFS** |
| Missing library | ESP32-A2DP or audio-tools not installed | Install via Arduino Library Manager |
| Version mismatch | Wrong ESP32 core version | Downgrade to core **2.0.17** |

For more details, see [`docs/compilation-guide.md`](docs/compilation-guide.md).

---

## OTA Updates

EHU32 supports over-the-air firmware updates:

1. Hold radio button **"8"** for ≥ 1 second — EHU32 starts a Wi-Fi hotspot
2. Connect your computer to the **EHU32-OTA** network (see `OTA.ino` for credentials)
3. Upload the new firmware via the Arduino IDE network port
4. To exit OTA mode without updating, hold **"8"** for 5 seconds — the board restarts

---

## Documentation

| Resource | Description |
|---|---|
| [`docs/`](docs/) | Full hardware installation guides, CAN bus details, troubleshooting |
| [`docs/hardware-overview.md`](docs/hardware-overview.md) | Component details and pin reference |
| [`docs/compilation-guide.md`](docs/compilation-guide.md) | Detailed build setup instructions |
| [`docs/troubleshooting.md`](docs/troubleshooting.md) | Common problems and solutions |
| [`docs/faq.md`](docs/faq.md) | Frequently asked questions |
| [`EHU32_wiring.pdf`](EHU32_wiring.pdf) | Full wiring diagram |
| [Wiki](https://github.com/PNKP237/EHU32/wiki) | CAN message reference, vehicle-specific notes |
| [`CONTRIBUTING.md`](CONTRIBUTING.md) | How to contribute to the project |

---

## Credits

- **[ESP32-A2DP](https://github.com/pschatzmann/ESP32-A2DP)** and **[arduino-audio-tools](https://github.com/pschatzmann/arduino-audio-tools)** by [pschatzmann](https://github.com/pschatzmann) — Bluetooth A2DP and I2S audio libraries
- **[Car-CAN-Message-DB](https://github.com/JJToB/Car-CAN-Message-DB)** by [JJToB](https://github.com/JJToB) — reverse engineering of Opel/Vauxhall CAN bus messages

---

## Disclaimer

This project comes with **absolutely no warranty of any kind**. I am not responsible for any damage to your vehicle, electronics, or anything else. Use at your own risk.
