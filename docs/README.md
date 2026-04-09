# EHU32 Documentation

Welcome to the EHU32 documentation hub. EHU32 brings Bluetooth audio to Opel/Vauxhall vehicles by integrating with the onboard display and radio over MS-CAN.
This fork adds a transliteration function for Cyrillic song tags to Latin to the original code.
Track switching using the media buttons (left-previous, right-next) has also been added. To switch tracks, hold the button (approximately 400ms) to prevent the default function from triggering and switching tracks simultaneously.

← [Back to main README](../README.md) | [EHU32_wiring.pdf](../EHU32_wiring.pdf)

---

## Supported Radios

| Radio | Manufacturer | AUX | Status |
|-------|-------------|-----|--------|
| CD30 | Delphi/Grundig | Internal | Supported |
| CD30 MP3 | Delphi/Grundig | Internal | Supported |
| CD40 USB | Delphi | Factory | Supported |
| CDC40 Opera | Blaupunkt / Delphi | Via DAB emulation | In Progress — see [Issue #19](https://github.com/PNKP237/EHU32/issues/19) |
| CD70 Navi | Siemens/VDO | Internal | Supported (best documented) |
| DVD90 Navi | Siemens/VDO | Internal | In Progress — see [Issue #16](https://github.com/PNKP237/EHU32/issues/16) |

## Supported Displays

| Display | Description |
|---------|-------------|
| CID | Colour Information Display |
| GID (single-line) | Graphic Information Display, one text line |
| GID (three-line) | Graphic Information Display, three text lines |
| BID | Basic Information Display |
| TID | Trip Information Display |

---

## Documentation Index

| File | Description |
|------|-------------|
| [hardware-overview.md](hardware-overview.md) | Bill of materials, ESP32 pin assignments, wiring overview |
| [can-bus-connection.md](can-bus-connection.md) | MS-CAN bus details, access points, transceiver selection |
| [power-supply.md](power-supply.md) | Power options: PCB 5V, buck converter, USB charger |
| [aux-audio-connection.md](aux-audio-connection.md) | PCM5102A setup, AUX wiring, ground loop notes |
| [compilation-guide.md](compilation-guide.md) | Arduino IDE settings, sdkconfig, flashing |
| [troubleshooting.md](troubleshooting.md) | Common issues and fixes |
| [faq.md](faq.md) | Frequently asked questions |
| **Community knowledge** | |
| [known-issues-and-solutions.md](known-issues-and-solutions.md) | Recurring problems and confirmed solutions from community discussions |
| [community-knowledge.md](community-knowledge.md) | ESP32 board compatibility, display limits, data availability, device tips |
| [radio-specific-notes.md](radio-specific-notes.md) | Radio-specific technical details: CD30, CD40, CD70, DVD90, CDC40 Opera |
| **Installation guides** | |
| [installation-cd30-cd30mp3.md](installation-cd30-cd30mp3.md) | CD30 / CD30 MP3 internal installation |
| [installation-cd40usb.md](installation-cd40usb.md) | CD40 USB internal installation |
| [installation-cd70-navi.md](installation-cd70-navi.md) | CD70 Navi internal installation |
| [installation-dvd90-navi.md](installation-dvd90-navi.md) | DVD90 Navi installation (in progress) |
| [installation-cdc40-opera.md](installation-cdc40-opera.md) | CDC40 Opera (in progress) |

---

## Quick Start

1. Read [hardware-overview.md](hardware-overview.md) for the bill of materials and wiring diagram.
2. Choose your radio's installation guide from the table above.
3. Follow [compilation-guide.md](compilation-guide.md) to build and flash the firmware.
4. If something isn't working, check [troubleshooting.md](troubleshooting.md).

## Links

- [Main README](../README.md)
- [EHU32_wiring.pdf](../EHU32_wiring.pdf)
- [GitHub repository](https://github.com/PNKP237/EHU32)
- [Wiki](https://github.com/PNKP237/EHU32/wiki)
