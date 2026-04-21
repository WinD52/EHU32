/*
 * CAN.ino — CAN bus communication layer using the ESP-IDF TWAI driver
 *
 * This file covers every aspect of EHU32's CAN (MS-CAN, 95 kbit/s) interaction:
 *   - TWAI driver initialisation (twai_init)
 *   - Inbound frame reception, filtering and routing (canReceiveTask)
 *   - Inbound frame decoding and business logic (canProcessTask)
 *   - Outbound frame transmission with alert monitoring (canTransmitTask)
 *   - ISO 15765-2 (DoCAN) multi-packet display update (canDisplayTask,
 *     prepareMultiPacket)
 *   - Air-conditioning panel macro playback (canAirConMacroTask)
 *   - "Aux" string detection in the display data stream (canMessageDecoder)
 *   - Measurement block request helpers (requestMeasurementBlocks,
 *     requestCoolantTemperature)
 *   - Radio button long-press action handlers (canActionEhuButton0–9)
 *   - DEBUG-only serial simulation task (CANsimTask)
 *
 * CAN bus speed: ~95.24 kbit/s — the GM/Vauxhall/Opel MS-CAN (Middle Speed CAN)
 * used by all NCDC-generation radios (CD30, CD30MP3, CD40USB, CD70, DVD90).
 *
 * ISO 15765-2 framing (used for all display messages):
 *   First frame (FF)       — byte 0 = 0x10 (or 0x11 for >255 byte payloads)
 *   Consecutive frame (CF) — byte 0 = 0x21 … 0x2F, then 0x20, 0x21, …
 *   Flow control (FC)      — byte 0 = 0x30 (Continue to Send) from display
 *
 * CAN ID reference (JJToB MS-CAN database):
 *   0x201  — Radio panel button events (scroll wheel)
 *   0x206  — Steering wheel button events
 *   0x208  — AC panel button events
 *   0x246  — DIS (Driver Information System / display module) diagnostic channel
 *   0x248  — ECC (Electronic Climate Control) diagnostic channel
 *   0x2C1  — Flow-control / abort frame from display to radio (response to 0x6C1)
 *   0x501  — Radio/head unit general status messages
 *   0x546  — DIS measurement block response
 *   0x548  — ECC measurement block response
 *   0x6C1  — Radio → display ISO 15765-2 transmissions (stock display source)
 *   0x6C8  — ECC → display transmissions (confirms ECC presence)
 *   0x6C0–0x6CF — Used for EHU32's own display transmissions (one unused ID
 *                  is selected automatically at first boot)
 *
 * Dependencies:
 *   - ESP-IDF TWAI driver (driver/twai.h, bundled with Arduino ESP32 core)
 *   - Global variables and RTOS handles declared in EHU32.ino
 */

#include "driver/twai.h"
#include "config.h"
void OTAhandleTask(void* pvParameters);

/* canISO_frameSpacing — inter-frame delay (in ms) between consecutive ISO 15765-2
 * frames, requested by the receiving node inside its flow-control frame (byte 2 =
 * STmin).  Most display units request 0 ms (no delay) or 2 ms.  Updated live in
 * canProcessTask when a 0x2C1 flow-control response is received. */
volatile uint8_t canISO_frameSpacing=0;

/* ──────────────────────────── Static CAN Message Definitions ────────────────────
 *
 * Scroll wheel / radio panel (CAN_ID_SWC_SCROLL = 0x201, 3 bytes):
 *   Byte 0: button state (0x08 = scroll event, 0x01 = pressed, 0x00 = released)
 *   Byte 1: button identifier (0x6A = scroll wheel)
 *   Byte 2: step value (0x01 = one step up / clockwise, 0xFF = one step down)
 *
 * Scroll wheel button press (CAN_ID_SWC_BUTTON = 0x206, 3 bytes):
 *   Byte 0: state (0x01 = pressed, 0x00 = released)
 *   Byte 1: button identifier (0x84 = scroll wheel centre button)
 *   Byte 2: hold counter (0x00 = initial press, 0x02 = released)
 *
 * AC panel knob (CAN_ID_AC_BUTTON = 0x208, 3 bytes):
 *   Byte 0: event type (0x08 = scroll, 0x01 = pressed, 0x00 = released)
 *   Byte 1: button identifier (0x16 = temp knob scroll, 0x17 = temp knob press)
 *   Byte 2: step value (0x01 = up, 0xFF = down, 0x00 = press, 0x02 = release)
 *
 * Measurement request to DIS (CAN_ID_DIS_REQUEST = 0x246, up to 7 bytes, KWP-2000 format):
 *   {0x06, 0xAA, 0x01, 0x01, block0, block1, block2}
 *   0xAA = ReadDataByLocalIdentifier service, 0x01 0x01 = parameter group
 *   block IDs: 0x0B=coolant, 0x0E=speed, 0x13=battery voltage
 *
 * Measurement request to ECC (CAN_ID_ECC_REQUEST = 0x248, same KWP-2000 format):
 *   block IDs: 0x07=battery voltage, 0x10=coolant, 0x11=RPM+speed
 *
 * Voltage / coolant single-block requests (CAN_ID_DIS_REQUEST or CAN_ID_ECC_REQUEST, 5 bytes):
 *   {0x04, 0xAA, 0x01, 0x01, blockID}
 * ──────────────────────────────────────────────────────────────────────────────── */
const twai_message_t  simulate_scroll_up={ .identifier=CAN_ID_SWC_SCROLL, .data_length_code=3, .data={0x08, 0x6A, 0x01}},       // scroll wheel: one step up
                      simulate_scroll_down={ .identifier=CAN_ID_SWC_SCROLL, .data_length_code=3, .data={0x08, 0x6A, 0xFF}},     // scroll wheel: one step down
                      simulate_scroll_press={ .identifier=CAN_ID_SWC_BUTTON, .data_length_code=3, .data={0x01, 0x84, 0x0}},    // scroll wheel centre: pressed
                      simulate_scroll_release={ .identifier=CAN_ID_SWC_BUTTON, .data_length_code=3, .data={0x0, 0x84, 0x02}},  // scroll wheel centre: released
                      Msg_ACmacro_down={ .identifier=CAN_ID_AC_BUTTON, .data_length_code=3, .data={0x08, 0x16, 0x01}},        // AC temp knob: one step down (towards cooler)
                      Msg_ACmacro_up={ .identifier=CAN_ID_AC_BUTTON, .data_length_code=3, .data={0x08, 0x16, 0xFF}},          // AC temp knob: one step up (towards warmer)
                      Msg_ACmacro_press={ .identifier=CAN_ID_AC_BUTTON, .data_length_code=3, .data={0x01, 0x17, 0x0}},        // AC temp knob: pressed
                      Msg_ACmacro_release={ .identifier=CAN_ID_AC_BUTTON, .data_length_code=3, .data={0x0, 0x17, 0x02}},      // AC temp knob: released
                      Msg_MeasurementRequestDIS={ .identifier=CAN_ID_DIS_REQUEST, .data_length_code=7, .data={0x06, 0xAA, 0x01, 0x01, 0x0B, 0x0E, 0x13}},  // request coolant + speed + voltage from DIS
                      Msg_MeasurementRequestECC={ .identifier=CAN_ID_ECC_REQUEST, .data_length_code=7, .data={0x06, 0xAA, 0x01, 0x01, 0x07, 0x10, 0x11}},  // request voltage + coolant + RPM/speed from ECC
                      Msg_VoltageRequestDIS={ .identifier=CAN_ID_DIS_REQUEST, .data_length_code=5, .data={0x04, 0xAA, 0x01, 0x01, 0x13}},      // request battery voltage from DIS only
                      Msg_CoolantRequestDIS={ .identifier=CAN_ID_DIS_REQUEST, .data_length_code=5, .data={0x04, 0xAA, 0x01, 0x01, 0x0B}},     // request coolant temperature from DIS only
                      Msg_CoolantRequestECC={ .identifier=CAN_ID_ECC_REQUEST, .data_length_code=5, .data={0x04, 0xAA, 0x01, 0x01, 0x10}};     // request coolant temperature from ECC only

