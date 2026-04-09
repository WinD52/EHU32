/*
 * A2DP.ino — Bluetooth A2DP audio sink and AVRCP metadata handling
 *
 * This file implements EHU32's Bluetooth audio functionality:
 *   - Starts a Bluetooth A2DP sink named "EHU32" that any phone/tablet/PC
 *     can connect to and stream audio from.
 *   - Outputs the received audio stream over I2S to a PCM5102A stereo DAC
 *     (configured for GPIO 26=BCK, 25=WS/LRCK, 22=DATA).
 *   - Receives AVRCP metadata (title, artist, album) and writes it to the
 *     shared text buffers, triggering a display update when all three are
 *     received.
 *   - Handles play/pause/next/previous via AVRCP commands sent from
 *     canProcessTask (steering wheel buttons).
 *   - Mutes the PCM5102A (via PCM_MUTE_CTL) when audio is paused/stopped.
 *   - Attempts automatic reconnection every 2 seconds if no source is
 *     connected.
 *
 * Volume control is disabled (A2DPNoVolumeControl) because volume is managed
 * by the car's head unit amplifier via the CAN bus.  Allowing A2DP volume
 * control would result in a confusing secondary volume layer.
 *
 * Dependencies:
 *   - ESP32-AudioTools (AudioTools.h, I2SStream)
 *   - ESP32-A2DP (BluetoothA2DPSink.h)
 *   - A2DPVolumeControl.h (A2DPNoVolumeControl class)
 *   - Global variables and RTOS handles declared in EHU32.ino
 *
 * Author: PNKP237 — https://github.com/PNKP237/EHU32
 */

#include <A2DPVolumeControl.h>
#include "config.h"				   
I2SStream i2s;
BluetoothA2DPSink a2dp_sink(i2s);
A2DPNoVolumeControl noVolumeControl;

/* avrc_metadata_callback() — called by the ESP32 Bluetooth stack whenever the
 * connected A2DP source sends AVRCP metadata for the current track.
 *
 * Parameters:
 *   md_type — bitmask identifying the metadata field:
 *     0x01  ESP_AVRC_MD_ATTR_TITLE  — track title   → title_buffer
 *     0x02  ESP_AVRC_MD_ATTR_ARTIST — artist name    → artist_buffer
 *     0x04  ESP_AVRC_MD_ATTR_ALBUM  — album name     → album_buffer
 *   data2   — null-terminated UTF-8 string
 *
 * Once all three fields have been received (all three *_recvd flags set), the
 * individual flags are cleared and DIS_forceUpdate is set to trigger an
 * immediate display refresh in A2DP_EventHandler().
 *
 * BufferSemaphore is held while writing to prevent canDisplayTask from reading
 * a partially updated buffer.
 */
// updates the buffers
void avrc_metadata_callback(uint8_t md_type, const uint8_t *data2) {  // fills the song title buffer with data, updates text_lenght with the amount of chars
  xSemaphoreTake(BufferSemaphore, portMAX_DELAY);      // take the semaphore as a way to prevent the buffers being accessed elsewhere
  switch(md_type){
    case 0x1: memset(title_buffer, 0, sizeof(title_buffer));
              snprintf(title_buffer, sizeof(title_buffer), "%s", data2);
              //DEBUG_PRINTF("\nA2DP: Received title: \"%s\"", data2);
              setFlag(md_title_recvd);
              break;
    case 0x2: memset(artist_buffer, 0, sizeof(artist_buffer));
              snprintf(artist_buffer, sizeof(artist_buffer), "%s", data2);
              //DEBUG_PRINTF("\nA2DP: Received artist: \"%s\"", data2);
              setFlag(md_artist_recvd);
              break;
    case 0x4: memset(album_buffer, 0, sizeof(album_buffer));
              snprintf(album_buffer, sizeof(album_buffer), "%s", data2);
              //DEBUG_PRINTF("\nA2DP: Received album: \"%s\"", data2);
              setFlag(md_album_recvd);
              break;
    default:  break;
  }
  xSemaphoreGive(BufferSemaphore);
  if(checkFlag(md_title_recvd) && checkFlag(md_artist_recvd) && checkFlag(md_album_recvd)){
    setFlag(DIS_forceUpdate);                                                      // lets the eventHandler task know that there's new data to be written to the display
    clearFlag(md_title_recvd);
    clearFlag(md_artist_recvd);
    clearFlag(md_album_recvd);
  }
}

