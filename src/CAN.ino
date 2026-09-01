/*
 * CAN.ino — CAN bus communication layer using the ESP-IDF TWAI driver
 *
 * Project: EHU32 (Opel MS-CAN Bluetooth Audio Gateway)
 * Original Author: PNKP237 — https://github.com/PNKP237/EHU32
 * Author & Maintainer: WinD52 — https://github.com/WinD52/EHU32
 *
 * Modern Platform Baseline: ESP-IDF 5.3 / Arduino Core 3.1
 *
 * This file covers every aspect of EHU32's CAN (MS-CAN, 95.238 kbit/s) interaction:
 *   - TWAI driver initialisation (twai_init)
 *   - Inbound frame reception, filtering and routing (canReceiveTask)
 *   - Inbound frame decoding and business logic (canProcessTask)
 *   - Outbound frame transmission with alert monitoring (canTransmitTask)
 *   - ISO 15765-2 (DoCAN) multi-packet display update (canDisplayTask, prepareMultiPacket)
 *   - Air-conditioning panel macro playback (canAirConMacroTask)
 *   - "Aux" string detection in the display data stream (canMessageDecoder)
 *   - Measurement block request helpers (requestMeasurementBlocks, requestCoolantTemperature)
 *   - Radio button long-press action handlers (canActionEhuButton0–9, Left, Right)
 */

#include "driver/twai.h"
#include "config.h"

// Межкадровый интервал ISO 15765-2 (STmin в мс), запрашиваемый дисплеем
volatile uint8_t canISO_frameSpacing = 0;

// ============================================================================
// Статические структуры сообщений шины CAN (MS-CAN 95.238 кбит/с)
// ============================================================================
const twai_message_t  simulate_scroll_up = { .identifier = CAN_ID_SWC_SCROLL, .data_length_code = 3, .data = {0x08, 0x6A, 0x01}},
                      simulate_scroll_down = { .identifier = CAN_ID_SWC_SCROLL, .data_length_code = 3, .data = {0x08, 0x6A, 0xFF}},
                      simulate_scroll_press = { .identifier = CAN_ID_SWC_BUTTON, .data_length_code = 3, .data = {0x01, 0x84, 0x0}},
                      simulate_scroll_release = { .identifier = CAN_ID_SWC_BUTTON, .data_length_code = 3, .data = {0x0, 0x84, 0x02}},
                      Msg_ACmacro_down = { .identifier = CAN_ID_AC_BUTTON, .data_length_code = 3, .data = {0x08, 0x16, 0x01}},
                      Msg_ACmacro_up = { .identifier = CAN_ID_AC_BUTTON, .data_length_code = 3, .data = {0x08, 0x16, 0xFF}},
                      Msg_ACmacro_press = { .identifier = CAN_ID_AC_BUTTON, .data_length_code = 3, .data = {0x01, 0x17, 0x0}},
                      Msg_ACmacro_release = { .identifier = CAN_ID_AC_BUTTON, .data_length_code = 3, .data = {0x0, 0x17, 0x02}},
                      Msg_MeasurementRequestDIS = { .identifier = CAN_ID_DIS_REQUEST, .data_length_code = 7, .data = {0x06, 0xAA, 0x01, 0x01, 0x0B, 0x0E, 0x13}},
                      Msg_MeasurementRequestECC = { .identifier = CAN_ID_ECC_REQUEST, .data_length_code = 7, .data = {0x06, 0xAA, 0x01, 0x01, 0x07, 0x10, 0x11}},
                      Msg_VoltageRequestDIS = { .identifier = CAN_ID_DIS_REQUEST, .data_length_code = 5, .data = {0x04, 0xAA, 0x01, 0x01, 0x13}},
                      Msg_CoolantRequestDIS = { .identifier = CAN_ID_DIS_REQUEST, .data_length_code = 5, .data = {0x04, 0xAA, 0x01, 0x01, 0x0B}},
                      Msg_CoolantRequestECC = { .identifier = CAN_ID_ECC_REQUEST, .data_length_code = 5, .data = {0x04, 0xAA, 0x01, 0x01, 0x10}};

twai_message_t  Msg_PreventDisplayUpdate = { .identifier = CAN_ID_DISPLAY_WRITE, .data_length_code = 8, .data = {0x30, 0x0, 0x7F, 0, 0, 0, 0, 0}},
                Msg_AbortTransmission = { .identifier = CAN_ID_DISPLAY_WRITE, .data_length_code = 8, .data = {0x32, 0x0, 0, 0, 0, 0, 0, 0}};

void a2dp_play();
void a2dp_pause();
void a2dp_next();
void a2dp_previous();
void a2dp_reconnect();