/* Msg_PreventDisplayUpdate — sent to CAN_ID_DISPLAY_WRITE (0x2C1) immediately after
 * the radio's 0x6C1 first frame is detected.  This is a forged ISO 15765-2 flow-control
 * "Continue to Send" frame: {0x30, 0x00, 0x7F, …} where 0x30=CTS, 0x00=no block limit,
 * 0x7F=127 ms minimum inter-frame spacing (maximum allowed by the standard).
 * By replying to the radio's first frame ourselves, we consume the radio's
 * transmission slot and can then transmit EHU32's own first frame in its place.
 *
 * Msg_AbortTransmission — sends {0x32, …} to CAN_ID_DISPLAY_WRITE (0x2C1), which is an
 * ISO 15765-2 "Abort" flow-control value.  Forces the radio to stop its current
 * multi-frame sequence.  Use only as a last resort since some displays react to 0x32
 * with a soft-reset.
 *
 * Note: the .ss (single-shot) flag cannot be set via the designated initialiser
 * because it lives inside a union in twai_message_t.  It is set in canReceiveTask
 * and canTransmitTask at runtime. */
twai_message_t  Msg_PreventDisplayUpdate={ .identifier=CAN_ID_DISPLAY_WRITE, .data_length_code=8, .data={0x30, 0x0, 0x7F, 0, 0, 0, 0, 0}},
                Msg_AbortTransmission={ .identifier=CAN_ID_DISPLAY_WRITE, .data_length_code=8, .data={0x32, 0x0, 0, 0, 0, 0, 0, 0}};     // can have unforseen consequences such as resets! use as last resort

/* twai_init() — initialise the ESP-IDF TWAI (CAN) driver at 95 kbit/s.
 *
 * Pin assignment:   TX = GPIO 5  (to SN65HVD230 TXD pin)
 *                   RX = GPIO 4  (from SN65HVD230 RXD pin, also ext0 wake-up)
 * Bus speed:        95 kbit/s (MS-CAN)
 *   Timing: APB clock = 80 MHz, prescaler brp=42 → tq = 42/80MHz = 0.525 µs
 *   tseg_1=15, tseg_2=4, sjw=3  → bit time = (1+15+4)*0.525 µs ≈ 10.5 µs → 95.24 kbit/s
 * RX queue:         40 frames (hardware TWAI RX FIFO size)
 * TX queue:         5 frames
 * Alert:            TWAI_ALERT_TX_SUCCESS only (used by canTransmitTask to detect
 *                   successful first-frame delivery and signal canDisplayTask)
 * Filter:           accept all frames (software filtering is done in canReceiveTask)
 * ISR priority:     NMI + IRAM for lowest possible latency on RX/TX interrupts
 */
// initializing CAN communication
void twai_init(){
  twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(CAN_TX_PIN, CAN_RX_PIN, TWAI_MODE_NORMAL);         // CAN bus set up
  g_config.rx_queue_len=40;
  g_config.tx_queue_len=5;
  g_config.intr_flags=(ESP_INTR_FLAG_NMI & ESP_INTR_FLAG_IRAM);   // run the TWAI driver at the highest possible priority
  twai_timing_config_t t_config =  {.brp = 42, .tseg_1 = 15, .tseg_2 = 4, .sjw = 3, .triple_sampling = false};    // set CAN prescalers and time quanta for 95kbit
  twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();
    DEBUG_PRINT("\nCAN/TWAI SETUP => "); 
  if(twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK) {
      DEBUG_PRINT("DRV_INSTALL: OK ");
  } else {
      DEBUG_PRINT("DRV_INST: FAIL ");
  }
  if (twai_start() == ESP_OK) {
      DEBUG_PRINT("DRV_START: OK ");
  } else {
      DEBUG_PRINT("DRV_START: FAIL ");
  }
  uint32_t alerts_to_enable=TWAI_ALERT_TX_SUCCESS;
  if(twai_reconfigure_alerts(alerts_to_enable, NULL) == ESP_OK){
      DEBUG_PRINTLN("ALERTS: OK \n");
  } else {
      DEBUG_PRINTLN("ALERTS: FAIL \n");
  }
}

/* canReceiveTask — Core 1, priority 1
 *
 * Purpose: read every TWAI frame from the hardware FIFO and handle
 *          time-critical operations synchronously, then forward everything
 *          else to canProcessTask via canRxQueue.
 *
 * Special case — 0x6C1 (radio → display first frame):
 *   When the radio sends its display-update first frame we must respond with
 *   our own flow-control frame (Msg_PreventDisplayUpdate) within ~1 ms to
 *   "steal" the radio's ISO 15765-2 session.  If CAN_allowAutoRefresh is set
 *   (i.e. "Aux" is currently displayed) and disp_mode is not -1, this task
 *   transmits Msg_PreventDisplayUpdate directly — bypassing canTxQueue to
 *   avoid queueing latency — then resumes canDisplayTask so it can transmit
 *   EHU32's replacement message immediately.
 *
 *   The specific 0x6C1 header patterns intercepted are:
 *     data[0]=0x10 (first frame), data[2]=0x40 (standard update),
 *     data[2]=0xC0 (alternative), or data[1]=0x4A + data[2]=0x50 (CD70/DVD90),
 *     data[5]=0x03 (type marker that identifies a text display update).
 *
 * Fallback — when displayMsgIdentifier is 0x6C1 (stock ID):
 *   The flow-control frame for EHU32's own first frame also arrives on 0x2C1.
 *   This task uses firstAckReceived to skip the radio's flow-control frame and
 *   pass only EHU32's own flow-control frame to canDisplayTask.
 *
 * Filtered IDs forwarded to canRxQueue:
 *   0x201  scroll wheel events        0x206  steering wheel buttons
 *   0x208  AC panel events            0x501  radio status
 *   0x546  DIS measurement response   0x548  ECC measurement response
 *   0x4E8  (reserved, logged only)    0x6C8  ECC presence confirmation
 *   0x6C1  radio display data         (also decoded by canProcessTask)
 */
