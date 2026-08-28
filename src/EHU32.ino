/*
 * EHU32.ino — Main entry point, global state, RTOS task creation and setup routine
 *
 * Project: EHU32 1.0 (Opel MS-CAN Bluetooth Audio Gateway)
 * Original Author: PNKP237 — https://github.com/PNKP237/EHU32
 * Author & Maintainer: WinD52 — https://github.com/WinD52/EHU32
 *
 * Version: v1.0.0-alpha (Modern Platform Baseline: ESP-IDF 5.3 / Arduino Core 3.1)
 *
 * Core architecture:
 *   - Two-core FreeRTOS scheduling (PRO_CPU Core 0 / APP_CPU Core 1)
 *   - Native TWAI (MS-CAN 95.238 kbit/s) driver integration
 *   - Asynchronous inter-task communication via FreeRTOS Queues, Mutexes and EventGroups
 *   - Power state management and ext0 deep sleep wake-up sequencing
 */

#include "config.h"
#include "BluetoothA2DPSink.h"
#include "esp_sleep.h"
#include "driver/twai.h"
#include <Preferences.h>
#include <WiFi.h>
#include <WiFiAP.h>
#include <ArduinoOTA.h>

// ============================================================================
// Битовые флаги группы событий FreeRTOS (Event-Group)
// ============================================================================
#define DIS_forceUpdate            (1 << 0)   // Запрос принудительного обновления экрана
#define CAN_MessageReady           (1 << 1)   // Первый кадр передан, ожидается Flow Control (0x30)
#define CAN_prevTxFail             (1 << 2)   // Ошибка передачи предыдущего кадра CAN
#define CAN_abortMultiPacket       (1 << 3)   // Прерывание многопакетной передачи (радио прислало новый FF)
#define CAN_flowCtlFail            (1 << 4)   // Сбой блокировки кадра радио (штрафная задержка 300 мс)
#define CAN_speed_recvd            (1 << 5)   // Получен блок скорости автомобиля
#define CAN_coolant_recvd          (1 << 6)   // Получен блок температуры охлаждающей жидкости
#define CAN_new_dataSet_recvd      (1 << 7)   // Получен полный комплект измерений (скорость, ОЖ, вольтаж)
#define CAN_voltage_recvd          (1 << 8)   // Получен блок напряжения аккумулятора
#define CAN_measurements_requested (1 << 9)   // Зарезервировано
#define disp_mode_changed          (1 << 10)  // Смена режима экрана (вывод статуса "No data yet...")
#define CAN_allowAutoRefresh       (1 << 11)  // Режим AUX подтвержден магнитолой -> перехват экрана разрешен
#define ECC_present                (1 << 12)  // Присутствие блока климат-контроля ECC в шине
#define ehu_started                (1 << 13)  // Магнитола включена (принят первый кадр 0x6C1 или 0x501)
#define a2dp_started               (1 << 14)  // Стек Bluetooth A2DP успешно запущен
#define flag_bt_connected          (1 << 15)  // Смартфон подключен по Bluetooth
#define bt_state_changed           (1 << 16)  // Изменение статуса подключения Bluetooth
#define bt_audio_playing           (1 << 17)  // Аудиопоток активен (воспроизведение)
#define audio_state_changed        (1 << 18)  // Изменение статуса воспроизведения (Play / Pause)
#define md_album_recvd             (1 << 19)  // Получен тег альбома по AVRCP
#define md_artist_recvd            (1 << 20)  // Получен тег исполнителя по AVRCP
#define md_title_recvd             (1 << 21)  // Получен тег названия трека по AVRCP
#define OTA_begin                  (1 << 22)  // Запуск режима беспроводного обновления Wi-Fi OTA
#define OTA_abort                  (1 << 23)  // Принудительное прерывание OTA и перезагрузка

// ============================================================================
// Дескрипторы задач, очередей и синхронизации FreeRTOS
// ============================================================================
TaskHandle_t canReceiveTaskHandle, canDisplayTaskHandle, canProcessTaskHandle,
             canTransmitTaskHandle, canWatchdogTaskHandle, canAirConMacroTaskHandle,
             canMessageDecoderTaskHandle, eventHandlerTaskHandle;