void canActionEhuButton0(bool btn_state, unsigned int btn_ms_held);
void canActionEhuButton1(bool btn_state, unsigned int btn_ms_held);
void canActionEhuButton2(bool btn_state, unsigned int btn_ms_held);
void canActionEhuButton3(bool btn_state, unsigned int btn_ms_held);
void canActionEhuButton4(bool btn_state, unsigned int btn_ms_held);
void canActionEhuButton5(bool btn_state, unsigned int btn_ms_held);
void canActionEhuButton6(bool btn_state, unsigned int btn_ms_held);
void canActionEhuButton7(bool btn_state, unsigned int btn_ms_held);
void canActionEhuButton8(bool btn_state, unsigned int btn_ms_held);
void canActionEhuButton9(bool btn_state, unsigned int btn_ms_held);
void canActionEhuButtonLeft(bool btn_state, unsigned int btn_ms_held);
void canActionEhuButtonRight(bool btn_state, unsigned int btn_ms_held);

// ============================================================================
// twai_init — Инициализация драйвера TWAI на скорости 95.238 кбит/с
// ============================================================================
bool twai_init(){
  twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(CAN_TX_PIN, CAN_RX_PIN, TWAI_MODE_NORMAL);
  g_config.rx_queue_len = 40;
  g_config.tx_queue_len = 5;
  g_config.intr_flags = 0; // Совместимо с ESP-IDF 5.x
  twai_timing_config_t t_config = {.brp = 42, .tseg_1 = 15, .tseg_2 = 4, .sjw = 3, .triple_sampling = false};
  twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();
  DEBUG_PRINT("\nCAN/TWAI SETUP => "); 
  
  esp_err_t res = twai_driver_install(&g_config, &t_config, &f_config);
  if(res != ESP_OK) {
      DEBUG_PRINTF("DRV_INST: FAIL (%s) ", esp_err_to_name(res));
      return false;
  }
  DEBUG_PRINT("DRV_INSTALL: OK ");

  res = twai_start();
  if (res != ESP_OK) {
      DEBUG_PRINTF("DRV_START: FAIL (%s) ", esp_err_to_name(res));
      twai_driver_uninstall();
      return false;
  }
  DEBUG_PRINT("DRV_START: OK ");

  uint32_t alerts_to_enable = TWAI_ALERT_TX_SUCCESS;
  twai_reconfigure_alerts(alerts_to_enable, NULL);
  DEBUG_PRINTLN("ALERTS: OK \n");
  return true;
}

