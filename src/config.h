#ifndef EHU32_CONFIG_H
#define EHU32_CONFIG_H

// ============================================================================
// EHU32 Configuration
// ============================================================================
// Centralized pin definitions and CAN bus identifiers.
// Modify pins here if your ESP32 board uses a different layout.
// ============================================================================

// --- Firmware Version ---
#define EHU32_VERSION "0.9.5 mod v4.5.2"
#define BT_DEVICE_NAME "Astra H Bluetooth"

// --- I2S Audio Output (PCM5102A DAC) ---
#define I2S_PIN_BCK     26    // Bit Clock
#define I2S_PIN_WS      25    // Word Select (LRCK)
#define I2S_PIN_DATA    22    // Serial Data Out

// --- PCM5102A Control ---
#define PCM_MUTE_CTL    23    // Soft-mute control (HIGH = unmuted)
#define PCM_ENABLE      27    // Power enable for PCM5102A (active LOW)

// --- CAN Bus (TWAI) ---
// GPIO_NUM_x type is required by TWAI_GENERAL_CONFIG_DEFAULT() and esp_sleep_enable_ext0_wakeup()
#define CAN_TX_PIN      GPIO_NUM_5    // CAN transceiver TX
#define CAN_RX_PIN      GPIO_NUM_4    // CAN transceiver RX (also ext0 wakeup source)

// ============================================================================
// CAN Bus Message Identifiers — MS-CAN (95 kbit/s)
// ============================================================================
// Reference: https://github.com/JJToB/Car-CAN-Message-DB
// These identifiers are specific to Opel/Vauxhall MS-CAN bus
// ============================================================================

// --- Steering Wheel / Rotary Controls ---
// These IDs are used both for transmitting simulated inputs and
// receiving actual hardware events from the radio and steering wheel.
#define CAN_ID_SWC_SCROLL        0x201  // Steering wheel scroll / radio source button events
                                        // TX: simulate scroll up/down; RX: radio panel button presses (buttons 0-9)
#define CAN_ID_SWC_BUTTON        0x206  // Steering wheel scroll button / SWC events
                                        // TX: simulate scroll press/release; RX: SWC media controls (play, next, prev)

// --- Climate Control ---
#define CAN_ID_AC_BUTTON         0x208  // AC panel selector knob events
                                        // TX: simulate AC knob input; RX: AC panel press/release events

// --- Diagnostic Requests (TX) ---
#define CAN_ID_DIS_REQUEST       0x246  // Request to DIS (Driver Information System)
                                        // Used for: speed, coolant temp, battery voltage
#define CAN_ID_ECC_REQUEST       0x248  // Request to ECC (Electronic Climate Control)
                                        // Used for: ambient temp, coolant (alternative path), RPM, speed

// --- Diagnostic Responses (RX) ---
#define CAN_ID_DIS_RESPONSE      0x546  // DIS measurement block response
                                        // Contains: coolant temp (0x0B), speed (0x0E), battery voltage (0x13)
#define CAN_ID_ECC_RESPONSE      0x548  // ECC measurement block response
                                        // Contains: battery voltage (0x07), coolant temp (0x10), speed+RPM (0x11)

// --- Display Control ---
#define CAN_ID_DISPLAY_WRITE     0x2C1  // Flow control / display update prevention
                                        // TX: prevent radio display update (ISO 15765-2 flow control frame)
                                        // RX: flow control reply from display unit (frame spacing, continue/abort)

// --- Radio / Headunit ---
#define CAN_ID_RADIO_DISPLAY     0x6C1  // Radio display update request (ISO 15765-2 first frame)
                                        // RX: radio requests to write text to center console display
                                        // TX: DummyFirstFrame used to invalidate radio's display call
#define CAN_ID_RADIO_POWER       0x501  // Radio power state
                                        // RX: data byte 3 == 0x18 signals radio/display shutdown

// --- Module Presence (RX) ---
#define CAN_ID_UHP_PRESENCE      0x6C7  // UHP (Universal Handsfree Phone) module status
                                        // Presence detected during startup; disables play/pause via SWC if found
#define CAN_ID_ECC_PRESENCE      0x6C8  // ECC (Electronic Climate Control) module status
                                        // Presence switches measurement requests from DIS to ECC

// --- Reserved / Future Use ---
#define CAN_ID_RESERVED_4E8      0x4E8  // Queued by receive task; not yet processed (reserved for future use)

// ============================================================================
// OTA WiFi Configuration
// ============================================================================
// Note: The hotspot SSID is generated dynamically at runtime as "EHU32-XXYY"
// where XXYY are the last two bytes of the device's soft-AP MAC address.
// This makes each unit uniquely identifiable on the network.
// To change the password, update OTA_PASSWORD here — it is applied to both
// WiFi.softAP() and ArduinoOTA.setPassword() in OTA.ino automatically.
#define OTA_PASSWORD     "ehu32updater"
#define OTA_TIMEOUT_MS   600000   // 10 minutes timeout before auto-reset

#endif // EHU32_CONFIG_H