QueueHandle_t canRxQueue, canTxQueue, canDispQueue;
SemaphoreHandle_t CAN_MsgSemaphore = NULL, BufferSemaphore = NULL;
EventGroupHandle_t eventGroup;

// Состояние драйвера TWAI и идентификатор дисплея
uint32_t alerts_triggered;
twai_status_info_t status_info;
uint32_t displayMsgIdentifier = 0;

// Текстовые буферы формирования сообщений
char DisplayMsg[1024], CAN_MsgArray[128][8], title_buffer[64], artist_buffer[64], album_buffer[64];
char coolant_buffer[32], speed_buffer[32], voltage_buffer[32];

// Режим отображения экрана (-1: выкл, 0: метаданные A2DP, 1: 3 строки авто, 2: 1 строка ОЖ)
volatile int disp_mode = -1;

// Таймштампы ограничения частоты событий
unsigned long last_millis = 0, last_millis_req = 0, last_millis_disp = 0, last_millis_aux = 0;
bool vehicle_ECC_present, vehicle_UHP_present;

// Прототипы функций задач и системных обработчиков
void canReceiveTask(void* pvParameters);
void canTransmitTask(void* pvParameters);
void canProcessTask(void* pvParameters);
void canDisplayTask(void* pvParameters);
void canWatchdogTask(void* pvParameters);
void canAirConMacroTask(void* pvParameters);
void canMessageDecoder(void* pvParameters);
void eventHandlerTask(void* pvParameters);

void a2dp_init();
void A2DP_EventHandler();
void a2dp_play();
void a2dp_pause();
void a2dp_next();
void a2dp_previous();
void a2dp_stop();
void a2dp_shutdown();

bool twai_init();
void requestMeasurementBlocks();
void requestCoolantTemperature();
void prepareMultiPacket(int bytes_processed, char* buffer_to_read);
int processDisplayMessage(char* upper_line_buffer, char* middle_line_buffer, char* lower_line_buffer);
void OTA_Handle();
void writeTextToDisplay(bool disp_mode_override = false, char* up_line_text = nullptr, char* mid_line_text = nullptr, char* low_line_text = nullptr);

// ============================================================================
// Вспомогательные функции взаимодействия с EventGroup и NVS
// ============================================================================
void setFlag(uint32_t bit){ xEventGroupSetBits(eventGroup, bit); }
void clearFlag(uint32_t bit){ xEventGroupClearBits(eventGroup, bit); }
void waitForFlag(uint32_t bit, TickType_t ticksToWait = portMAX_DELAY){ xEventGroupWaitBits(eventGroup, bit, pdFALSE, pdTRUE, ticksToWait); }
bool checkFlag(uint32_t bit){ EventBits_t bits = xEventGroupGetBits(eventGroup); return (bits & bit) != 0; }

void prefs_clear(){
  Preferences settings; settings.begin("my-app", false); settings.clear(); settings.end();
}
bool getPreferencesBool(const char* key){
  Preferences settings; settings.begin("my-app", true); bool result = settings.getBool(key, 0); settings.end(); return result;
}
void setPreferencesBool(const char* key, bool value){
  Preferences settings; settings.begin("my-app", false); settings.putBool(key, value); settings.end();
}