/* a2dp_connection_state_changed() — A2DP connection state callback.
 *
 * State machine:
 *   state=0 (ESP_A2D_CONNECTION_STATE_DISCONNECTED) — no source connected.
 *            Clears bt_connected; sets bt_state_changed for display update.
 *   state=1 (ESP_A2D_CONNECTION_STATE_CONNECTING)   — connection in progress.
 *            Clears bt_connected (not yet fully connected).
 *   state=2 (ESP_A2D_CONNECTION_STATE_CONNECTED)    — source connected.
 *            Sets bt_connected; sets bt_state_changed for display update.
 */
// a2dp bt connection callback
void a2dp_connection_state_changed(esp_a2d_connection_state_t state, void *ptr){    // callback for bluetooth connection state change
  if(state==2){                                                                     // state=0 -> disconnected, state=1 -> connecting, state=2 -> connected
    setFlag(bt_connected);
  } else {
    clearFlag(bt_connected);
  }
  setFlag(bt_state_changed);
}

/* a2dp_audio_state_changed() — A2DP audio state callback.
 *
 * State machine:
 *   state=1 (ESP_A2D_AUDIO_STATE_STOPPED)  — audio stream stopped / paused.
 *            Clears bt_audio_playing; sets audio_state_changed.
 *            A2DP_EventHandler will mute the PCM5102A (PCM_MUTE_CTL LOW).
 *   state=2 (ESP_A2D_AUDIO_STATE_STARTED)  — audio stream started / playing.
 *            Sets bt_audio_playing; sets audio_state_changed.
 *            A2DP_EventHandler will unmute the PCM5102A (PCM_MUTE_CTL HIGH).
 */
// a2dp audio state callback
void a2dp_audio_state_changed(esp_a2d_audio_state_t state, void *ptr){  // callback for audio playing/stopped
  if(state==2){                                                         //  state=1 -> stopped, state=2 -> playing
    setFlag(bt_audio_playing);
  } else {
    clearFlag(bt_audio_playing);
  }
  setFlag(audio_state_changed);
}

/* a2dp_init() — initialise the Bluetooth A2DP sink and I2S output.
 *
 * I2S pin assignment (PCM5102A DAC):
 *   BCK  (bit clock)  = GPIO 26
 *   WS   (word select / LRCK) = GPIO 25
 *   DATA (serial data)        = GPIO 22
 *
 * Volume control: A2DPNoVolumeControl is installed to prevent the A2DP stack
 * from scaling down audio samples.  The car amplifier provides volume control
 * at the hardware level; software volume reduction would degrade dynamic range.
 *
 * Auto-reconnect: enabled with a 2000 ms initial delay and 500 ms between
 * retries, so the phone reconnects automatically the next time the car starts.
 *
 * After init:
 *   - a2dp_started flag is set.
 *   - disp_mode is set to 0 (audio metadata).
 *   - A "Waiting for connection…" message is written to the display.
 */
// start A2DP audio service
void a2dp_init(){
  auto i2s_conf=i2s.defaultConfig();
  i2s_conf.pin_bck=I2S_PIN_BCK;
  i2s_conf.pin_ws=I2S_PIN_WS;
  i2s_conf.pin_data=I2S_PIN_DATA;
  i2s.begin(i2s_conf);
  a2dp_sink.set_avrc_metadata_callback(avrc_metadata_callback);
  a2dp_sink.set_avrc_metadata_attribute_mask(ESP_AVRC_MD_ATTR_TITLE | ESP_AVRC_MD_ATTR_ARTIST | ESP_AVRC_MD_ATTR_ALBUM);
  a2dp_sink.set_on_connection_state_changed(a2dp_connection_state_changed);
  a2dp_sink.set_on_audio_state_changed(a2dp_audio_state_changed);
  a2dp_sink.set_volume_control(&noVolumeControl);
  a2dp_sink.set_reconnect_delay(500);
  a2dp_sink.set_auto_reconnect(true, 2000);

  a2dp_sink.start(BT_DEVICE_NAME);         // setting up bluetooth audio sink
  setFlag(a2dp_started);
  DEBUG_PRINTLN("A2DP: Started!");
  disp_mode=0;                      // set display mode to audio metadata on boot
  writeTextToDisplay(1, "EHU32 v" EHU32_VERSION, "Bluetooth on", "Waiting for connection...");
}

