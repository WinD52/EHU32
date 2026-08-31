/*
 * A2DP.ino — Bluetooth A2DP audio sink and AVRCP metadata handling
 *
 * Project: EHU32 (Opel MS-CAN Bluetooth Audio Gateway)
 * Original Author: PNKP237 — https://github.com/PNKP237/EHU32
 * Author & Maintainer: WinD52 — https://github.com/WinD52/EHU32
 *
 * Version: v1.0.0-alpha (Modern Platform Baseline: ESP-IDF 5.3 / Arduino Core 3.1)
 *
 * Audio Architecture:
 *   - Native ESP-IDF 5.x I2S Standard driver (driver/i2s_std.h)
 *   - 32-bit stereo Philips slot configuration for PCM5102A DAC
 *   - Clean linear 32-bit fixed-point DSP headroom (-6.02 dBFS / 1.05 Vrms)
 *   - Zero non-linear waveshaping distortion; zero DMA sample dropouts (portMAX_DELAY)
 *   - Dynamic I2S clock reconfiguration for seamless 44.1 kHz / 48.0 kHz switching
 *   - Nominal Bluetooth Classic RF transmit power configured to +6 dBm (ESP_PWR_LVL_P6)
 */

#include "config.h"
#include "BluetoothA2DPSink.h"
#include "driver/i2s_std.h"
#include "esp_gap_bt_api.h"

static BluetoothA2DPSink a2dp_sink;
static i2s_chan_handle_t tx_handle = NULL;
static uint32_t current_sample_rate = 44100;
static volatile uint16_t pending_sample_rate = 0;

// 32-битный статический DSP буфер для линейной обработки сэмплов
static int32_t dsp_buffer_32bit[512];

// ============================================================================
// audio_data_stream_32bit_dsp — Линейный 32-битный DSP конвейер (1.05 Vrms Headroom)
// ============================================================================
static void audio_data_stream_32bit_dsp(const uint8_t *data, uint32_t length) {
  if (tx_handle == NULL || data == NULL || length == 0) return;

  // Динамическая переконфигурация тактового генератора I2S при смене трека (44.1k <-> 48k)
  if (pending_sample_rate != 0 && pending_sample_rate != current_sample_rate) {
    uint16_t new_rate = pending_sample_rate;
    i2s_channel_disable(tx_handle);
    i2s_std_clk_config_t clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(new_rate);
    if (i2s_channel_reconfig_std_clock(tx_handle, &clk_cfg) == ESP_OK) {
      current_sample_rate = new_rate;
      DEBUG_PRINTF("[I2S] Sample rate dynamically reconfigured to %u Hz\n", new_rate);
    }
    i2s_channel_enable(tx_handle);
    pending_sample_rate = 0;
  }

  const int16_t *src = (const int16_t *)data;
  size_t total_samples = length / sizeof(int16_t);
  size_t processed = 0;

  while (processed < total_samples) {
    size_t chunk_samples = total_samples - processed;
    if (chunk_samples > 512) chunk_samples = 512;

    for (size_t i = 0; i < chunk_samples; i++) {
      // 1. Знаковое расширение 16-бит исходного звука в 32-битное пространство со знаком
      int32_t s32 = ((int32_t)src[processed + i]) << 16;

      // 2. Чистое линейное масштабирование 0.50x (-6.02 dBFS Headroom)
      // Опускает пиковый размах 2.1 Vrms точно до 1.05 Vrms (стандарт входа AUX CD30).
      // Все 16 бит исходного звука сохраняются на 100% без усечения и без нелинейной "каши"!
      s32 = s32 >> 1;

      dsp_buffer_32bit[i] = s32;
    }

    // 3. Непрерывная передача в DMA без потерь сэмплов и без разрывов фазы
    size_t bytes_to_write = chunk_samples * sizeof(int32_t);
    size_t bytes_written = 0;
    i2s_channel_write(tx_handle, (const char *)dsp_buffer_32bit, bytes_to_write, &bytes_written, portMAX_DELAY);

    processed += chunk_samples;
  }
}

