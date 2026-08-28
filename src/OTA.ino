/*
 * OTA.ino — Over-The-Air (Wi-Fi) firmware update module
 *
 * Project: EHU32 (Opel MS-CAN Bluetooth Audio Gateway)
 * Original Author: PNKP237 — https://github.com/PNKP237/EHU32
 * Author & Maintainer: WinD52 — https://github.com/WinD52/EHU32
 *
 * Version: v1.0.0-alpha (Modern Platform Baseline: ESP-IDF 5.3 / Arduino Core 3.1)
 *
 * Functionality:
 *   - Starts a password-protected Wi-Fi SoftAP hotspot ("EHU32-XXYY")
 *   - Hosts the ArduinoOTA service for wireless flashing (UDP port 3232)
 *   - Displays dynamic SSID, password and live upload progress on car display (GID/CID)
 *   - Implements safe Bluetooth teardown and 10-minute idle auto-reset protection
 */

#include "config.h"
#include <WiFi.h>
#include <WiFiAP.h>
#include <ArduinoOTA.h>
#include "esp_mac.h"

// Учетные данные Wi-Fi SoftAP (SSID формируется динамически на основе MAC-адреса)
char ssid[20];
const char* password = OTA_PASSWORD;

// Переменные состояния конечного автомата OTA
volatile bool OTA_running = 0, OTA_finished = 0, OTA_progressing = 0;

void a2dp_stop();

// ============================================================================
// OTA_start — Запуск точки доступа Wi-Fi SoftAP и службы ArduinoOTA
// ============================================================================
void OTA_start(){
  a2dp_stop(); // Остановка Bluetooth и освобождение ресурсов I2S
  vTaskDelay(pdMS_TO_TICKS(500));

  // Формирование уникального имени сети EHU32-XXYY из последних 2 байт MAC-адреса
  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
  snprintf(ssid, sizeof(ssid), "EHU32-%02X%02X", mac[4], mac[5]);

  if (!WiFi.softAP(ssid, password)) {
    vTaskDelay(pdMS_TO_TICKS(1000));
    ESP.restart();
  } else {
    // Вывод информации о точке доступа на дисплей автомобиля
    char ota_info[48];
    snprintf(ota_info, sizeof(ota_info), "SSID: %s", ssid);
    writeTextToDisplay(true, (char*)"OTA Update Mode", ota_info, (char*)"Pass: " OTA_PASSWORD);

    ArduinoOTA
      .setMdnsEnabled(false)
      .setRebootOnSuccess(true)
      .setPassword(OTA_PASSWORD)
      .onStart([]() {
        String type;
        if (ArduinoOTA.getCommand() == U_FLASH)
          type = "sketch";
        else
          type = "filesystem";
        OTA_progressing = 1;
      })
      .onProgress([](unsigned int progress, unsigned int total) {
        // Безопасный расчет процента прошивки (64-битное умножение, защита от деления на 0)
        if(total > 0){
          unsigned int progress_val = (static_cast<uint64_t>(progress) * 100ULL) / total;
          if((progress_val % 10) == 0){
            char progress_text[32];
            snprintf(progress_text, sizeof(progress_text), "Updating... %u%%", progress_val);
            writeTextToDisplay(true, nullptr, progress_text, nullptr);
          }
        }
      })
      .onError([](ota_error_t error) {
        char err_reason[32];
        switch(error){
          case OTA_AUTH_ERROR:    snprintf(err_reason, sizeof(err_reason), "Not authenticated"); break;
          case OTA_BEGIN_ERROR:   snprintf(err_reason, sizeof(err_reason), "Error starting"); break;
          case OTA_CONNECT_ERROR: snprintf(err_reason, sizeof(err_reason), "Connection problem"); break;
          case OTA_RECEIVE_ERROR: snprintf(err_reason, sizeof(err_reason), "Error receiving"); break;
          case OTA_END_ERROR:     snprintf(err_reason, sizeof(err_reason), "Couldn't apply update"); break;
          default: break;
        }
        writeTextToDisplay(true, (char*)"Error updating", err_reason, (char*)"Resetting...");
        vTaskDelay(pdMS_TO_TICKS(3000));
        ESP.restart();
      })
      .onEnd([]() {
        prefs_clear(); // Сброс настроек для повторной инициализации после обновления
        OTA_finished = 1;
        OTA_progressing = 0;
      });

    ArduinoOTA.begin();
    OTA_running = 1;
  }
}

// ============================================================================
// OTA_Handle — Главный цикл обработки беспроводного обновления
// ============================================================================
void OTA_Handle(){
  unsigned long time_started = 0;
  while(1){
    // Ожидание флага запуска OTA (удержание кнопки 8 на магнитоле >= 1 сек)
    while(!checkFlag(OTA_begin)){
      vTaskDelay(pdMS_TO_TICKS(1000));
    }
    if(!OTA_running){
      OTA_start();
      time_started = millis();
    }
    ArduinoOTA.handle();

    // 10-минутный таймаут ожидания прошивки до автоперезагрузки
    if(!OTA_progressing){
      if(millis() - time_started >= OTA_TIMEOUT_MS){
        vTaskDelay(pdMS_TO_TICKS(1000));
        ESP.restart();
      }
    }

    // Поддержание сторожевого таймера в режиме ожидания
    if(OTA_running && !OTA_progressing && !OTA_finished){
      vTaskDelay(1);
    }

    // Принудительное прерывание OTA (удержание кнопки 8 >= 5 сек)
    if(checkFlag(OTA_abort) && OTA_running && !OTA_progressing && !OTA_finished){
      vTaskDelay(pdMS_TO_TICKS(1000));
      ESP.restart();
    }

    // Успешное завершение обновления -> перезагрузка в новую прошивку
    if(OTA_finished){
      vTaskDelay(pdMS_TO_TICKS(1000));
      ESP.restart();
    }
  }
}