/* A2DP_EventHandler() — process Bluetooth / A2DP events; called every 10 ms from
 *                        eventHandlerTask (Core 1).
 *
 * Handled events (in order):
 *
 *   ehu_started && !a2dp_started
 *     — First time the radio is detected on the bus.  Calls a2dp_init() to start
 *       Bluetooth.  This deferred start avoids running Bluetooth during the CAN
 *       setup phase where timing is critical.
 *
 *   DIS_forceUpdate && disp_mode==0 && CAN_allowAutoRefresh && bt_audio_playing
 *     — New metadata has arrived and "Aux" is active.  Calls writeTextToDisplay()
 *       to push the track info to the car display.
 *
 *   bt_state_changed && disp_mode==0
 *     — BT connection state changed.  Shows "Bluetooth connected" (with peer name)
 *       or "Bluetooth disconnected" on the display.  Also applies max volume (127)
 *       on connection as a workaround for some sources that connect at low volume.
 *
 *   audio_state_changed && bt_connected && disp_mode==0
 *     — Audio play/pause state changed.  If playing, unmutes the DAC and forces
 *       a metadata display update.  If paused/stopped, mutes the DAC and shows
 *       "Bluetooth connected / Paused".
 */
// handles events such as connecion/disconnection and audio play/pause
void A2DP_EventHandler(){
  if(checkFlag(ehu_started) && !checkFlag(a2dp_started)){             // this enables bluetooth A2DP service only after the radio is started
    a2dp_init();
  }

  if(checkFlag(DIS_forceUpdate) && disp_mode==0 && checkFlag(CAN_allowAutoRefresh) && checkFlag(bt_audio_playing)){                       // handles data processing for A2DP AVRC data events
    writeTextToDisplay();
  }

  if(checkFlag(bt_state_changed) && disp_mode==0){                                   // mute external DAC when not playing
    if(checkFlag(bt_connected)){
      a2dp_sink.set_volume(127);        // workaround to ensure max volume being applied on successful connection
      writeTextToDisplay(1, "", "Bluetooth connected", (char*)a2dp_sink.get_peer_name());
    } else {
      writeTextToDisplay(1, "", "Bluetooth disconnected", "");
    }
    clearFlag(bt_state_changed);
  }

  if(checkFlag(audio_state_changed) && checkFlag(bt_connected) && disp_mode==0){      // mute external DAC when not playing; bt_connected ensures no "Connected, paused" is displayed, seems that the audio_state_changed callback comes late
    if(checkFlag(bt_audio_playing)){
      digitalWrite(PCM_MUTE_CTL, HIGH);
      setFlag(DIS_forceUpdate);              // force reprinting of audio metadata when the music is playing
    } else {
      digitalWrite(PCM_MUTE_CTL, LOW);
      writeTextToDisplay(1, "Bluetooth connected", "Paused", "");
    }
    clearFlag(audio_state_changed);
  }
}

/* a2dp_shutdown() — gracefully stop Bluetooth and restart the ESP32.
 *
 * Triggered by canProcessTask when CAN ID 0x501 with data[3]=0x18 is received,
 * which indicates the radio/head unit is shutting down.
 *
 * Note: ESP.restart() is used as a crude workaround because calling
 *       a2dp_sink.end() followed by a2dp_sink.start() causes Bluetooth stack
 *       issues that prevent clean reconnection.  The restart ensures a clean
 *       Bluetooth stack state on the next car start.  The lines after
 *       ESP.restart() are unreachable but kept for documentation purposes.
 */
// ID 0x501 DB3 0x18 indicates imminent shutdown of the radio and display; disconnect from source
void a2dp_shutdown(){
  vTaskSuspend(canMessageDecoderTaskHandle);
  //vTaskSuspend(canWatchdogTaskHandle);
  ESP.restart();                            // very crude workaround until I find a better way to deal with reconnection problems after end() is called
  delay(1000);
  a2dp_sink.disconnect();
  a2dp_sink.end();
  clearFlag(ehu_started);                            // so it is possible to restart and reconnect the source afterwards in the rare case radio is shutdown but ESP32 is still powered up
  clearFlag(a2dp_started);                           // while extremely unlikely to happen in the vehicle, this comes handy for debugging on my desk setup
  DEBUG_PRINTLN("CAN: EHU went down! Disconnecting A2DP.");
}