// Фиксация новой частоты дискретизации без блокировок в Bluedroid коллбэке
static void a2dp_sample_rate_changed(uint16_t rate) {
  if (rate != 0 && rate != current_sample_rate) {
    pending_sample_rate = rate;
  }
}

// ============================================================================
// avrc_metadata_callback — Прием текстовых тегов AVRCP (Название, Артист, Альбом)
// ============================================================================
void avrc_metadata_callback(uint8_t md_type, const uint8_t *data2) {
  if(xSemaphoreTake(BufferSemaphore, pdMS_TO_TICKS(10)) == pdTRUE){
    switch(md_type){
      case ESP_AVRC_MD_ATTR_TITLE:
        memset(title_buffer, 0, sizeof(title_buffer));
        snprintf(title_buffer, sizeof(title_buffer), "%s", (const char*)data2);
        setFlag(md_title_recvd);
        DEBUG_PRINTF("[AVRCP META] Title:  \"%s\"\n", title_buffer);
        break;
      case ESP_AVRC_MD_ATTR_ARTIST:
        memset(artist_buffer, 0, sizeof(artist_buffer));
        snprintf(artist_buffer, sizeof(artist_buffer), "%s", (const char*)data2);
        setFlag(md_artist_recvd);
        DEBUG_PRINTF("[AVRCP META] Artist: \"%s\"\n", artist_buffer);
        break;
      case ESP_AVRC_MD_ATTR_ALBUM:
        memset(album_buffer, 0, sizeof(album_buffer));
        snprintf(album_buffer, sizeof(album_buffer), "%s", (const char*)data2);
        setFlag(md_album_recvd);
        DEBUG_PRINTF("[AVRCP META] Album:  \"%s\"\n", album_buffer);
        break;
      default: break;
    }
    xSemaphoreGive(BufferSemaphore);
  }

  // Строгое сохранение оригинальной логики WinD52 (требуются все 3 поля)
  if(checkFlag(md_title_recvd) && checkFlag(md_artist_recvd) && checkFlag(md_album_recvd)){
    DEBUG_PRINTLN("[AVRCP META] All 3 tags received -> Updating GID/CID display buffer");
    setFlag(DIS_forceUpdate);
    clearFlag(md_title_recvd);
    clearFlag(md_artist_recvd);
    clearFlag(md_album_recvd);
  }
}

// ============================================================================
// a2dp_connection_state_changed — Коллбэк статуса Bluetooth-соединения
// ============================================================================
void a2dp_connection_state_changed(esp_a2d_connection_state_t state, void *ptr){
  if(state == ESP_A2D_CONNECTION_STATE_CONNECTED){
    DEBUG_PRINTF("\n>>> [A2DP EVENT] Smartphone CONNECTED! Peer: %s <<<\n", a2dp_sink.get_peer_name());
    setFlag(flag_bt_connected);
  } else if(state == ESP_A2D_CONNECTION_STATE_DISCONNECTED){
    DEBUG_PRINTLN("\n>>> [A2DP EVENT] Smartphone DISCONNECTED <<<");
    clearFlag(flag_bt_connected);
  }
  setFlag(bt_state_changed);
}

// ============================================================================
// a2dp_audio_state_changed — Коллбэк активности аудиопотока (Play / Pause)
// ============================================================================
void a2dp_audio_state_changed(esp_a2d_audio_state_t state, void *ptr){
  if(state == ESP_A2D_AUDIO_STATE_STARTED){
    DEBUG_PRINTLN("[A2DP AUDIO] Stream status: PLAYING (Un-muting PCM5102A)");
    setFlag(bt_audio_playing);
  } else if(state == ESP_A2D_AUDIO_STATE_STOPPED || state == ESP_A2D_AUDIO_STATE_REMOTE_SUSPEND){
    DEBUG_PRINTLN("[A2DP AUDIO] Stream status: PAUSED / STOPPED (Muting PCM5102A)");
    clearFlag(bt_audio_playing);
  }
  setFlag(audio_state_changed);
}