// ============================================================================
// canReceiveTask — Прием, фильтрация и мгновенная блокировка кадров радио (Core 1)
// ============================================================================
void canReceiveTask(void *pvParameters){
  static twai_message_t Recvd_CAN_MSG, DummyFirstFrame = { .identifier = CAN_ID_RADIO_DISPLAY, .data_length_code = 8, .data = {0x10, 0xA7, 0x50, 0x00, 0xA4, 0x03, 0x10, 0x13}};
  bool allowDisplayBlocking = 0, firstAckReceived = 0, overwriteAttemped = 0;
  uint32_t flowCtlUsed = (displayMsgIdentifier - 0x400);
  Msg_PreventDisplayUpdate.extd = 0; Msg_PreventDisplayUpdate.ss = 1; Msg_PreventDisplayUpdate.self = 0; Msg_PreventDisplayUpdate.rtr = 0;
  Msg_AbortTransmission.extd = 0; Msg_AbortTransmission.ss = 1; Msg_AbortTransmission.self = 0; Msg_AbortTransmission.rtr = 0;
  while(1){
    allowDisplayBlocking = checkFlag(CAN_allowAutoRefresh);
    if(twai_receive(&Recvd_CAN_MSG, portMAX_DELAY) == ESP_OK){
      switch(Recvd_CAN_MSG.identifier){
        case CAN_ID_RADIO_DISPLAY: {
          if(disp_mode != -1){
            if(Recvd_CAN_MSG.data_length_code >= 8 && Recvd_CAN_MSG.data[0] == 0x10 && (Recvd_CAN_MSG.data[2] == 0x40 || Recvd_CAN_MSG.data[2] == 0xC0 || (Recvd_CAN_MSG.data[2] == 0x50 && Recvd_CAN_MSG.data[1] == 0x4A)) && Recvd_CAN_MSG.data[5] == 0x03 && (disp_mode != 0 || allowDisplayBlocking)){
              esp_err_t tx_res = twai_transmit(&Msg_PreventDisplayUpdate, pdMS_TO_TICKS(30));
              if(tx_res == ESP_OK){
                clearFlag(CAN_flowCtlFail);
                DEBUG_PRINTLN("CAN: Blocked successfully");
              } else {
                setFlag(CAN_flowCtlFail);
                DEBUG_PRINTLN("CAN: Blocking failed!");
              }
              overwriteAttemped = 1;
              if(eTaskGetState(canDisplayTaskHandle) != eSuspended){
                if(Recvd_CAN_MSG.data[0] == 0x10) setFlag(CAN_abortMultiPacket);
              }
              vTaskResume(canDisplayTaskHandle);
            }
          }
        } // Intentional fall-through
        case CAN_ID_SWC_SCROLL:
        case CAN_ID_SWC_BUTTON:
        case CAN_ID_AC_BUTTON:
        case CAN_ID_RADIO_POWER:
        case CAN_ID_DIS_RESPONSE:
        case CAN_ID_ECC_RESPONSE:
        case CAN_ID_RESERVED_4E8:
        case CAN_ID_ECC_PRESENCE:
          xQueueSend(canRxQueue, &Recvd_CAN_MSG, portMAX_DELAY);
          break;
        case CAN_ID_DISPLAY_WRITE:{
          if(flowCtlUsed == CAN_ID_DISPLAY_WRITE){
            if(firstAckReceived || !overwriteAttemped){
              waitForFlag(CAN_MessageReady, pdMS_TO_TICKS(20));
              if(Recvd_CAN_MSG.data_length_code >= 1 && Recvd_CAN_MSG.data[0] == 0x30){
                xTaskNotifyGive(canDisplayTaskHandle);
                xQueueSend(canRxQueue, &Recvd_CAN_MSG, portMAX_DELAY);
                if(firstAckReceived) firstAckReceived = 0;
                if(overwriteAttemped) overwriteAttemped = 0;
              }
            } else {
              firstAckReceived = 1;
            }
          }
          if(overwriteAttemped && flowCtlUsed != CAN_ID_DISPLAY_WRITE){
            twai_transmit(&DummyFirstFrame, pdMS_TO_TICKS(100));
            DEBUG_PRINTLN("CAN: Attempting to invalidate radio's screen call...");
            overwriteAttemped = 0;
          }
          break;
        }
        default: break;
      }
      if(Recvd_CAN_MSG.identifier == flowCtlUsed && Recvd_CAN_MSG.identifier != CAN_ID_DISPLAY_WRITE && Recvd_CAN_MSG.data[0] == 0x30){
        xTaskNotifyGive(canDisplayTaskHandle);
      }
    }
  }
}