// this task only reads CAN messages, filters them and enqueues them to be decoded ansynchronously. 0x6C1 is a special case, as the radio message has to be blocked ASAP
void canReceiveTask(void *pvParameters){
  static twai_message_t Recvd_CAN_MSG, DummyFirstFrame={ .identifier=CAN_ID_RADIO_DISPLAY, .data_length_code=8, .data={0x10, 0xA7, 0x50, 0x00, 0xA4, 0x03, 0x10, 0x13}};
  uint32_t alerts_FlowCtl;        // separate buffer for the flow control alerts, prevents race conditions with the other task which also uses alerts
  bool allowDisplayBlocking=0, firstAckReceived=0, overwriteAttemped=0;
  uint32_t flowCtlUsed=(displayMsgIdentifier-0x400);  // set from memory, if using 0x6C0 flow control will be 0x2C0 etc.
  Msg_PreventDisplayUpdate.extd=0; Msg_PreventDisplayUpdate.ss=1; Msg_PreventDisplayUpdate.self=0; Msg_PreventDisplayUpdate.rtr=0;
  Msg_AbortTransmission.extd=0; Msg_AbortTransmission.ss=1; Msg_AbortTransmission.self=0; Msg_AbortTransmission.rtr=0;
  while(1){
    allowDisplayBlocking=checkFlag(CAN_allowAutoRefresh);   // checking earlier improves performance, there's very little time to send that message, otherwise we get error frames (because of the same ID)
    if(twai_receive(&Recvd_CAN_MSG, portMAX_DELAY)==ESP_OK){
      switch(Recvd_CAN_MSG.identifier){
        case CAN_ID_RADIO_DISPLAY: {
          if(disp_mode!=-1){            // don't bother checking the data if there's no need to update the display
            if(Recvd_CAN_MSG.data[0]==0x10 && (Recvd_CAN_MSG.data[2]==0x40 || Recvd_CAN_MSG.data[2]==0xC0 || (Recvd_CAN_MSG.data[2]==0x50 && Recvd_CAN_MSG.data[1]==0x4A)) && Recvd_CAN_MSG.data[5]==0x03 && (disp_mode!=0 || allowDisplayBlocking)){       // another task processes the data since we can't do that here
              twai_transmit(&Msg_PreventDisplayUpdate, pdMS_TO_TICKS(30));  // radio blocking msg has to be transmitted ASAP, which is why we skip the queue
              DEBUG_PRINTLN("CAN: Received display update, trying to block");
              twai_read_alerts(&alerts_FlowCtl, pdMS_TO_TICKS(10));    // read stats to a local alert buffer
              if(alerts_FlowCtl & TWAI_ALERT_TX_SUCCESS){
                clearFlag(CAN_flowCtlFail);
                DEBUG_PRINTLN("CAN: Blocked successfully");
              } else {
                setFlag(CAN_flowCtlFail);                     // lets the display task know that we failed blocking the display TX and as such the display task shall wait
                DEBUG_PRINTLN("CAN: Blocking failed!");
              }
              overwriteAttemped=1;    // if the display message retransmission was intended to mask the radio's message
              if(eTaskGetState(canDisplayTaskHandle)!=eSuspended){
                if(Recvd_CAN_MSG.data[0]==0x10) setFlag(CAN_abortMultiPacket);    // let the transmission task know that the radio has transmissed a new first frame -> any ongoing transmission is no longer valid
              }
              vTaskResume(canDisplayTaskHandle); // only retransmit the msg for audio metadata mode and single line coolant, since these don't update frequently
            }
          }
        }
        case CAN_ID_SWC_SCROLL:
        case CAN_ID_SWC_BUTTON:
        case CAN_ID_AC_BUTTON:
        case CAN_ID_RADIO_POWER:
        case CAN_ID_DIS_RESPONSE:
        case CAN_ID_ECC_RESPONSE:
        case CAN_ID_RESERVED_4E8:
        case CAN_ID_ECC_PRESENCE:
          xQueueSend(canRxQueue, &Recvd_CAN_MSG, portMAX_DELAY);      // queue the message contents to be read at a later time
          break;
        case CAN_ID_DISPLAY_WRITE:{        // this attempts to invalidate the radio's display call with identifier 0x6C1
          if(flowCtlUsed==CAN_ID_DISPLAY_WRITE){       // old/backup logic for radio messages on 0x6C1
            if(firstAckReceived || !overwriteAttemped){          // disregard the first flow control frame meant for the radio unit ONLY if it was a result of retransmission to mask the radio's display update
              waitForFlag(CAN_MessageReady, pdMS_TO_TICKS(20));   // this is blocking a lot of stuff so gotta find a sweet spot for how long to block for
              if(Recvd_CAN_MSG.data[0]==0x30){
                xTaskNotifyGive(canDisplayTaskHandle);          // let the display update task know that the data is ready to be transmitted
                xQueueSend(canRxQueue, &Recvd_CAN_MSG, portMAX_DELAY);
                if(firstAckReceived) firstAckReceived=0;             // reset it to be ready for the next one
                if(overwriteAttemped) overwriteAttemped=0;
              }
            } else {
              firstAckReceived=1;   // flow control not meant for EHU32, set the switch and wait for the second one
            }
          }
          if(overwriteAttemped && flowCtlUsed!=CAN_ID_DISPLAY_WRITE){
            twai_transmit(&DummyFirstFrame, pdMS_TO_TICKS(100));  // transmit a dummy frame ASAP to invalidate previous display call
            DEBUG_PRINTLN("CAN: Attempting to invalidate radio's screen call...");
            overwriteAttemped=0;
          }
          break;
        }
        default: break;
      }
      if(Recvd_CAN_MSG.identifier==flowCtlUsed && Recvd_CAN_MSG.identifier!=CAN_ID_DISPLAY_WRITE && Recvd_CAN_MSG.data[0]==0x30){ // can't be a switch case because it might be dynamic
        xTaskNotifyGive(canDisplayTaskHandle);
      }
    }
  }
}

/* canProcessTask — Core 0, priority 2
 *
 * Purpose: decode all incoming CAN frames that have been pre-filtered by
 *          canReceiveTask and queued in canRxQueue.  Runs at priority 2 so it
 *          can pre-empt canTransmitTask (priority 1) when new data arrives.
 *
 * Handled CAN IDs:
 *
 *   0x201 — Radio panel button event (scroll wheel / numeric buttons)
 *     data[0]: button state bitmask
 *     data[1]: button ID (0x30–0x39 → buttons 0–9, 0x6A → scroll wheel)
 *     data[2]: hold time counter (×100 ms) or scroll step
 *     → Dispatches to canActionEhuButton0–9()
 *
 *   0x206 — Steering wheel remote control button event (released state only)
 *     data[0]: 0x00 = button released
 *     data[1]: button ID
 *       0x81 = left thumb button (play/pause toggle, disabled if UHP present)
 *       0x91 = right upper button (next track)
 *       0x92 = right lower button (previous track)
 *     Only active when Bluetooth is connected AND "Aux" is displayed.
 *
 *   0x208 — AC panel (ECC) knob event
 *     data[0/1/2]: press/scroll/release event for the AC temperature knob.
 *     A long-press (≥400 ms or byte 2 ≥ 0x05) resumes canAirConMacroTask.
 *     Vectra C ECCs report a clean hold counter in byte 2 (0x05+); older
 *     Astra/Corsa/Zafira ECCs only send press+immediate release so elapsed
 *     time is measured with millis() instead.
 *
 *   0x2C1 — Display flow-control / frame spacing update
 *     data[2]: STmin (minimum inter-frame spacing in ms) requested by the
 *              display module.  canISO_frameSpacing is updated if the value
 *              changed, which canDisplayTask uses in its inter-frame delay.
 *
 *   0x501 — Radio general status
 *     data[3] = 0x18 → radio is shutting down → trigger a2dp_shutdown().
 *
 *   0x546 — DIS (display module) measurement block response (KWP-2000)
 *     data[0]: block ID
 *       0x0B — coolant temperature: data[5] - 40 = °C
 *       0x0E — vehicle speed:       (data[2]<<8 | data[3]) / 128 = km/h
 *       0x13 — battery voltage:     data[6] / 10 = V
 *     Once all three flags (CAN_voltage_recvd, CAN_coolant_recvd,
 *     CAN_speed_recvd) are set, CAN_new_dataSet_recvd is raised.
 *
 *   0x548 — ECC (climate control) measurement block response (KWP-2000)
 *     data[0]: block ID
 *       0x07 — battery voltage: data[2] / 10 = V
 *              If the value is outside 9–16 V, the Vectra-C erratic-voltage
 *              bypass is activated and subsequent voltage reads fall back to DIS.
 *       0x10 — coolant temperature: (data[3]<<8 | data[4]) / 10 = °C
 *       0x11 — RPM + speed: (data[1]<<8 | data[2]) = RPM, data[4] = km/h
 *
 *   0x6C1 — Radio → display first frame / consecutive frame
 *     Used for two purposes:
 *     a) First boot detection (ehu_started / a2dp reconnect trigger)
 *     b) In audio metadata mode (disp_mode==0): raw payload bytes are
 *        forwarded to canDispQueue for Aux pattern detection.
 *     Also resets the watchdog timer via xTaskNotifyGive(canWatchdogTaskHandle).
 *
 *   0x6C8 — ECC module heartbeat
 *     Sets ECC_present flag to indicate ECC is available for measurements.
 */