// ============================================================================
// Главная функция инициализации setup()
// ============================================================================
void setup(){
  pinMode(CAN_RX_PIN, INPUT_PULLUP);
  esp_sleep_enable_ext0_wakeup(CAN_RX_PIN, 0); // Пробуждение по уровню LOW (доминантный бит CAN)
  pinMode(PCM_MUTE_CTL, OUTPUT);
  pinMode(PCM_ENABLE, OUTPUT);
  digitalWrite(PCM_MUTE_CTL, HIGH);
  digitalWrite(PCM_ENABLE, HIGH); // Питание ЦАП и трансивера отключено до подтверждения активности шины
  delay(20);

  DEBUG_SERIAL(115200);
  DEBUG_PRINTLN("\n=== EHU32 2.0 START (115200) ===");

  // 1. Инициализация объектов синхронизации FreeRTOS до запуска драйверов
  CAN_MsgSemaphore = xSemaphoreCreateMutex();
  BufferSemaphore  = xSemaphoreCreateMutex();
  canRxQueue       = xQueueCreate(100, sizeof(twai_message_t));
  canTxQueue       = xQueueCreate(100, sizeof(twai_message_t));
  canDispQueue     = xQueueCreate(255, sizeof(uint8_t));
  eventGroup       = xEventGroupCreate();

  // 2. Инициализация драйвера TWAI (CAN 95.238 кбит/с)
  if(!twai_init()){
    DEBUG_PRINTLN("CAN: TWAI initialization failed! Sleeping...");
    #ifdef EHU32_DEBUG
    vTaskDelay(pdMS_TO_TICKS(10));
    #endif
    esp_deep_sleep_start();
  }

  // 3. Проверка активности шины (100 мс) — чистый цикл энергосбережения WinD52
  twai_message_t testMsg;
  if(twai_receive(&testMsg, pdMS_TO_TICKS(100)) != ESP_OK){
    DEBUG_PRINTLN("CAN inactive. Back to sleep!");
    #ifdef EHU32_DEBUG
    vTaskDelay(pdMS_TO_TICKS(10));
    #endif
    esp_deep_sleep_start();
  }

  // Шина активна -> включаем питание ЦАП PCM5102A и трансивера CAN (активный LOW)
  digitalWrite(PCM_ENABLE, LOW);

  // 4. Загрузка конфигурации из энергонезависимой памяти (NVS)
  Preferences settings;
  settings.begin("my-app", false);
  if(!settings.isKey("setupcomplete")){
    DEBUG_PRINTLN("CAN SETUP: Key does not exist! Creating keys");
    settings.clear();
    settings.putBool("setupcomplete", 0);
    settings.putBool("uhppresent", 0);
    settings.putBool("eccpresent", 0);
    settings.putBool("vectra", 0);
    settings.putUInt("identifier", 0);
  }
  bool init_setupComplete = settings.getBool("setupcomplete", 0);

  // Первичный скан шины (только при первом старте)
  if(!init_setupComplete){
    unsigned long millis_init_start = millis();
    bool init_usedCANids[16] = {0};

    // Сканирование диапазона 0x6C0..0x6CF в течение 20 секунд
    while(millis() - millis_init_start < 20000){
      if(twai_receive(&testMsg, pdMS_TO_TICKS(50)) == ESP_OK){
        if((testMsg.identifier & 0xFF0) == 0x6C0){
          int idx = testMsg.identifier - 0x6C0;
          if(idx >= 0 && idx < 16 && !init_usedCANids[idx]){
            init_usedCANids[idx] = 1;
            if(testMsg.identifier == CAN_ID_UHP_PRESENCE){
              settings.putBool("uhppresent", 1);
              vehicle_UHP_present = 1;
            }
            if(testMsg.identifier == CAN_ID_ECC_PRESENCE){
              settings.putBool("eccpresent", 1);
              vehicle_ECC_present = 1;
            }
            DEBUG_PRINTF("CAN SETUP: Marking 0x%03X as a CAN ID in use\n", testMsg.identifier);
          }
        }
      }
    }

    // Подбор свободного адреса для передачи на дисплей
    DEBUG_PRINT("CAN SETUP: Attempting to test display responses: ");
    twai_message_t testMsgTx = { .identifier = 0x6C0, .data_length_code = 8, .data = {0x10, 0xA7, 0x40, 0x00, 0xA4, 0x03, 0x10, 0x13}};
    for(int i = 0; i < 16 && displayMsgIdentifier == 0; i++){
      if(init_usedCANids[i]) continue;
      testMsgTx.identifier = (0x6C0 + i);
      DEBUG_PRINTF("0x%03X... ", testMsgTx.identifier);
      twai_transmit(&testMsgTx, pdMS_TO_TICKS(300));
      unsigned long millis_transmitted = millis();
      while((millis() - millis_transmitted < 1000) && displayMsgIdentifier == 0){
        if(twai_receive(&testMsg, pdMS_TO_TICKS(100)) == ESP_OK){
          if(testMsg.data_length_code >= 1 && testMsg.identifier == (testMsgTx.identifier - 0x400) && testMsg.data[0] == 0x30){
            displayMsgIdentifier = testMsgTx.identifier;
            DEBUG_PRINTF("got a response on 0x%03X!", testMsg.identifier);
          }
        }
      }
      vTaskDelay(pdMS_TO_TICKS(100));
    }
    if(displayMsgIdentifier == 0){
      if(init_usedCANids[8] == 1){
        displayMsgIdentifier = CAN_ID_ECC_PRESENCE;
        DEBUG_PRINTLN("\nCAN SETUP: Unable to find a valid unused CAN ID, but detected ECC -> using 0x6C8");
      } else {
        displayMsgIdentifier = CAN_ID_RADIO_DISPLAY;
        DEBUG_PRINTLN("\nCAN SETUP: Unable to find a valid unused CAN ID. Falling back to stock -> using 0x6C1");
      }
    }
    DEBUG_PRINTLN("\nCAN SETUP: Saving the display message identifier to flash...");
    settings.putUInt("identifier", displayMsgIdentifier);
    settings.putBool("setupcomplete", 1);
  } else {
    displayMsgIdentifier = settings.getUInt("identifier", 0);
    if(displayMsgIdentifier == 0) displayMsgIdentifier = CAN_ID_RADIO_DISPLAY;
    DEBUG_PRINTF("CAN SETUP: Get the display identifier from flash -> 0x%03X\n", displayMsgIdentifier);
    vehicle_ECC_present = settings.getBool("eccpresent", 0);
    vehicle_UHP_present = settings.getBool("uhppresent", 0);
  }
  settings.end();

  // 5. Создание и привязка задач к ядрам FreeRTOS (Core 0 / Core 1)
  xTaskCreatePinnedToCore(canReceiveTask, "CANbusReceiveTask", 4096, NULL, 1, &canReceiveTaskHandle, 1);
  xTaskCreatePinnedToCore(canTransmitTask, "CANbusTransmitTask", 4096, NULL, 1, &canTransmitTaskHandle, 0);
  xTaskCreatePinnedToCore(canProcessTask, "CANbusMessageProcessor", 4096, NULL, 2, &canProcessTaskHandle, 0);
  xTaskCreatePinnedToCore(canDisplayTask, "DisplayUpdateTask", 4096, NULL, 1, &canDisplayTaskHandle, 1);
  vTaskSuspend(canDisplayTaskHandle);
  xTaskCreatePinnedToCore(canWatchdogTask, "WatchdogTask", 2048, NULL, tskIDLE_PRIORITY, &canWatchdogTaskHandle, 0);
  xTaskCreatePinnedToCore(canMessageDecoder, "MessageDecoder", 2048, NULL, tskIDLE_PRIORITY, &canMessageDecoderTaskHandle, 0);
  vTaskSuspend(canMessageDecoderTaskHandle);
  xTaskCreatePinnedToCore(canAirConMacroTask, "AirConMacroTask", 2048, NULL, 10, &canAirConMacroTaskHandle, 0);
  vTaskSuspend(canAirConMacroTaskHandle);
  xTaskCreatePinnedToCore(eventHandlerTask, "eventHandler", 4096, NULL, 4, &eventHandlerTaskHandle, 1);
}

