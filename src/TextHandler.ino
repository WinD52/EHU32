/*
 * TextHandler.ino — UTF-8 to UTF-16 conversion and display text formatting
 *
 * Project: EHU32 (Opel MS-CAN Bluetooth Audio Gateway)
 * Original Author: PNKP237 — https://github.com/PNKP237/EHU32
 * Author & Maintainer: WinD52 — https://github.com/WinD52/EHU32
 *
 * Modern Platform Baseline: ESP-IDF 5.3 / Arduino Core 3.1
 *
 * Text processing and formatting engine:
 *   - Converts UTF-8 encoded metadata to UTF-16 Big-Endian (ISO 15765-2 payload)
 *   - Fast O(1) Cyrillic transliteration table in Flash ROM (0 bytes RAM overhead)
 *   - Formats tagged display fields: 0x10 (Title), 0x11 (Album), 0x12 (Artist)
 *   - Injects Opel GID/CID font and alignment escape sequences (DIS_leftadjusted / DIS_smallfont)
 *   - Implements hardware-specific 7-byte packet padding workaround
 */

#include "config.h"

// ============================================================================
// Служебные ESC-последовательности форматирования шрифтов дисплеев Opel GID/CID
// ============================================================================
const char DIS_leftadjusted[14] = {0x00, 0x1B, 0x00, 0x5B, 0x00, 0x66, 0x00, 0x53, 0x00, 0x5F, 0x00, 0x67, 0x00, 0x6D}, 
           DIS_smallfont[14]    = {0x00, 0x1B, 0x00, 0x5B, 0x00, 0x66, 0x00, 0x53, 0x00, 0x5F, 0x00, 0x64, 0x00, 0x6D}, 
           DIS_centered[8]      = {0x00, 0x1B, 0x00, 0x5B, 0x00, 0x63, 0x00, 0x6D}, 
           DIS_rightadjusted[8] = {0x00, 0x1B, 0x00, 0x5B, 0x00, 0x72, 0x00, 0x6D};

// ============================================================================
// Статические таблицы транслитерации кириллицы в Flash ROM (O(1) доступ)
// ============================================================================

// Заглавные буквы: А (0x0410) ... Я (0x042F)
static const char* const cyr_upper[32] = {
  "A", "B", "V", "G", "D", "E", "Zh", "Z",
  "I", "Y", "K", "L", "M", "N", "O", "P",
  "R", "S", "T", "U", "F", "Kh", "Ts", "Ch",
  "Sh", "Sh", "", "Y", "", "E", "Yu", "Ya"
};

// Строчные буквы: а (0x0430) ... я (0x044F)
static const char* const cyr_lower[32] = {
  "a", "b", "v", "g", "d", "e", "zh", "z",
  "i", "y", "k", "l", "m", "n", "o", "p",
  "r", "s", "t", "u", "f", "kh", "ts", "ch",
  "sh", "sh", "", "y", "", "e", "yu", "ya"
};

// Быстрый табличный поиск замены для кириллического символа
static inline const char* get_cyr_translit(uint32_t cp) {
  if (cp >= 0x0410 && cp <= 0x042F) return cyr_upper[cp - 0x0410];
  if (cp >= 0x0430 && cp <= 0x044F) return cyr_lower[cp - 0x0430];
  if (cp == 0x0401) return "Yo";
  if (cp == 0x0451) return "yo";
  return nullptr;
}

// ============================================================================
// utf8_to_utf16 — Конвертация UTF-8 в UTF-16 BE с транслитерацией кириллицы
// ============================================================================
unsigned int utf8_to_utf16(const char* utf8_buffer, char* utf16_buffer, size_t utf16_max_bytes){
  unsigned int utf16_bytecount = 0;
  while (*utf8_buffer != '\0' && (utf16_bytecount + 4) < utf16_max_bytes){
    uint32_t charint = 0;
    if ((*utf8_buffer & 0x80) == 0x00){
      charint = *utf8_buffer & 0x7F;
      utf8_buffer++;
    }
    else if ((*utf8_buffer & 0xE0) == 0xC0){
      charint = (*utf8_buffer & 0x1F) << 6;
      charint |= (*(utf8_buffer + 1) & 0x3F);
      utf8_buffer += 2;
    }
    else if ((*utf8_buffer & 0xF0) == 0xE0){
      charint = (*utf8_buffer & 0x0F) << 12;
      charint |= (*(utf8_buffer + 1) & 0x3F) << 6;
      charint |= (*(utf8_buffer + 2) & 0x3F);
      utf8_buffer += 3;
    }
    else if ((*utf8_buffer & 0xF8) == 0xF0){
      charint = (*utf8_buffer & 0x07) << 18;
      charint |= (*(utf8_buffer + 1) & 0x3F) << 12;
      charint |= (*(utf8_buffer + 2) & 0x3F) << 6;
      charint |= (*(utf8_buffer + 3) & 0x3F);
      utf8_buffer += 4;
    }
    else {
      return utf16_bytecount / 2;
    }

    // 1. Латинские и поддерживаемые расширенные символы (U+0000..U+024F, U+1E00..U+2C6F)
    if ((charint >= 0x0000 && charint <= 0x024F) || (charint >= 0x1E00 && charint <= 0x2C6F)){
      if (charint >= 0x10000) {
        charint -= 0x10000;
        utf16_buffer[utf16_bytecount++] = static_cast<char>((charint >> 10) + 0xD8);
        utf16_buffer[utf16_bytecount++] = static_cast<char>((charint >> 2) & 0xFF);
        utf16_buffer[utf16_bytecount++] = static_cast<char>(0xDC | ((charint >> 10) & 0x03));
        utf16_buffer[utf16_bytecount++] = static_cast<char>((charint & 0x03) << 6);
      }
      else {
        utf16_buffer[utf16_bytecount++] = static_cast<char>((charint >> 8) & 0xFF);
        utf16_buffer[utf16_bytecount++] = static_cast<char>(charint & 0xFF);
      }
    }
    // 2. Кириллица (U+0400..U+04FF) — мгновенная табличная транслитерация в латиницу
    else if (charint >= 0x0400 && charint <= 0x04FF) {
      const char* latin = get_cyr_translit(charint);
      if (latin != nullptr) {
        for (const char* p = latin; *p != '\0'; p++) {
          if (utf16_bytecount + 2 >= utf16_max_bytes) break;
          utf16_buffer[utf16_bytecount++] = 0x00;
          utf16_buffer[utf16_bytecount++] = *p;
        }
      }
    }
  }
  return utf16_bytecount / 2;
}