// ============================================================================
// i2s_driver_init — Инициализация нативного драйвера I2S чипа ESP32 (32 бита)
// ============================================================================
static bool i2s_driver_init(uint32_t sample_rate = 44100) {
  i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
  chan_cfg.dma_desc_num = 8;
  chan_cfg.dma_frame_num = 256;
  chan_cfg.auto_clear = true;

  if (i2s_new_channel(&chan_cfg, &tx_handle, NULL) != ESP_OK) {
    DEBUG_PRINTLN("[I2S ERROR] Channel creation failed");
    return false;
  }

  // Честный 32-битный стерео Philips слот для PCM5102A
  i2s_std_config_t std_cfg = {
    .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(sample_rate),
    .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO),
    .gpio_cfg = {
      .mclk = I2S_GPIO_UNUSED,
      .bclk = (gpio_num_t)I2S_PIN_BCK,
      .ws   = (gpio_num_t)I2S_PIN_WS,
      .dout = (gpio_num_t)I2S_PIN_DATA,
      .din  = I2S_GPIO_UNUSED,
      .invert_flags = { .mclk_inv = false, .bclk_inv = false, .ws_inv = false },
    },
  };

  if (i2s_channel_init_std_mode(tx_handle, &std_cfg) != ESP_OK) {
    DEBUG_PRINTLN("[I2S ERROR] Standard mode init failed");
    i2s_del_channel(tx_handle);
    tx_handle = NULL;
    return false;
  }
  if (i2s_channel_enable(tx_handle) != ESP_OK) {
    DEBUG_PRINTLN("[I2S ERROR] Channel enable failed");
    i2s_del_channel(tx_handle);
    tx_handle = NULL;
    return false;
  }
  DEBUG_PRINTLN("[I2S] Native 32-bit DSP driver initialized (32-bit slots, 1.05 Vrms Headroom)");
  return true;
}

// ============================================================================
// a2dp_init — Запуск Bluetooth A2DP Sink и конфигурация мощности передатчика
// ============================================================================
void a2dp_init(){
  DEBUG_PRINTLN("[A2DP] Initializing native 32-bit I2S driver...");
  if (!i2s_driver_init(44100)) {
    DEBUG_PRINTLN("[A2DP FATAL] I2S initialization failed, rebooting...");
    vTaskDelay(pdMS_TO_TICKS(500));
    ESP.restart();
  }

  a2dp_sink.set_stream_reader(audio_data_stream_32bit_dsp, false);
  a2dp_sink.set_sample_rate_callback(a2dp_sample_rate_changed);
  a2dp_sink.set_avrc_metadata_callback(avrc_metadata_callback);
  a2dp_sink.set_avrc_metadata_attribute_mask(ESP_AVRC_MD_ATTR_TITLE | ESP_AVRC_MD_ATTR_ARTIST | ESP_AVRC_MD_ATTR_ALBUM);
  a2dp_sink.set_on_connection_state_changed(a2dp_connection_state_changed);
  a2dp_sink.set_on_audio_state_changed(a2dp_audio_state_changed);
  a2dp_sink.set_reconnect_delay(500);
  a2dp_sink.set_auto_reconnect(true, 2000);

  DEBUG_PRINTF("[A2DP] Starting Bluetooth sink service with name: \"%s\"...\n", BT_DEVICE_NAME);
  a2dp_sink.start(BT_DEVICE_NAME);
  
  // Установка номинальной мощности Bluetooth (+6 dBm)
  esp_bredr_tx_power_set(ESP_PWR_LVL_P6, ESP_PWR_LVL_P6);

  setFlag(a2dp_started);
  DEBUG_PRINTLN(">>> [A2DP] Bluetooth Stack is ONLINE and DISCOVERABLE! <<<");
  disp_mode = 0;
  writeTextToDisplay(true, (char*)"EHU32 v" EHU32_VERSION, (char*)"Bluetooth on", (char*)"Waiting for connection...");
}