// ============================================================================
// canWatchdogTask — Сторожевой таймер активности шины CAN и магнитолы (15 сек)
// ============================================================================
void canWatchdogTask(void *pvParameters){
  while(1){
    if(ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(15000)) == 0){
      DEBUG_PRINTLN("WATCHDOG: Triggering software reset...");
      vTaskDelay(pdMS_TO_TICKS(100));
      a2dp_shutdown();
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

// ============================================================================
// writeTextToDisplay — Мост между текстом и многопакетным буфером DoCAN
// ============================================================================
void writeTextToDisplay(bool disp_mode_override, char* up_line_text, char* mid_line_text, char* low_line_text){
  DEBUG_PRINTLN("EVENTS: Refreshing buffer...");
  xSemaphoreTake(CAN_MsgSemaphore, portMAX_DELAY);
  xSemaphoreTake(BufferSemaphore, portMAX_DELAY);
  if(!disp_mode_override){
    if(disp_mode == 0 && (album_buffer[0] != '\0' || title_buffer[0] != '\0' || artist_buffer[0] != '\0')){
      prepareMultiPacket(processDisplayMessage(album_buffer, title_buffer, artist_buffer), DisplayMsg);
    } else {
      if(disp_mode == 1){
        prepareMultiPacket(processDisplayMessage(coolant_buffer, speed_buffer, voltage_buffer), DisplayMsg);
      }
      if(disp_mode == 2){
        prepareMultiPacket(processDisplayMessage(nullptr, coolant_buffer, nullptr), DisplayMsg);
      }
    }
  } else {
    prepareMultiPacket(processDisplayMessage(up_line_text, mid_line_text, low_line_text), DisplayMsg);
  }
  xSemaphoreGive(CAN_MsgSemaphore);
  xSemaphoreGive(BufferSemaphore);
  vTaskResume(canDisplayTaskHandle);
  clearFlag(DIS_forceUpdate);
}

// ============================================================================
// eventHandlerTask — Главный диспетчер событий, KWP-2000 блоков и шины
// ============================================================================
void eventHandlerTask(void *pvParameters){
  while(1){
    if(checkFlag(OTA_begin)){
      disp_mode = 0;
      writeTextToDisplay(true, (char*)"Bluetooth off", (char*)"OTA Started", (char*)"Waiting for connection...");
      vTaskDelay(1000);
      vTaskSuspend(canWatchdogTaskHandle);
      OTA_Handle();
    }

    if(disp_mode == 1 && checkFlag(ehu_started)){
      if(checkFlag(disp_mode_changed)){
        clearFlag(disp_mode_changed);
        writeTextToDisplay(true, nullptr, (char*)"No data yet...", nullptr);
      }
      if(millis() - last_millis_req >= 400){
        requestMeasurementBlocks();
        last_millis_req = millis();
      }
      if((millis() - last_millis_disp >= 400) && checkFlag(CAN_new_dataSet_recvd)){
        clearFlag(CAN_new_dataSet_recvd);
        writeTextToDisplay();
        last_millis_disp = millis();
      }
    }

    if(disp_mode == 2 && checkFlag(ehu_started)){
      if(checkFlag(disp_mode_changed)){
        clearFlag(disp_mode_changed);
        writeTextToDisplay(true, nullptr, (char*)"No data yet...", nullptr);
      }
      if(millis() - last_millis_req >= 3000){
        requestCoolantTemperature();
        last_millis_req = millis();
      }
      if((millis() - last_millis_disp >= 3000) && checkFlag(CAN_coolant_recvd)){
        clearFlag(CAN_coolant_recvd);
        writeTextToDisplay();
        last_millis_disp = millis();
      }
    }

    // Корректная обработка аппаратного состояния Bus-Off (без ложных срабатываний)
    esp_err_t err = twai_get_status_info(&status_info);
    if(err == ESP_OK && status_info.state == TWAI_STATE_BUS_OFF){
      DEBUG_PRINTLN("CAN: DETECTED BUS OFF. TRYING TO RECOVER -> REINSTALLING");
      vTaskSuspend(canReceiveTaskHandle);
      vTaskSuspend(canTransmitTaskHandle);
      vTaskSuspend(canProcessTaskHandle);
      vTaskSuspend(canDisplayTaskHandle);
      vTaskSuspend(canWatchdogTaskHandle);

      xQueueReset(canTxQueue);
      xQueueReset(canRxQueue);
      clearFlag(CAN_MessageReady | CAN_prevTxFail | CAN_abortMultiPacket | CAN_flowCtlFail);
      while(ulTaskNotifyTake(pdTRUE, 0) > 0);

      twai_stop();
      if(twai_driver_uninstall() == ESP_OK){
        DEBUG_PRINTLN("CAN: TWAI DRIVER UNINSTALL OK");
      } else {
        DEBUG_PRINTLN("CAN: TWAI DRIVER UNINSTALL FAIL!!! Rebooting...");
        vTaskDelay(pdMS_TO_TICKS(100));
        ESP.restart();
      }
      vTaskDelay(pdMS_TO_TICKS(100));
      if(!twai_init()){
        ESP.restart();
      }
      vTaskDelay(pdMS_TO_TICKS(100));
      vTaskResume(canReceiveTaskHandle);
      vTaskResume(canTransmitTaskHandle);
      vTaskResume(canProcessTaskHandle);
      vTaskResume(canDisplayTaskHandle);
      vTaskResume(canWatchdogTaskHandle);
    }

    A2DP_EventHandler();
    vTaskDelay(10);
  }
}

// ============================================================================
// loop() — Главный цикл Arduino (фоновый холостой ход)
// ============================================================================
void loop(){
  vTaskDelay(pdMS_TO_TICKS(1000));
}