// ============================================================================
// canProcessTask — Обработка кадров из очереди, логика кнопок и датчиков (Core 0)
// ============================================================================
void canProcessTask(void *pvParameters){
  static twai_message_t RxMsg;
  bool badVoltage_VectraC_bypass = getPreferencesBool("vectra");
  unsigned long millis_EccKnobPressed = 0;
  
  while(1){
    xQueueReceive(canRxQueue, &RxMsg, portMAX_DELAY);
    
    // Входной санитайзер
    if (RxMsg.extd || RxMsg.rtr) continue;

    switch(RxMsg.identifier){
      case CAN_ID_SWC_SCROLL: {
        if (RxMsg.data_length_code < 3) break;
        bool btn_state = RxMsg.data[0];
        unsigned int btn_ms_held = (RxMsg.data[2] * 100);
        switch(RxMsg.data[1]){
          case 0x30:  canActionEhuButton0(btn_state, btn_ms_held); break;
          case 0x31:  canActionEhuButton1(btn_state, btn_ms_held); break;
          case 0x32:  canActionEhuButton2(btn_state, btn_ms_held); break;
          case 0x33:  canActionEhuButton3(btn_state, btn_ms_held); break;
          case 0x34:  canActionEhuButton4(btn_state, btn_ms_held); break;
          case 0x35:  canActionEhuButton5(btn_state, btn_ms_held); break;
          case 0x36:  canActionEhuButton6(btn_state, btn_ms_held); break;
          case 0x37:  canActionEhuButton7(btn_state, btn_ms_held); break;
          case 0x38:  canActionEhuButton8(btn_state, btn_ms_held); break;
          case 0x39:  canActionEhuButton9(btn_state, btn_ms_held); break;
          case 0x6D:  canActionEhuButtonLeft(btn_state, btn_ms_held); break;
          case 0x6C:  canActionEhuButtonRight(btn_state, btn_ms_held); break;
          default: break;
        }
        break;
      }
      case CAN_ID_SWC_BUTTON: {
        if (RxMsg.data_length_code < 3) break;
        uint8_t state = RxMsg.data[0];
        uint8_t btn   = RxMsg.data[1];
        uint8_t hold  = RxMsg.data[2];

        // ============================================================
        // КНОПКА OK (0x84) — 100% ОРИГИНАЛЬНЫЙ КОД WinD52
        // ============================================================
        if (btn == 0x84) {
          static bool triggered = false;
          if (state == 0x01 && hold >= 4 && !triggered) {
            triggered = true;
            if (disp_mode == 0) {
              disp_mode = 1;
              setFlag(disp_mode_changed);
              setFlag(DIS_forceUpdate);
              DEBUG_PRINTLN("DISP_MODE: Switching to vehicle data...");
            } else {
              disp_mode = 0;
              setFlag(DIS_forceUpdate);
              DEBUG_PRINTLN("DISP_MODE: Switching to audio metadata");
            }
          }
          if (state == 0x00) {
            triggered = false;
          }
          break;
        }

        // ============================================================
        // ОСТАЛЬНЫЕ КНОПКИ (0x81, 0x91, 0x92) — 100% ОРИГИНАЛ WinD52
        // ============================================================
        if (checkFlag(flag_bt_connected) && state == 0x00 && checkFlag(CAN_allowAutoRefresh)) {
          switch (btn) {
            case 0x81: {
              if (!vehicle_UHP_present) {
                if (checkFlag(bt_audio_playing)) a2dp_pause();
                else a2dp_play();
              }
              break;
            }
            case 0x91: {
              a2dp_next();
              break;
            }
            case 0x92: {
              a2dp_previous();
              break;
            }
            default: break;
          }
        }
        break;
      }
      case CAN_ID_AC_BUTTON: {
        if (RxMsg.data_length_code < 3) break;
        if(eTaskGetState(canAirConMacroTaskHandle) == eSuspended){
          if(RxMsg.data[0] == 0x01 && RxMsg.data[1] == 0x17 && RxMsg.data[2] == 0x0){
            millis_EccKnobPressed = millis();
          } else {
            if(RxMsg.data[0] == 0x0 && RxMsg.data[1] == 0x17 && (RxMsg.data[2] == 0x0 || RxMsg.data[2] >= 0x05)){
              if(RxMsg.data[2] >= 0x05){
                vTaskResume(canAirConMacroTaskHandle);
              } else {
                if(millis() - millis_EccKnobPressed >= 400){
                  vTaskResume(canAirConMacroTaskHandle);
                }
              }
            }
          }
        }
        break;
      }
      case CAN_ID_DISPLAY_WRITE: {
        if (RxMsg.data_length_code < 3) break;
        if(RxMsg.data[2] != 0 && canISO_frameSpacing != RxMsg.data[2]) canISO_frameSpacing = RxMsg.data[2];
        break;
      }
      case CAN_ID_RADIO_POWER: {
        if (RxMsg.data_length_code < 4) break;
        if(checkFlag(a2dp_started) && RxMsg.data[3] == 0x18){
          a2dp_shutdown();
        }
        break;
      }
      case CAN_ID_DIS_RESPONSE: {
          if (RxMsg.data_length_code < 7) break;
          if(disp_mode == 1 || disp_mode == 2){
            if(xSemaphoreTake(BufferSemaphore, pdMS_TO_TICKS(10)) == pdTRUE){
              DEBUG_PRINT("CAN: Got measurements from DIS: ");
              switch(RxMsg.data[0]){
                case 0x0B:  {
                  DEBUG_PRINT("coolant\n");
                  int CAN_data_coolant = RxMsg.data[5] - 40;
                  snprintf(voltage_buffer, sizeof(voltage_buffer), " ");
                  snprintf(coolant_buffer, sizeof(coolant_buffer), "Coolant: %d%c%cC   ", CAN_data_coolant, 0xC2, 0xB0);
                  setFlag(CAN_coolant_recvd);
                  break;
                }
                case 0x0E: {
                  DEBUG_PRINT("speed\n");
                  int CAN_data_speed = (RxMsg.data[2] << 8 | RxMsg.data[3]);
                  CAN_data_speed /= 128;
                  snprintf(speed_buffer, sizeof(speed_buffer), "%d km/h    ", CAN_data_speed);
                  setFlag(CAN_speed_recvd);
                  break;
                }
                case 0x13: {
                  DEBUG_PRINT("battery voltage\n");
                  float CAN_data_voltage = RxMsg.data[6];
                  CAN_data_voltage /= 10;
                  snprintf(voltage_buffer, sizeof(voltage_buffer), "Voltage: %.1f V  ", CAN_data_voltage);
                  setFlag(CAN_voltage_recvd);
                  break;
                }
                default:    break;
              }
              if(checkFlag(CAN_voltage_recvd) && checkFlag(CAN_coolant_recvd) && checkFlag(CAN_speed_recvd)){
                clearFlag(CAN_voltage_recvd);
                clearFlag(CAN_coolant_recvd);
                clearFlag(CAN_speed_recvd);
                setFlag(CAN_new_dataSet_recvd);
              }
              xSemaphoreGive(BufferSemaphore);
            }
          }
        break;
      }
      case CAN_ID_ECC_RESPONSE: {
          if (RxMsg.data_length_code < 5) break;
          if(disp_mode == 1 || disp_mode == 2){
            if(xSemaphoreTake(BufferSemaphore, pdMS_TO_TICKS(10)) == pdTRUE){
              DEBUG_PRINT("CAN: Got measurements from ECC: ");
              switch(RxMsg.data[0]){
                case 0x07:  {
                  if(!badVoltage_VectraC_bypass){
                    float CAN_data_voltage = RxMsg.data[2];
                    CAN_data_voltage /= 10;
                    if(CAN_data_voltage > 9 && CAN_data_voltage < 16){
                      snprintf(voltage_buffer, sizeof(voltage_buffer), "Voltage: %.1f V  ", CAN_data_voltage);
                    } else {
                      badVoltage_VectraC_bypass = 1;
                      setPreferencesBool("vectra", 1);
                    }
                    setFlag(CAN_voltage_recvd);
                    DEBUG_PRINT("battery voltage\n");
                  } else {
                    xQueueSend(canTxQueue, &Msg_VoltageRequestDIS, pdMS_TO_TICKS(100));
                  }
                  break;
                }
                case 0x10:  {
                  unsigned short raw_coolant = (RxMsg.data[3] << 8 | RxMsg.data[4]);
                  float CAN_data_coolant = raw_coolant;
                  CAN_data_coolant /= 10;
                  snprintf(coolant_buffer, sizeof(coolant_buffer), "Coolant: %.1f%c%cC   ", CAN_data_coolant, 0xC2, 0xB0);
                  setFlag(CAN_coolant_recvd);
                  DEBUG_PRINT("coolant\n");
                  break;
                }
                case 0x11:  {
                  int CAN_data_rpm = (RxMsg.data[1] << 8 | RxMsg.data[2]);
                  int CAN_data_speed = RxMsg.data[4];
                  snprintf(speed_buffer, sizeof(speed_buffer), "%d km/h %d RPM     ", CAN_data_speed, CAN_data_rpm);
                  setFlag(CAN_speed_recvd);
                  DEBUG_PRINT("speed and RPMs\n");
                  break; 
                }
                default:    break;
              }
              if(checkFlag(CAN_voltage_recvd) && checkFlag(CAN_coolant_recvd) && checkFlag(CAN_speed_recvd)){
                clearFlag(CAN_voltage_recvd);
                clearFlag(CAN_coolant_recvd);
                clearFlag(CAN_speed_recvd);
                setFlag(CAN_new_dataSet_recvd);
              }
              xSemaphoreGive(BufferSemaphore);
            }
          }
        break;
      }
      case CAN_ID_RADIO_DISPLAY: {
        if(!checkFlag(a2dp_started)){
          setFlag(ehu_started);
          disp_mode = 0;
        } else if(checkFlag(a2dp_started) && !checkFlag(ehu_started)){
          a2dp_reconnect(); 
          setFlag(ehu_started);
        }
        
        // Гарантированно будим декодер для подтверждения режима AUX
        if(eTaskGetState(canMessageDecoderTaskHandle) == eSuspended){
          vTaskResume(canMessageDecoderTaskHandle);
        }

        if(disp_mode == 0){
          for(int i = 1; i < RxMsg.data_length_code && i <= 7; i++){
            xQueueSend(canDispQueue, &RxMsg.data[i], portMAX_DELAY);
          }
        }
        xTaskNotifyGive(canWatchdogTaskHandle);
        break;
      }
      case CAN_ID_ECC_PRESENCE: {
        if(!checkFlag(ECC_present)) setFlag(ECC_present);
        break;
      }
      default:    break;
    }
  }
}