// ============================================================================
// A2DP_EventHandler — Диспетчеризация событий связи и софт-мьюта ЦАПа
// ============================================================================
void A2DP_EventHandler(){
  static unsigned long last_reconnect_attempt = 0;
  static unsigned long reconnect_cooldown = 1500; // Пауза 1.5 сек перед первым запросом

  if(checkFlag(ehu_started) && !checkFlag(a2dp_started)){
    a2dp_init();
    last_reconnect_attempt = millis(); // Фиксируем точный момент старта Bluetooth
  }

  // Диспетчер автоподключения: опрашивает телефон только в статусе DISCONNECTED
  if(checkFlag(ehu_started) && checkFlag(a2dp_started)){
    if(a2dp_sink.get_connection_state() == ESP_A2D_CONNECTION_STATE_DISCONNECTED){
      if(millis() - last_reconnect_attempt >= reconnect_cooldown){
        last_reconnect_attempt = millis();
        bool success = a2dp_sink.reconnect();
        DEBUG_PRINTF("[A2DP] state=DISCONNECTED -> reconnect() result=%s\n", success ? "ACCEPTED" : "REJECTED");
        reconnect_cooldown = success ? 10000 : 1500;
      }
    }
  }

  if(checkFlag(DIS_forceUpdate) && disp_mode == 0 && checkFlag(CAN_allowAutoRefresh) && checkFlag(bt_audio_playing)){
    writeTextToDisplay();
  }

  if(checkFlag(bt_state_changed) && disp_mode == 0){
    if(checkFlag(flag_bt_connected)){
      a2dp_sink.set_volume(127);
      writeTextToDisplay(true, (char*)"", (char*)"Bluetooth connected", (char*)a2dp_sink.get_peer_name());
    } else {
      writeTextToDisplay(true, (char*)"", (char*)"Bluetooth disconnected", (char*)"");
    }
    clearFlag(bt_state_changed);
  }

  if(checkFlag(audio_state_changed) && checkFlag(flag_bt_connected) && disp_mode == 0){
    if(checkFlag(bt_audio_playing)){
      digitalWrite(PCM_MUTE_CTL, HIGH); // Снимаем аппаратный софт-мьют ЦАП
      setFlag(DIS_forceUpdate);
    } else {
      digitalWrite(PCM_MUTE_CTL, LOW);  // Аппаратный мьют ЦАП при паузе
      writeTextToDisplay(true, (char*)"Bluetooth connected", (char*)"Paused", (char*)"");
    }
    clearFlag(audio_state_changed);
  }
}

// ============================================================================
// Управление воспроизведением AVRCP (Play, Pause, Next, Prev)
// ============================================================================
void a2dp_play()     { a2dp_sink.play(); }
void a2dp_pause()    { a2dp_sink.pause(); }
void a2dp_next()     { a2dp_sink.next(); }
void a2dp_previous() { a2dp_sink.previous(); }
void a2dp_reconnect() { a2dp_sink.reconnect(); }

// ============================================================================
// Остановка службы A2DP и выключение питания
// ============================================================================
void a2dp_stop(){
  vTaskSuspend(canMessageDecoderTaskHandle);
  digitalWrite(PCM_MUTE_CTL, LOW);
  a2dp_sink.end();
  vTaskDelay(pdMS_TO_TICKS(50));
  if (tx_handle != NULL) {
    i2s_channel_disable(tx_handle);
    i2s_del_channel(tx_handle);
    tx_handle = NULL;
  }
  clearFlag(a2dp_started);
  DEBUG_PRINTLN("[A2DP] Service Stopped & I2S Channel Freed");
}

void a2dp_shutdown(){
  vTaskSuspend(canMessageDecoderTaskHandle);
  ESP.restart();
}