// ============================================================================
// processDisplayMessage — Формирование полного 3-строчного буфера DisplayMsg
// ============================================================================
int processDisplayMessage(char* upper_line_buffer, char* middle_line_buffer, char* lower_line_buffer){
  static char utf16_middle_line[256], utf16_lower_line[256], utf16_upper_line[256];
  int upper_line_buffer_length = 0, middle_line_buffer_length = 0, lower_line_buffer_length = 0;

  if(upper_line_buffer != nullptr){
    upper_line_buffer_length = utf8_to_utf16(upper_line_buffer, utf16_upper_line, sizeof(utf16_upper_line));
  }
  if(middle_line_buffer != nullptr){
    middle_line_buffer_length = utf8_to_utf16(middle_line_buffer, utf16_middle_line, sizeof(utf16_middle_line));
    if(middle_line_buffer_length == 0 || (middle_line_buffer_length == 1 && utf16_middle_line[1] == 0x20)){
      static char playing_fallback[] = "Playing";
      middle_line_buffer_length = utf8_to_utf16(playing_fallback, utf16_middle_line, sizeof(utf16_middle_line));
    }
  }
  if(lower_line_buffer != nullptr){
    lower_line_buffer_length = utf8_to_utf16(lower_line_buffer, utf16_lower_line, sizeof(utf16_lower_line));
  }

  memset(DisplayMsg, 0, sizeof(DisplayMsg));

  DisplayMsg[1] = 0x40; // Команда записи текста дисплея
  DisplayMsg[4] = 0x03; // Тип полезной нагрузки (текст)

  int last_byte_written = 4;

  // ПОЛЕ НАЗВАНИЯ ТРЕКА / TITLE (0x10) — Средняя строка GID/CID
  last_byte_written++;
  DisplayMsg[last_byte_written] = 0x10;
  last_byte_written++;
  if(middle_line_buffer_length > 1){
    memcpy(DisplayMsg + last_byte_written + 1, DIS_leftadjusted, sizeof(DIS_leftadjusted));
    last_byte_written += sizeof(DIS_leftadjusted);
    DisplayMsg[6] = sizeof(DIS_leftadjusted) / 2;
  }
  memcpy(DisplayMsg + last_byte_written + 1, utf16_middle_line, middle_line_buffer_length * 2);
  last_byte_written += (middle_line_buffer_length * 2);

  DisplayMsg[6] += middle_line_buffer_length;

  int album_count_pos = 10;
  // ПОЛЕ АЛЬБОМА / ALBUM (0x11) — Верхняя строка GID/CID
  last_byte_written++;
  DisplayMsg[last_byte_written] = 0x11;
  last_byte_written++;
  album_count_pos = last_byte_written;
  if(upper_line_buffer_length >= 1){
    memcpy(DisplayMsg + last_byte_written + 1, DIS_smallfont, sizeof(DIS_smallfont));
    last_byte_written += sizeof(DIS_smallfont);
    DisplayMsg[album_count_pos] = sizeof(DIS_smallfont) / 2;
  }
  memcpy(DisplayMsg + last_byte_written + 1, utf16_upper_line, upper_line_buffer_length * 2);
  last_byte_written += (upper_line_buffer_length * 2);
  DisplayMsg[album_count_pos] += upper_line_buffer_length;

  int artist_count_pos = album_count_pos;
  // ПОЛЕ ИСПОЛНИТЕЛЯ / ARTIST (0x12) — Нижняя строка GID/CID
  last_byte_written++;
  DisplayMsg[last_byte_written] = 0x12;
  last_byte_written++;
  artist_count_pos = last_byte_written;
  if(lower_line_buffer_length >= 1){
    memcpy(DisplayMsg + last_byte_written + 1, DIS_smallfont, sizeof(DIS_smallfont));
    last_byte_written += sizeof(DIS_smallfont);
    DisplayMsg[artist_count_pos] = sizeof(DIS_smallfont) / 2;
  }
  memcpy(DisplayMsg + last_byte_written + 1, utf16_lower_line, lower_line_buffer_length * 2);
  last_byte_written += (lower_line_buffer_length * 2);
  DisplayMsg[artist_count_pos] += lower_line_buffer_length;

  // Хак дисплеев Opel: защита от зависания при кратности кадра 7 байтам
  if((last_byte_written + 1) % 7 == 0){
    DisplayMsg[artist_count_pos] += 1;
    DisplayMsg[last_byte_written + 1] = 0x00; 
    DisplayMsg[last_byte_written + 2] = 0x20;
    last_byte_written += 2;
  }
  if(last_byte_written > 254){
    last_byte_written = 254;
  }
  DisplayMsg[0] = last_byte_written + 1; // Общий размер полезной нагрузки (ISO-TP Length)
  DisplayMsg[3] = DisplayMsg[0] - 3;
  return last_byte_written + 1;
}