// ============================================================================
// canTransmitTask — Асинхронная отправка кадров и контроль алертов (Core 0)
// ============================================================================
void canTransmitTask(void *pvParameters){
  static twai_message_t TxMessage;
  int alert_result;
  while(1){
    xQueueReceive(canTxQueue, &TxMessage, portMAX_DELAY);
    TxMessage.extd = 0;
    TxMessage.rtr = 0;
    TxMessage.ss = 0;
    TxMessage.self = 0;
    if(twai_transmit(&TxMessage, pdMS_TO_TICKS(50)) == ESP_OK) {
    } else {
      setFlag(CAN_prevTxFail);
      if(TxMessage.identifier == displayMsgIdentifier && (TxMessage.data[0] == 0x10 || TxMessage.data[0] == 0x11)) setFlag(CAN_abortMultiPacket);
    }
    alert_result = twai_read_alerts(&alerts_triggered, pdMS_TO_TICKS(10));
    if(alert_result == ESP_OK){
      if(alerts_triggered & TWAI_ALERT_TX_SUCCESS){
        if(TxMessage.identifier == displayMsgIdentifier && (TxMessage.data[0] == 0x10 || TxMessage.data[0] == 0x11)) setFlag(CAN_MessageReady);
      } else {
        DEBUG_PRINTLN("TX:FAIL ");
        setFlag(CAN_prevTxFail);
      }
    } else {
        setFlag(CAN_prevTxFail);
        if(TxMessage.identifier == displayMsgIdentifier && (TxMessage.data[0] == 0x10 || TxMessage.data[0] == 0x11)) setFlag(CAN_abortMultiPacket);
        DEBUG_PRINT("AR:FAIL:");
      if(alert_result == ESP_ERR_INVALID_ARG){
        DEBUG_PRINTLN("INV_ARG");
      }
      if(alert_result == ESP_ERR_INVALID_STATE){
        DEBUG_PRINTLN("INV_STATE");
      }
      if(alert_result == ESP_ERR_TIMEOUT){
        DEBUG_PRINTLN("TIMEOUT");
      }
    }
  }
}