// this task processes filtered CAN frames read from canRxQueue
void canProcessTask(void *pvParameters){
  static twai_message_t RxMsg;
  bool badVoltage_VectraC_bypass=getPreferencesBool("vectra"); // read from the preferences to check if the car is a vectra
  unsigned long millis_EccKnobPressed;
  while(1){
    xQueueReceive(canRxQueue, &RxMsg, portMAX_DELAY);     // receives data from the internal queue
    switch(RxMsg.identifier){
      case CAN_ID_SWC_SCROLL: {                                         // radio button decoder
        bool btn_state=RxMsg.data[0];
        unsigned int btn_ms_held=(RxMsg.data[2]*100);
        switch(RxMsg.data[1]){
          case 0x30:  canActionEhuButton0(btn_state, btn_ms_held);      // CD30 has no '0' button!
                      break;
          case 0x31:  canActionEhuButton1(btn_state, btn_ms_held);
                      break;
          case 0x32:  canActionEhuButton2(btn_state, btn_ms_held);
                      break;
          case 0x33:  canActionEhuButton3(btn_state, btn_ms_held);
                      break;
          case 0x34:  canActionEhuButton4(btn_state, btn_ms_held);
                      break;
          case 0x35:  canActionEhuButton5(btn_state, btn_ms_held);
                      break;
          case 0x36:  canActionEhuButton6(btn_state, btn_ms_held);
                      break;
          case 0x37:  canActionEhuButton7(btn_state, btn_ms_held);
                      break;
          case 0x38:  canActionEhuButton8(btn_state, btn_ms_held);
                      break;
          case 0x39:  canActionEhuButton9(btn_state, btn_ms_held);
                      break;
          case 0x6D:  canActionEhuButtonLeft(btn_state, btn_ms_held);
                      break;
          case 0x6C:  canActionEhuButtonRight(btn_state, btn_ms_held);
                      break;            
          default: break;
        }
        break;
      }
      case CAN_ID_SWC_BUTTON: {
  uint8_t state = RxMsg.data[0];
  uint8_t btn   = RxMsg.data[1];
  uint8_t hold  = RxMsg.data[2];

  // ============================================================
  // КНОПКА OK (0x84) — ЛЕВАЯ КРУТИЛКА НА РУЛЕ (НАЖАТИЕ)
  // Удержание: срабатывает при hold >= 4 во время state == 0x01
  // ============================================================
  if (btn == 0x84) {
    static bool triggered = false;
    
    if (state == 0x01 && hold >= 4 && !triggered && checkFlag(CAN_allowAutoRefresh)) {
      triggered = true;
      
      // Циклическое переключение: только 0 и 1
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
      triggered = false;   // сброс при отпускании
    }
    break;
  }

  // ============================================================
  // ОСТАЛЬНЫЕ КНОПКИ (0x81, 0x91, 0x92) — БЕЗ ИЗМЕНЕНИЙ
  // ============================================================
  if (checkFlag(bt_connected) && state == 0x00 && checkFlag(CAN_allowAutoRefresh)) {
    switch (btn) {
      case 0x81: {
        if (!vehicle_UHP_present) {
          if (checkFlag(bt_audio_playing)) a2dp_sink.pause();
          else a2dp_sink.play();
        }
        break;
      }
      case 0x91: {
        a2dp_sink.next();
        break;
      }
      case 0x92: {
        a2dp_sink.previous();
        break;
      }
      default: break;
    }
  }
  break;
}
      case CAN_ID_AC_BUTTON: {                               // AC panel button event
        if(eTaskGetState(canAirConMacroTaskHandle)==eSuspended){
          if(RxMsg.data[0]==0x01 && RxMsg.data[1]==0x17 && RxMsg.data[2]==0x0){     // button pressed, start save timestamp
            millis_EccKnobPressed=millis();
          } else {
            if(RxMsg.data[0]==0x0 && RxMsg.data[1]==0x17 && (RxMsg.data[2]==0x0 || RxMsg.data[2]>=0x05)){
              if(RxMsg.data[2]>=0x05){    // properly implemented ECC button counting (such as Vectra C) does not require stupid workarounds
                vTaskResume(canAirConMacroTaskHandle);
              } else {
                if((millis_EccKnobPressed+400)<=millis()){  // late Astra/Corsa/Zafira ECCs don't follow the standard, as these only report the initial press and release (saying it was released after being held for 0ms lol)
                  vTaskResume(canAirConMacroTaskHandle);    // so we count the time on our own, and if the time elapsed 
                }
              }
            }
          }
        } // should be an else here to implement something to stop the macro task, can't be bothered to implement this now
        break;
      }
      case CAN_ID_DISPLAY_WRITE: {
        if(RxMsg.data[2]!=0 && canISO_frameSpacing!=RxMsg.data[2]) canISO_frameSpacing=RxMsg.data[2];            // adjust ISO 15765-2 frame spacing delay only if the receiving node calls for it
        break;
      }
      case CAN_ID_RADIO_POWER: {                                         // CD30MP3 goes to sleep -> disable bluetooth connectivity
        if(checkFlag(a2dp_started) && RxMsg.data[3]==0x18){
          a2dp_shutdown();
        }
        break;
      }
      case CAN_ID_DIS_RESPONSE: {                             // display measurement blocks (used as a fallback or for )
          if(disp_mode==1 || disp_mode==2){
            xSemaphoreTake(BufferSemaphore, portMAX_DELAY);
            DEBUG_PRINT("CAN: Got measurements from DIS: ");
            switch(RxMsg.data[0]){              // measurement block ID -> update data which the message is referencing, I may implement more cases in the future which is why switch is there
              case 0x0B:  {             // 0x0B references coolant temps
                DEBUG_PRINT("coolant\n");
                int CAN_data_coolant=RxMsg.data[5]-40;
                snprintf(voltage_buffer, sizeof(voltage_buffer), " ");
                snprintf(coolant_buffer, sizeof(coolant_buffer), "Coolant: %d%c%cC   ", CAN_data_coolant, 0xC2, 0xB0);
                setFlag(CAN_coolant_recvd);
                break;
              }
              case 0x0E: {
                DEBUG_PRINT("speed\n");
                int CAN_data_speed=(RxMsg.data[2]<<8 | RxMsg.data[3]);
                CAN_data_speed/=128;
                snprintf(speed_buffer, sizeof(speed_buffer), "%d km/h    ", CAN_data_speed);
                setFlag(CAN_speed_recvd);
                break;
              }
              case 0x13: {    // reading voltage from display, research courtesy of @xymetox
                DEBUG_PRINT("battery voltage\n");
                float CAN_data_voltage=RxMsg.data[6];
                CAN_data_voltage/=10;
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
            xSemaphoreGive(BufferSemaphore);  // let the message processing continue
          }
        break;
      }
      case CAN_ID_ECC_RESPONSE: {                             // AC measurement blocks
          if(disp_mode==1 || disp_mode==2) xSemaphoreTake(BufferSemaphore, portMAX_DELAY);    // if we're in body data mode, take the semaphore to prevent the buffer being modified while the display message is being compiled
          DEBUG_PRINT("CAN: Got measurements from ECC: ");
          switch(RxMsg.data[0]){              // measurement block ID -> update data which the message is referencing
            case 0x07:  {             // 0x07 references battery voltage
              if(!badVoltage_VectraC_bypass){
                float CAN_data_voltage=RxMsg.data[2];
                CAN_data_voltage/=10;
                if(CAN_data_voltage>9 && CAN_data_voltage<16){
                  snprintf(voltage_buffer, sizeof(voltage_buffer), "Voltage: %.1f V  ", CAN_data_voltage);
                } else {            // we get erroneous readings, as such we'll switch to reading from display on the next measurement request
                  badVoltage_VectraC_bypass=1;
                  setPreferencesBool("vectra", 1);
                }
                setFlag(CAN_voltage_recvd);
                DEBUG_PRINT("battery voltage\n");
              } else {
                xQueueSend(canTxQueue, &Msg_VoltageRequestDIS, pdMS_TO_TICKS(100));   // request just the voltage from Vectra's display because ECC voltage reading is erratic compared to Astra/Zafira
              }
              break;
            }
            case 0x10:  {             // 0x10 references coolant temps
              unsigned short raw_coolant=(RxMsg.data[3]<<8 | RxMsg.data[4]);
              float CAN_data_coolant=raw_coolant;
              CAN_data_coolant/=10;
              snprintf(coolant_buffer, sizeof(coolant_buffer), "Coolant: %.1f%c%cC   ", CAN_data_coolant, 0xC2, 0xB0);
              setFlag(CAN_coolant_recvd);
              DEBUG_PRINT("coolant\n");
              break;
            }
            case 0x11:  {             // 0x11 references RPMs and speed
              int CAN_data_rpm=(RxMsg.data[1]<<8 | RxMsg.data[2]);
              int CAN_data_speed=RxMsg.data[4];
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
          if(disp_mode==1 || disp_mode==2) xSemaphoreGive(BufferSemaphore);  // let the message processing continue
        break;
      }
      case CAN_ID_RADIO_DISPLAY: {                                         // radio requests a display update
        if(!checkFlag(a2dp_started)){
          setFlag(ehu_started);                    // start the bluetooth A2DP service after first radio display call
          disp_mode=0;
          vTaskResume(canMessageDecoderTaskHandle);       // begin decoding data from the display
        } else if(checkFlag(a2dp_started) && !checkFlag(ehu_started)){
          a2dp_sink.reconnect();
          setFlag(ehu_started);
        }
        if(disp_mode==0){      // queue that data for decoding by another task
          for(int i=1; i<=7; i++){
            xQueueSend(canDispQueue, &RxMsg.data[i], portMAX_DELAY);    // send a continuous byte stream
          }
        }
        xTaskNotifyGive(canWatchdogTaskHandle);    // reset the watchdog
        break;
      }
      case CAN_ID_ECC_PRESENCE: {
        /* CAN_ID_ECC_PRESENCE (0x6C8) is transmitted by the ECC module.  Its presence
         * confirms that the vehicle has electronic climate control, enabling the
         * ECC-specific measurement request messages (Msg_MeasurementRequestECC etc.). */
        if(!checkFlag(ECC_present)) setFlag(ECC_present);
        break;
      }
      default:    break;
    }
  }
}

/* canTransmitTask — Core 0, priority 1
 *
 * Purpose: dequeue outgoing CAN frames from canTxQueue and hand them to the
 *          TWAI driver.  Monitors the TWAI_ALERT_TX_SUCCESS alert after every
 *          transmission to detect whether the frame was acknowledged on the bus.
 *
 * Alert monitoring:
 *   After each twai_transmit(), twai_read_alerts() is called with a 10 ms timeout.
 *   If TWAI_ALERT_TX_SUCCESS is set and the transmitted frame was a first frame
 *   (data[0] == 0x10 or 0x11) on displayMsgIdentifier, CAN_MessageReady is set
 *   to tell canReceiveTask that a flow-control response (0x30) is now expected.
 *   If transmission fails (queue full or TWAI error), CAN_prevTxFail is set
 *   so canDisplayTask can decide to retry.
 *
 * Error recovery:
 *   CAN_prevTxFail and CAN_abortMultiPacket are set on failure, causing
 *   canDisplayTask to re-enter its send loop.
 */
// this task receives CAN messages from canTxQueue and transmits them asynchronously
void canTransmitTask(void *pvParameters){
  static twai_message_t TxMessage;
  int alert_result;
  while(1){
    xQueueReceive(canTxQueue, &TxMessage, portMAX_DELAY);
    TxMessage.extd=0;
    TxMessage.rtr=0;
    TxMessage.ss=0;
    TxMessage.self=0;
    //DEBUG_PRINTF("%03X # %02X %02X %02X %02X %02X %02X %02X %02X", TxMessage.identifier, TxMessage.data[0], TxMessage.data[1], TxMessage.data[2], TxMessage.data[3], TxMessage.data[4], TxMessage.data[5], TxMessage.data[6], TxMessage.data[7]);
    if(twai_transmit(&TxMessage, pdMS_TO_TICKS(50))==ESP_OK) {
      //DEBUG_PRINT(" Q:OK ");
    } else {
      //DEBUG_PRINT("Q:FAIL ");
      setFlag(CAN_prevTxFail);
      if(TxMessage.identifier==displayMsgIdentifier && (TxMessage.data[0]==0x10 || TxMessage.data[0]==0x11)) setFlag(CAN_abortMultiPacket);
    }
    alert_result=twai_read_alerts(&alerts_triggered, pdMS_TO_TICKS(10));    // read stats
    if(alert_result==ESP_OK){
        //DEBUG_PRINT("AR:OK ");
      if(alerts_triggered & TWAI_ALERT_TX_SUCCESS){
        if(TxMessage.identifier==displayMsgIdentifier && (TxMessage.data[0]==0x10 || TxMessage.data[0]==0x11)) setFlag(CAN_MessageReady); // let the display task know that the first frame has been transmitted and we're expecting flow control (0x2C1) frame now
        //DEBUG_PRINTLN("TX:OK ");
      } else {
        DEBUG_PRINTLN("TX:FAIL ");
        setFlag(CAN_prevTxFail);
      }
    } else {
        setFlag(CAN_prevTxFail);
        if(TxMessage.identifier==displayMsgIdentifier && (TxMessage.data[0]==0x10 || TxMessage.data[0]==0x11)) setFlag(CAN_abortMultiPacket);
        DEBUG_PRINT("AR:FAIL:");
      if(alert_result==ESP_ERR_INVALID_ARG){
        DEBUG_PRINTLN("INV_ARG");
      }
      if(alert_result==ESP_ERR_INVALID_STATE){
        DEBUG_PRINTLN("INV_STATE");
      }
      if(alert_result==ESP_ERR_TIMEOUT){
        DEBUG_PRINTLN("TIMEOUT");
      }
    }
  }
}

#ifdef DEBUG
char* split_text[3];
char usage_stats[512];

bool checkMutexState(){
  if(xSemaphoreTake(CAN_MsgSemaphore, pdMS_TO_TICKS(1))==pdTRUE){
    xSemaphoreGive(CAN_MsgSemaphore);
    return 0;
  } else {
    return 1;
  }
}

void CANsimTask(void *pvParameters){
  while(1){
    if(Serial.available()>0){
      Serial.print("SERIAL: Action - ");
      char serial_input=Serial.read();
      switch(serial_input){
        case '2': Serial.print("UP\n");
                  xQueueSend(canTxQueue, &simulate_scroll_up, portMAX_DELAY);
                  break;
        case '8': Serial.print("DOWN\n");
                  xQueueSend(canTxQueue, &simulate_scroll_down, portMAX_DELAY);
                  break;
        case '6': Serial.print("UP\n");
                  xQueueSend(canTxQueue, &simulate_scroll_up, portMAX_DELAY);
                  break;
        case '4': Serial.print("DOWN\n");
                  xQueueSend(canTxQueue, &simulate_scroll_down, portMAX_DELAY);
                  break;
        case '5': Serial.print("PRESS\n");
                  xQueueSend(canTxQueue, &simulate_scroll_press, portMAX_DELAY);
                  vTaskDelay(pdMS_TO_TICKS(100));
                  xQueueSend(canTxQueue, &simulate_scroll_release, portMAX_DELAY);
                  break;
        case 'd': {
          Serial.print("CURRENT FLAGS CAN: ");
          Serial.printf("CAN_MessageReady=%d CAN_prevTxFail=%d, DIS_forceUpdate=%d, ECC_present=%d, ehu_started=%d \n", checkFlag(CAN_MessageReady), checkFlag(CAN_prevTxFail), checkFlag(DIS_forceUpdate), checkFlag(ECC_present), checkFlag(ehu_started));
          Serial.print("CURRENT FLAGS BODY: ");
          Serial.printf("CAN_voltage_recvd=%d CAN_coolant_recvd=%d, CAN_speed_recvd=%d, CAN_new_dataSet_recvd=%d \n", checkFlag(CAN_voltage_recvd), checkFlag(CAN_coolant_recvd), checkFlag(CAN_speed_recvd), checkFlag(CAN_new_dataSet_recvd));
          Serial.print("TIME AND STUFF: ");
          Serial.printf("last_millis_req=%lu last_millis_disp=%lu, millis=%lu \n", last_millis_req, last_millis_disp, millis());
          Serial.printf("CanMsgSemaphore state: %d \n", checkMutexState());
          Serial.printf("TxQueue items: %d, RxQueue items: %d \n", uxQueueMessagesWaiting(canTxQueue), uxQueueMessagesWaiting(canRxQueue));
          break;
        }
        case 'T': {             // print arbitrary text from serial to the display, max char count for each line is 35
          char inputBuffer[256];
          int bytesRead=Serial.readBytesUntil('\n', inputBuffer, 256);
          inputBuffer[bytesRead]='\0';
          serialStringSplitter(inputBuffer);
          disp_mode=0;
          writeTextToDisplay(1, split_text[0], split_text[1], split_text[2]);
          break;
        }
        case 'C': {
          prefs_clear();
          break;
        }
        default: break;
      }
    }
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

void serialStringSplitter(char* input){
  char* text_in;
  int text_count=0;
  text_in=strtok(input, "|");
  while(text_in!=NULL && text_count<3){
    split_text[text_count]=text_in;
    text_count++;
    text_in=strtok(NULL, "|");
  }
  while(text_count<3){
    split_text[text_count]="";
    text_count++;
  }
}
#endif

/* canDisplayTask — Core 1, priority 1
 *
 * Purpose: implement the ISO 15765-2 (DoCAN) multi-packet transmission protocol
 *          to send display messages to the car's instrument cluster / radio display.
 *
 * Protocol flow:
 *   1. Take CAN_MsgSemaphore (prevents writeTextToDisplay() from modifying the
 *      buffer mid-transmission).
 *   2. If CAN_flowCtlFail is set (previous block attempt failed), wait 300 ms for
 *      the radio to finish its own transmission before we start ours.
 *   3. Send CAN_MsgArray[0] (the first frame, FF) via canTxQueue.
 *   4. Block on xTaskNotifyWait() indefinitely.  canReceiveTask or
 *      canTransmitTask will call xTaskNotifyGive() when a flow-control frame
 *      with byte 0 = 0x30 (CTS) is detected.
 *   5. Once unblocked, send CAN_MsgArray[1..n] (consecutive frames, CF) one by
 *      one, honouring canISO_frameSpacing between each frame.
 *      The loop stops when:
 *        - the next frame label is 0x00 (no more data), OR
 *        - CAN_prevTxFail is set (TX error), OR
 *        - CAN_abortMultiPacket is set (radio sent a new first frame while we
 *          were still transmitting → our session is stale).
 *   6. Release CAN_MsgSemaphore.
 *   7. If retryTx is true, loop back to step 1.  Otherwise vTaskSuspend(NULL)
 *      to wait for the next writeTextToDisplay() call.
 *
 * Note: canDisplayTask is kept suspended (vTaskSuspend) between transmissions
 *       and resumed by writeTextToDisplay() or canReceiveTask.
 */
// this task implements ISO 15765-2 (multi-packet transmission over CAN frames) in a crude, but hopefully robust way in order to send frames to the display
void canDisplayTask(void *pvParameters){
  static twai_message_t MsgToTx;
  MsgToTx.identifier=displayMsgIdentifier;
  MsgToTx.data_length_code=8;
  uint32_t notifResult;
  bool retryTx=0;
  while(1){
    retryTx=0;
    if(xSemaphoreTake(CAN_MsgSemaphore, portMAX_DELAY)==pdTRUE){          // if the buffer is being accessed, block indefinitely
      if(checkFlag(CAN_flowCtlFail)){
        vTaskDelay(pdMS_TO_TICKS(300));   // since we failed at flow control, wait for the radio to finish its business
      }
      clearFlag(CAN_prevTxFail);
      clearFlag(CAN_abortMultiPacket);      // new transmission, we clear these
      memcpy(MsgToTx.data, CAN_MsgArray[0], 8);
      xQueueSend(canTxQueue, &MsgToTx, portMAX_DELAY);
      DEBUG_PRINTLN("CAN: Now waiting for flow control frame...");
      if(xTaskNotifyWait(0, 0, NULL, portMAX_DELAY)==pdPASS){            // blocking execution until flow control from display arrives
        DEBUG_PRINTLN("CAN: Got flow control! Sending consecutive frames...");
        for(int i=1;i<64 && (CAN_MsgArray[i][0]!=0x00 && !checkFlag(CAN_prevTxFail) && !checkFlag(CAN_abortMultiPacket));i++){                 // this loop will stop sending data once the next packet doesn't contain a label
          memcpy(MsgToTx.data, CAN_MsgArray[i], 8);
          xQueueSend(canTxQueue, &MsgToTx, portMAX_DELAY);
          vTaskDelay(pdMS_TO_TICKS(canISO_frameSpacing));         // receiving node can request a variable frame spacing time, we take it into account here, so far I've seen BID request 2ms while GID/CID request 0ms (no delay)
        }
        clearFlag(CAN_MessageReady);  // clear this as fast as possible once we're done sending
        if(checkFlag(CAN_prevTxFail) || checkFlag(CAN_abortMultiPacket)){
          retryTx=1;                          // "queue up" to restart this task since something went wrong
          clearFlag(CAN_prevTxFail);
        }
        xTaskNotifyStateClear(NULL);
      } else {                      // fail 
        DEBUG_PRINTLN("CAN: Flow control frame has not been received in time, aborting");
        clearFlag(CAN_MessageReady);
        retryTx=1;
      }
      xSemaphoreGive(CAN_MsgSemaphore);           // release the semaphore
    }
    if(!retryTx) vTaskSuspend(NULL);   // have the display task stop itself only if there is no need to retransmit, else run once again
  }
}

/* canAirConMacroTask — Core 0, priority 10
 *
 * Purpose: simulate a sequence of AC panel knob events to toggle the AC
 *          compressor on/off.  The sequence navigates the ECC menu:
 *   1. Wait 500 ms (let the AC menu appear on screen after activation).
 *   2. Knob one step down  (moves selection towards AC toggle)
 *   3. Knob press + release (confirms selection)
 *   4. Knob two steps up   (navigates away from the changed setting)
 *   5. Knob press + release (confirms the second selection)
 *
 * Timing: 100 ms between each event.  The 500 ms initial delay is necessary
 * because some ECC units take up to 400 ms to display the menu after the first
 * input, and premature inputs are silently dropped.
 *
 * Lifecycle: created at startup but immediately suspended.  Resumed by
 *            canProcessTask when the AC knob long-press gesture is detected.
 *            Suspends itself again (vTaskSuspend(NULL)) after the sequence.
 */
// this task provides asynchronous simulation of button presses on the AC panel, quickly toggling AC
void canAirConMacroTask(void *pvParameters){
  while(1){
    vTaskDelay(pdMS_TO_TICKS(500));    // initial delay has to be extended, in some cases 100ms was not enough to have the AC menu appear on the screen, resulting in the inputs being dropped and often entering the vent config instead
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

/* canMessageDecoder — Core 0, idle priority
 *
 * Purpose: scan the raw UTF-16 byte stream from the radio's 0x6C1 display
 *          messages (queued byte-by-byte in canDispQueue) for specific text
 *          patterns to determine whether EHU32 is allowed to overwrite the
 *          display.
 *
 * "Aux" detection (pattern index 0):
 *   When "Aux" appears on the display it means the radio's AUX input is
 *   selected and EHU32 should overwrite the display with Bluetooth metadata.
 *   The full UTF-16 LE pattern for the ESC-formatted "Aux" string is:
 *     {0x00, 0x6D, 0x00, 0x41, 0x00, 0x75, 0x00, 0x78}
 *   (the 0x00, 0x6D prefix is the tail end of the left-align ESC sequence)
 *   On match: sets CAN_allowAutoRefresh and DIS_forceUpdate, records timestamp
 *             in last_millis_aux for the 6-second timeout.
 *
 * Sound menu items (patterns 1–5): Fader, Balance, Bass, Treble, Sound Off
 *   These appear on CD30/CD40 when the user enters the SOUND menu.  If EHU32
 *   were to intercept these, the user could never adjust audio settings.
 *   On match: clears CAN_allowAutoRefresh (lets the radio's message through).
 *   UTF-16 LE patterns:
 *     "Fader"    {0x46,0,0x61,0,0x64,0,0x65,0,0x72}
 *     "Balance"  {0x42,0,0x61,0,0x6C,0,0x61,0,0x6E,0,0x63,0,0x65}
 *     "Bass"     {0x42,0,0x61,0,0x73,0,0x73}
 *     "Treble"   {0x54,0,0x72,0,0x65,0,0x62,0,0x6C,0,0x65}
 *     "Sound Off"{0x53,0,0x6F,0,0x75,0,0x6E,0,0x64,0,0x20,0,0x4F,0,0x66,0,0x66}
 *
 * Aux timeout: if CAN_allowAutoRefresh is set but "Aux" has not appeared within
 *              the last 6 seconds, CAN_allowAutoRefresh is automatically cleared
 *              to prevent stale display updates after the user switches sources.
 *
 * Communication: receives bytes from canDispQueue (filled by canProcessTask).
 */
// this task monitors raw data contained within messages sent by the radio and looks for Aux string being printed to the display; rejects "Aux" in views such as "Audio Source" screen (CD70/DVD90)
void canMessageDecoder(void *pvParameters){
  uint8_t rxDisplay;
  const uint8_t AuxPattern[8]={0x00, 0x6D, 0x00, 0x41, 0x00, 0x75, 0x00, 0x78};     // snippet of data to look for, allows for robust detection of "Aux" on all kinds of headunits
  int patternIndex=0;
  bool patternFound=0;
  int currentIndex[6] = {0};
  const char patterns[6][17] = {           // this is a crutch for CD30/CD40 "SOUND" menu, required to be able to adjust fader/balance/bass/treble, otherwise EHU32 will block it from showing up
    {0, 0x6D, 0, 0x41, 0, 0x75, 0, 0x78},                // formatted Aux (left or center aligned). Weird formatting because the data is in UTF-16
    {0x46, 0, 0x61, 0, 0x64, 0, 0x65, 0, 0x72},                                      // Fader
    {0x42, 0, 0x61, 0, 0x6c, 0, 0x61, 0, 0x6e, 0, 0x63, 0, 0x65},                    // Balance
    {0x42, 0, 0x61, 0, 0x73, 0, 0x73},                                               // Bass
    {0x54, 0, 0x72, 0, 0x65, 0, 0x62, 0, 0x6c, 0, 0x65},                             // Treble
    {0x53, 0, 0x6f, 0, 0x75, 0, 0x6e, 0, 0x64, 0, 0x20, 0, 0x4f, 0, 0x66, 0, 0x66}   // Sound Off
  };
  const char patternLengths[6] = {8, 9, 13, 7, 11, 17};
  while(1){
    if(xQueueReceive(canDispQueue, &rxDisplay, portMAX_DELAY)==pdTRUE){         // wait for new data queued by the ProcessTask
      for(int i=0;i<6; i++){
          if(rxDisplay==patterns[i][currentIndex[i]]){
            currentIndex[i]++;
            if(currentIndex[i]==patternLengths[i]){
              switch(i){
                case 0:{          // formatting+Aux detected
                  patternFound=1;
                  last_millis_aux=millis();             // keep track of when was the last time Aux has been seen
                  DEBUG_PRINTLN("CAN Decode: Found Aux string!");
                  break;
                }
                case 1:   // either Fader, Balance, Bass, Treble or Sound Off
                case 2:
                case 3:
                case 4:
                case 5: patternFound=0;
                        clearFlag(CAN_allowAutoRefresh);        // we let the following message(s) through
              }
              for(int j=0; j<6; j++){
                  currentIndex[j]=0;
              }
              break;
            }
          } else {
            currentIndex[i] = 0;                  // no match, start anew
            if (rxDisplay == patterns[i][0]) {
              currentIndex[i] = 1;
            }
          }
      }
    }
    if(checkFlag(CAN_allowAutoRefresh) && !patternFound && (last_millis_aux+6000<millis())){
      clearFlag(CAN_allowAutoRefresh);    // Aux string has not appeared within the last 6 secs -> stop auto-updating the display
      DEBUG_PRINTLN("CAN Decode: Disabling display autorefresh...");
    } else {
      if(patternFound && !checkFlag(CAN_allowAutoRefresh)){
        setFlag(CAN_allowAutoRefresh);
        setFlag(DIS_forceUpdate);        // gotta force a buffer update here anyway since the metadata might be outdated (wouldn't wanna reprint old audio metadata right?)
        DEBUG_PRINTLN("CAN Decode: Enabling display autorefresh...");
      }
      patternFound=0;
    }
    vTaskDelay(pdMS_TO_TICKS(2));
  }
}

/* prepareMultiPacket() — encode a flat byte buffer into ISO 15765-2 CAN frames.
 *
 * Parameters:
 *   bytesProcessed  — total number of payload bytes to encode (from DisplayMsg).
 *   buffer_to_read  — pointer to the source buffer (DisplayMsg).
 *
 * Output: CAN_MsgArray[][] is filled with labelled 8-byte frames ready for
 *         sequential transmission by canDisplayTask.
 *
 * Frame structure (ISO 15765-2):
 *   Frame 0  — First Frame (FF):
 *     byte 0 = 0x10 (payload ≤ 255 bytes) or 0x11 (payload > 255 bytes, TODO)
 *     bytes 1–7 = first 7 payload bytes
 *   Frames 1..n — Consecutive Frames (CF):
 *     byte 0 = 0x21, 0x22, … 0x2F, then wraps back to 0x20 (ISO sequence number)
 *     bytes 1–7 = next 7 payload bytes
 *   Last partial frame (if bytesProcessed % 7 != 0):
 *     byte 0 = next sequence number
 *     bytes 1..k = remaining bytes; bytes k+1..7 are left as zeros (padding)
 *   Terminator: CAN_MsgArray[packetCount+1][0] = 0x00 to signal end of data
 *               (canDisplayTask stops transmitting when it sees a 0x00 label).
 *
 * Note: proper DoCAN handling for payloads > 255 bytes is not yet implemented;
 *       for those the first-frame header needs two additional length bytes.
 */
// loads formatted UTF16 data into CAN_MsgArray and labels the messages; needs to know how many bytes to load into the array; afterwards this array is ready to be transmitted with sendMultiPacket()
void prepareMultiPacket(int bytesProcessed, char* buffer_to_read){             // longer CAN messages are split into appropriately labeled (the so called PCI) packets, starting with 0x21 up to 0x2F, then rolling back to 0x20
  int packetCount=bytesProcessed/7, bytesToProcess=bytesProcessed%7;
  unsigned char frameIndex=0x20;
  for(int i=0; i<packetCount; i++){
    CAN_MsgArray[i][0]=frameIndex;
    frameIndex=(frameIndex==0x2F)? 0x20: frameIndex+1;             // reset back to 0x20 after 0x2F, otherwise increment the frameIndex (consecutive frame index)
    memcpy(&CAN_MsgArray[i][1], &buffer_to_read[i*7], 7);       // copy 7 bytes at a time to fill consecutive frames with data
  }
  if(bytesProcessed<=255){       // TODO: implement proper 255+ byte DoCAN handling
    CAN_MsgArray[0][0]=0x10;            // first frame index is always 0x10 for datasets smaller than 255 bytes
  } else {
    CAN_MsgArray[0][0]=0x11;
  }
  if(bytesToProcess>0){                 // if there are bytes left to be processed but are not enough for a complete message, process them now
    CAN_MsgArray[packetCount][0]=frameIndex;
    memcpy(&CAN_MsgArray[packetCount][1], &buffer_to_read[packetCount*7], bytesToProcess);
    packetCount++;
  }
  CAN_MsgArray[packetCount+1][0]=0x0;      // remove the next frame label if there was any, as such it will not be transmitted
}

// function to queue a frame requesting measurement blocks
void requestMeasurementBlocks(){
  DEBUG_PRINT("CAN: Requesting measurements from ");
  if(checkFlag(ECC_present)){              // request measurement blocks from the climate control module
    DEBUG_PRINTLN("climate control...");
    xQueueSend(canTxQueue, &Msg_MeasurementRequestECC, portMAX_DELAY);
  } else {
    DEBUG_PRINTLN("display...");
    xQueueSend(canTxQueue, &Msg_MeasurementRequestDIS, portMAX_DELAY);        // fallback if ECC is not present, then we read reduced data from the display
  }
}

// function to queue a frame requesting just the coolant data
void requestCoolantTemperature(){
  DEBUG_PRINT("CAN: Requesting coolant temperature from ");
  if(checkFlag(ECC_present)){              // request measurement blocks from the climate control module
    DEBUG_PRINTLN("climate control...");
    xQueueSend(canTxQueue, &Msg_CoolantRequestECC, portMAX_DELAY);
  } else {
    DEBUG_PRINTLN("display...");
    xQueueSend(canTxQueue, &Msg_CoolantRequestDIS, portMAX_DELAY);        // fallback if ECC is not present, then we read reduced data from the display
  }
}

// below functions are used to add additional functionality to longpresses on the radio panel
void canActionEhuButton0(bool btn_state, unsigned int btn_ms_held){         // do not use for CD30! it does not have a "0" button
}

// regular audio metadata mode
void canActionEhuButton1(bool btn_state, unsigned int btn_ms_held){
  if(disp_mode!=0 && btn_ms_held>=500){
    disp_mode=0;   // we have to check whether the music is playing, else the buffered song title just stays there
    setFlag(DIS_forceUpdate);
  }
}

// measurement mode type 1, printing speed+rpm, coolant and voltage from measurement blocks
void canActionEhuButton2(bool btn_state, unsigned int btn_ms_held){
  if(disp_mode!=1 && btn_ms_held>=500){
    clearFlag(CAN_new_dataSet_recvd);
    disp_mode=1;
    setFlag(disp_mode_changed);
    DEBUG_PRINTLN("DISP_MODE: Switching to vehicle data...");
  }
}

// measurement mode type 2, printing coolant from measurement blocks
void canActionEhuButton3(bool btn_state, unsigned int btn_ms_held){
  if(disp_mode!=2 && btn_ms_held>=500){
    clearFlag(CAN_new_dataSet_recvd);
    disp_mode=2;
    setFlag(disp_mode_changed);
    DEBUG_PRINTLN("DISP_MODE: Switching to 1-line coolant...");
  }
}

// no action
void canActionEhuButton4(bool btn_state, unsigned int btn_ms_held){
}

// no action
void canActionEhuButton5(bool btn_state, unsigned int btn_ms_held){
}

// no action
void canActionEhuButton6(bool btn_state, unsigned int btn_ms_held){
}

// no action
void canActionEhuButton7(bool btn_state, unsigned int btn_ms_held){
}

// Start OTA to allow updating over Wi-Fi
void canActionEhuButton8(bool btn_state, unsigned int btn_ms_held){
  if(!checkFlag(OTA_begin) && btn_ms_held>=1000){
    setFlag(OTA_begin);
  } else {
    if(btn_ms_held>=5000) setFlag(OTA_abort);       // allows to break out of OTA mode
  }
}

// holding the button for half a second disables EHU32 influencing the screen in any way, holding it for 5 whole seconds clears any saved settings and hard resets the ESP32
void canActionEhuButton9(bool btn_state, unsigned int btn_ms_held){
  if(disp_mode!=-1 && btn_ms_held>=500){
    disp_mode=-1;
    DEBUG_PRINTLN("Screen updates disabled");
  }
  if(btn_ms_held>=5000){                // this will perform a full reset, including clearing settings, so the next boot will take some time
    if(!checkFlag(OTA_begin)){
      vTaskDelay(pdMS_TO_TICKS(1000));
      prefs_clear();
      vTaskDelay(pdMS_TO_TICKS(1000));
      ESP.restart();
    }
  }
}
// ============================================================
// КНОПКИ "ВЛЕВО" и "ВПРАВО" на магнитоле
// Долгое нажатие (>=400 мс) переключает трек (один раз)
// ============================================================

void canActionEhuButtonLeft(bool btn_state, unsigned int btn_ms_held){
    static bool triggered = false;
    if(btn_ms_held >= 400 && checkFlag(CAN_allowAutoRefresh) && checkFlag(bt_connected)){
        if(!triggered){
            triggered = true;
            a2dp_sink.previous();
            DEBUG_PRINTLN("DISP_MODE: Previous track");
        }
    } else {
        triggered = false;
    }
}

void canActionEhuButtonRight(bool btn_state, unsigned int btn_ms_held){
    static bool triggered = false;
    if(btn_ms_held >= 400 && checkFlag(CAN_allowAutoRefresh) && checkFlag(bt_connected)){
        if(!triggered){
            triggered = true;
            a2dp_sink.next();
            DEBUG_PRINTLN("DISP_MODE: Next track");
        }
    } else {
        triggered = false;
    }
}
