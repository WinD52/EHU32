/*
 * config.h — Central configuration file for EHU32 1.0.0-alfa
 *
 * Project: EHU32 (Opel MS-CAN Bluetooth Audio Gateway)
 * Original Author: PNKP237 — https://github.com/PNKP237/EHU32
 * Author & Maintainer: WinD52 — https://github.com/WinD52/EHU32
 *
 * Modern Platform Baseline: ESP-IDF 5.3 / Arduino Core 3.1
 *
 * Centralized pin definitions, CAN bus identifiers and system build options.
 * Target platforms: Opel/Vauxhall Astra H, Zafira B, Vectra C, Signum, Corsa D.
 */

#ifndef EHU32_CONFIG_H
#define EHU32_CONFIG_H

#include <Arduino.h>
#include "driver/twai.h"
#include "esp_a2dp_api.h"
#include "esp_mac.h"

// ============================================================================
// Конфигурация прошивки и идентификаторы Bluetooth
// ============================================================================
#define EHU32_VERSION "1.0.0-alfa"
#define BT_DEVICE_NAME "Astra H Bluetooth"

// ============================================================================
// Макросы отладочного вывода в Serial (скорость 115200 бод)
// ============================================================================
#define EHU32_DEBUG

#ifdef EHU32_DEBUG
  #define DEBUG_SERIAL(X)   Serial.begin(X)
  #define DEBUG_PRINT(X)    Serial.print(X)
  #define DEBUG_PRINTLN(X)  Serial.println(X)
  #define DEBUG_PRINTF(...) Serial.printf(__VA_ARGS__)
#else
  #define DEBUG_SERIAL(X)
  #define DEBUG_PRINT(X)
  #define DEBUG_PRINTLN(X)
  #define DEBUG_PRINTF(...)
#endif

// ============================================================================
// Аудиотракт I2S (ЦАП PCM5102A -> Аналоговый вход AUX магнитолы CD30)
// ============================================================================
#define I2S_PIN_BCK     26    // Тактовый сигнал битовой синхронизации (Bit Clock)
#define I2S_PIN_WS      25    // Выбор канала / тактовый сигнал слова (LRCK)
#define I2S_PIN_DATA    22    // Последовательная линия передачи данных (Serial Data Out)

// ============================================================================
// Управление питанием и аппаратным Mute ЦАПа PCM5102A
// ============================================================================
#define PCM_MUTE_CTL    23    // Soft-mute управление (HIGH = звук включен, LOW = Mute)
#define PCM_ENABLE      27    // Включение питания ЦАП и CAN-трансивера (активный LOW)

// ============================================================================
// Шина CAN (Аппаратный контроллер TWAI микроконтроллера ESP32)
// ============================================================================
#define CAN_TX_PIN      GPIO_NUM_5    // Линия передачи TXD на CAN-трансивер
#define CAN_RX_PIN      GPIO_NUM_4    // Линия приема RXD (источник пробуждения ext0 из сна)

// ============================================================================
// Идентификаторы сообщений шины Opel MS-CAN (95.238 кбит/с)
// Справочник: https://github.com/JJToB/Car-CAN-Message-DB
// ============================================================================

// --- Рулевое колесо и органы управления магнитолы ---
#define CAN_ID_SWC_SCROLL        0x201  // Кнопки магнитолы 0-9 / энкодер громкости
#define CAN_ID_SWC_BUTTON        0x206  // Кнопки рулевого колеса (Play, Next, Prev, OK)

// --- Блок климат-контроля (ECC) ---
#define CAN_ID_AC_BUTTON         0x208  // События центрального энкодера и кнопки панели климата

// --- Диагностические запросы параметров (TX, протокол KWP-2000) ---
#define CAN_ID_DIS_REQUEST       0x246  // Запрос параметров к дисплею DIS (скорость, ОЖ, вольтаж)
#define CAN_ID_ECC_REQUEST       0x248  // Запрос параметров к климату ECC (обороты, скорость, ОЖ)

// --- Диагностические ответы параметров (RX, протокол KWP-2000) ---
#define CAN_ID_DIS_RESPONSE      0x546  // Ответ с блоками измерений от дисплея DIS
#define CAN_ID_ECC_RESPONSE      0x548  // Ответ с блоками измерений от климат-контроля ECC

// --- Управление и синхронизация дисплея ---
#define CAN_ID_DISPLAY_WRITE     0x2C1  // Flow Control от дисплея / кадр блокировки радио (0x30)
#define CAN_ID_RADIO_DISPLAY     0x6C1  // Кадры обновления экрана от магнитолы (First Frame DoCAN)
#define CAN_ID_RADIO_POWER       0x501  // Состояние питания магнитолы (байт 3 == 0x18 — сон)

// --- Контроль присутствия штатных модулей в шине (RX) ---
#define CAN_ID_UHP_PRESENCE      0x6C7  // Статус присутствия штатного блока громкой связи UHP4
#define CAN_ID_ECC_PRESENCE      0x6C8  // Статус присутствия блока климат-контроля ECC

// --- Резервные идентификаторы ---
#define CAN_ID_RESERVED_4E8      0x4E8  // Зарезервировано для будущих расширений

// ============================================================================
// Конфигурация беспроводного обновления OTA (Wi-Fi SoftAP)
// ============================================================================
#define OTA_PASSWORD     "ehu32updater" // Пароль точки доступа и авторизации прошивки
#define OTA_TIMEOUT_MS   600000         // Таймаут ожидания прошивки (10 минут) до авторебута

#endif // EHU32_CONFIG_H