// ============================================================================
// canDisplayTask — Многопакетный протокол DoCAN ISO 15765-2 на дисплей (Core 1)
// ============================================================================
void canDisplayTask(void *pvParameters){
  static twai_message_t MsgToTx;
  MsgToTx.identifier = displayMsgIdentifier;
  MsgToTx.data_length_code = 8;
  bool retryTx = 0;

  // 1. Мгновенная самозаморозка при создании задачи (исключает фальстарт в FreeRTOS SMP)
  vTaskSuspend(NULL);

  while(1){
    retryTx = 0;
    if(xSemaphoreTake(CAN_MsgSemaphore, portMAX_DELAY) == pdTRUE){
      if(checkFlag(CAN_flowCtlFail)){
        vTaskDelay(pdMS_TO_TICKS(300));
      }
      clearFlag(CAN_prevTxFail);
      clearFlag(CAN_abortMultiPacket);

      while(ulTaskNotifyTake(pdTRUE, 0) > 0);

      memcpy(MsgToTx.data, CAN_MsgArray[0], 8);
      xQueueSend(canTxQueue, &MsgToTx, portMAX_DELAY);
      DEBUG_PRINTLN("CAN: Now waiting for flow control frame...");
      
      if(ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(500)) > 0){
        DEBUG_PRINTLN("CAN: Got flow control! Sending consecutive frames...");
        for(int i = 1; i < 64 && (CAN_MsgArray[i][0] != 0x00 && !checkFlag(CAN_prevTxFail) && !checkFlag(CAN_abortMultiPacket)); i++){
          memcpy(MsgToTx.data, CAN_MsgArray[i], 8);
          xQueueSend(canTxQueue, &MsgToTx, portMAX_DELAY);
          vTaskDelay(pdMS_TO_TICKS(canISO_frameSpacing));
        }
        clearFlag(CAN_MessageReady);
        if(checkFlag(CAN_prevTxFail) || checkFlag(CAN_abortMultiPacket)){
          retryTx = 1;
          clearFlag(CAN_prevTxFail);
        }
      } else {
        DEBUG_PRINTLN("CAN: Flow control frame has not been received in time, aborting");
        clearFlag(CAN_MessageReady);
        retryTx = 1; // 100% WinD52 эталон
      }
      xSemaphoreGive(CAN_MsgSemaphore);
    }
    if(!retryTx) vTaskSuspend(NULL);
  }
}

// ============================================================================
// canAirConMacroTask — Воспроизведение макроса переключения кондиционера (Core 0)
// ============================================================================
void canAirConMacroTask(void *pvParameters){
  // 1. Мгновенная самозаморозка при создании задачи
  vTaskSuspend(NULL);

  while(1){
    vTaskDelay(pdMS_TO_TICKS(500));
    xQueueSend(canTxQueue, &Msg_ACmacro_down, portMAX_DELAY);
    vTaskDelay(pdMS_TO_TICKS(100));
    xQueueSend(canTxQueue, &Msg_ACmacro_press, portMAX_DELAY);
    vTaskDelay(pdMS_TO_TICKS(100));
    xQueueSend(canTxQueue, &Msg_ACmacro_release, portMAX_DELAY);
    vTaskDelay(pdMS_TO_TICKS(100));
    xQueueSend(canTxQueue, &Msg_ACmacro_up, portMAX_DELAY);
    vTaskDelay(pdMS_TO_TICKS(100));
    xQueueSend(canTxQueue, &Msg_ACmacro_up, portMAX_DELAY);
    vTaskDelay(pdMS_TO_TICKS(100));
    xQueueSend(canTxQueue, &Msg_ACmacro_press, portMAX_DELAY);
    vTaskDelay(pdMS_TO_TICKS(100));
    xQueueSend(canTxQueue, &Msg_ACmacro_release, portMAX_DELAY);
    vTaskSuspend(NULL);
  }
}

// ============================================================================
// canMessageDecoder — Сканер потока дисплея на строку "Aux" и меню SOUND (Core 0)
// ============================================================================
void canMessageDecoder(void *pvParameters){
  uint8_t rxDisplay;
  int currentIndex[6] = {0};
  const char patterns[6][17] = {
    {0, 0x6D, 0, 0x41, 0, 0x75, 0, 0x78},
    {0x46, 0, 0x61, 0, 0x64, 0, 0x65, 0, 0x72},
    {0x42, 0, 0x61, 0, 0x6C, 0, 0x61, 0, 0x6E, 0, 0x63, 0, 0x65},
    {0x42, 0, 0x61, 0, 0x73, 0, 0x73},
    {0x54, 0, 0x72, 0, 0x65, 0, 0x62, 0, 0x6C, 0, 0x65},
    {0x53, 0, 0x6F, 0, 0x75, 0, 0x6E, 0, 0x64, 0, 0x20, 0, 0x4F, 0, 0x66, 0, 0x66}
  };
  const char patternLengths[6] = {8, 9, 13, 7, 11, 17};
  bool patternFound = 0;

  // 1. Мгновенная самозаморозка при создании задачи
  vTaskSuspend(NULL);

  while(1){
    if(xQueueReceive(canDispQueue, &rxDisplay, portMAX_DELAY) == pdTRUE){
      for(int i = 0; i < 6; i++){
          if(rxDisplay == patterns[i][currentIndex[i]]){
            currentIndex[i]++;
            if(currentIndex[i] == patternLengths[i]){
              switch(i){
                case 0:{
                  patternFound = 1;
                  last_millis_aux = millis();
                  DEBUG_PRINTLN("CAN Decode: Found Aux string!");
                  break;
                }
                case 1:
                case 2:
                case 3:
                case 4:
                case 5: {
                  patternFound = 0;
                  clearFlag(CAN_allowAutoRefresh);
                  break;
                }
              }
              for(int j = 0; j < 6; j++){
                  currentIndex[j] = 0;
              }
              break;
            }
          } else {
            currentIndex[i] = 0;
            if (rxDisplay == patterns[i][0]) {
              currentIndex[i] = 1;
            }
          }
      }
    }
    if(checkFlag(CAN_allowAutoRefresh) && !patternFound && (millis() - last_millis_aux >= 6000)){
      clearFlag(CAN_allowAutoRefresh);
      DEBUG_PRINTLN("CAN Decode: Disabling display autorefresh...");
    } else {
      if(patternFound && !checkFlag(CAN_allowAutoRefresh)){
        setFlag(CAN_allowAutoRefresh);
        setFlag(DIS_forceUpdate);
        DEBUG_PRINTLN("CAN Decode: Enabling display autorefresh...");
      }
      patternFound = 0;
    }
    vTaskDelay(pdMS_TO_TICKS(2));
  }
}

// ============================================================================
// prepareMultiPacket — Нарезка буфера UTF-16 BE на 8-байтовые кадры DoCAN
// ============================================================================
void prepareMultiPacket(int bytesProcessed, char* buffer_to_read){
  if (bytesProcessed <= 0) return;
  if (bytesProcessed > 254) bytesProcessed = 254;
  // Очищаем весь массив 1024 байта нулями перед упаковкой:
  memset(CAN_MsgArray, 0, sizeof(CAN_MsgArray)); 
  // Нарезаем свежие данные...
  int packetCount = bytesProcessed / 7, bytesToProcess = bytesProcessed % 7;
  unsigned char frameIndex = 0x20;
  for(int i = 0; i < packetCount; i++){
    CAN_MsgArray[i][0] = frameIndex;
    frameIndex = (frameIndex == 0x2F) ? 0x20 : frameIndex + 1;
    memcpy(&CAN_MsgArray[i][1], &buffer_to_read[i * 7], 7);
  }
  
  CAN_MsgArray[0][0] = 0x10; // First Frame PCI

  if(bytesToProcess > 0){
    CAN_MsgArray[packetCount][0] = frameIndex;
    memcpy(&CAN_MsgArray[packetCount][1], &buffer_to_read[packetCount * 7], bytesToProcess);
    packetCount++;
  }
  // Терминатор строго в конце актуального блока
  CAN_MsgArray[packetCount][0] = 0x00;
}

// ============================================================================
// Вспомогательные функции запроса диагностических блоков KWP-2000
// ============================================================================
void requestMeasurementBlocks(){
  DEBUG_PRINT("CAN: Requesting measurements from ");
  if(checkFlag(ECC_present)){
    DEBUG_PRINTLN("climate control...");
    xQueueSend(canTxQueue, &Msg_MeasurementRequestECC, portMAX_DELAY);
  } else {
    DEBUG_PRINTLN("display...");
    xQueueSend(canTxQueue, &Msg_MeasurementRequestDIS, portMAX_DELAY);
  }
}

void requestCoolantTemperature(){
  DEBUG_PRINT("CAN: Requesting coolant temperature from ");
  if(checkFlag(ECC_present)){
    DEBUG_PRINTLN("climate control...");
    xQueueSend(canTxQueue, &Msg_CoolantRequestECC, portMAX_DELAY);
  } else {
    DEBUG_PRINTLN("display...");
    xQueueSend(canTxQueue, &Msg_CoolantRequestDIS, portMAX_DELAY);
  }
}

// ============================================================================
// Обработчики кнопок панели магнитолы 0-9 и стрелок (CAN ID 0x201)
// ============================================================================
void canActionEhuButton0(bool btn_state, unsigned int btn_ms_held){}
void canActionEhuButton1(bool btn_state, unsigned int btn_ms_held){
  if(disp_mode != 0 && btn_ms_held >= 500){
    disp_mode = 0;
    setFlag(DIS_forceUpdate);
  }
}
void canActionEhuButton2(bool btn_state, unsigned int btn_ms_held){
  if(disp_mode != 1 && btn_ms_held >= 500){
    clearFlag(CAN_new_dataSet_recvd);
    disp_mode = 1;
    setFlag(disp_mode_changed);
    DEBUG_PRINTLN("DISP_MODE: Switching to vehicle data...");
  }
}
void canActionEhuButton3(bool btn_state, unsigned int btn_ms_held){
  if(disp_mode != 2 && btn_ms_held >= 500){
    clearFlag(CAN_new_dataSet_recvd);
    disp_mode = 2;
    setFlag(disp_mode_changed);
    DEBUG_PRINTLN("DISP_MODE: Switching to 1-line coolant...");
  }
}
void canActionEhuButton4(bool btn_state, unsigned int btn_ms_held){}
void canActionEhuButton5(bool btn_state, unsigned int btn_ms_held){}
void canActionEhuButton6(bool btn_state, unsigned int btn_ms_held){}
void canActionEhuButton7(bool btn_state, unsigned int btn_ms_held){}

void canActionEhuButton8(bool btn_state, unsigned int btn_ms_held){
  if(!checkFlag(OTA_begin) && btn_ms_held >= 1000){
    setFlag(OTA_begin);
  } else {
    if(btn_ms_held >= 5000) setFlag(OTA_abort);
  }
}

void canActionEhuButton9(bool btn_state, unsigned int btn_ms_held){
  if(disp_mode != -1 && btn_ms_held >= 500){
    disp_mode = -1;
    DEBUG_PRINTLN("Screen updates disabled");
  }
  if(btn_ms_held >= 5000){
    if(!checkFlag(OTA_begin)){
      vTaskDelay(pdMS_TO_TICKS(1000));
      prefs_clear();
      vTaskDelay(pdMS_TO_TICKS(1000));
      ESP.restart();
    }
  }
}

void canActionEhuButtonLeft(bool btn_state, unsigned int btn_ms_held){
  static bool triggered = false;
  if(btn_ms_held >= 400 && checkFlag(CAN_allowAutoRefresh) && checkFlag(flag_bt_connected)){
    if(!triggered){
      triggered = true;
      a2dp_previous();
      DEBUG_PRINTLN("DISP_MODE: Previous track");
    }
  } else {
    triggered = false;
  }
}

void canActionEhuButtonRight(bool btn_state, unsigned int btn_ms_held){
  static bool triggered = false;
  if(btn_ms_held >= 400 && checkFlag(CAN_allowAutoRefresh) && checkFlag(flag_bt_connected)){
    if(!triggered){
      triggered = true;
      a2dp_next();
      DEBUG_PRINTLN("DISP_MODE: Next track");
    }
  } else {
    triggered = false;
  }
}
