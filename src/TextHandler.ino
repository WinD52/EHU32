/*
 * TextHandler.ino — UTF-8 to UTF-16 conversion and display text formatting
 *
 * This file handles all text processing required to build display messages:
 *   1. Converts UTF-8 encoded strings (from A2DP metadata, sensor readings,
 *      or status messages) to the UTF-16 Big-Endian encoding required by the
 *      car's display units.
 *   2. Applies display-specific formatting escape sequences (alignment, font).
 *   3. Assembles the complete ISO 15765-2 payload in DisplayMsg[].
 *
 * Display protocol:
 *   The car display accepts a proprietary text display command (0x40 00 xx 03 …)
 *   sent as an ISO 15765-2 multi-packet CAN message.  The payload contains
 *   tagged fields:
 *     0x10 — title/main  (middle line on most displays)
 *     0x11 — album field (upper line)
 *     0x12 — artist field (lower line)
 *   Each field is prefixed with a character count byte, followed by optional
 *   formatting escape sequences and then the UTF-16 encoded text.
 *
 * Supported Unicode ranges (display hardware limitation):
 *   U+0000–U+024F  Basic Latin, Latin-1 Supplement, Latin Extended A/B
 *   U+1E00–U+2C6F  Latin Extended Additional, General Punctuation, etc.
 *   Cyrillic and characters outside these ranges are silently dropped.
 *
 * Dependencies:
 *   - Global DisplayMsg buffer declared in EHU32.ino
 *   - prepareMultiPacket() in CAN.ino
 *
 * Author: PNKP237 — https://github.com/PNKP237/EHU32
 */

/* ─────────────────────────── Display Escape Sequences ───────────────────────────
 * The car display interprets UTF-16 encoded ESC sequences embedded in the text
 * payload to control text alignment and font size.  All sequences use the
 * 7-bit ASCII ESC character (U+001B = 0x001B) followed by '[' (0x005B) and
 * a command suffix, terminated with 'm' (0x006D).
 *
 * Sequences (stored as UTF-16 BE byte pairs):
 *
 *   DIS_leftadjusted[14] = ESC [ f S _ g m
 *     {0x00,0x1B, 0x00,0x5B, 0x00,0x66, 0x00,0x53, 0x00,0x5F, 0x00,0x67, 0x00,0x6D}
 *     Left-aligns the following text.  Used for the main title (middle line)
 *     with the standard/large font.
 *
 *   DIS_smallfont[14] = ESC [ f S _ d m
 *     {0x00,0x1B, 0x00,0x5B, 0x00,0x66, 0x00,0x53, 0x00,0x5F, 0x00,0x64, 0x00,0x6D}
 *     Left-aligns text in a smaller font.  Used for the upper (album) and
 *     lower (artist) lines where space is limited.
 *
 *   DIS_centered[8] = ESC [ c m
 *     {0x00,0x1B, 0x00,0x5B, 0x00,0x63, 0x00,0x6D}
 *     Centers the following text.  Not currently used in the main code path
 *     but available for status messages.
 *
 *   DIS_rightadjusted[8] = ESC [ r m
 *     {0x00,0x1B, 0x00,0x5B, 0x00,0x72, 0x00,0x6D}
 *     Right-aligns the following text.  Not currently used but available.
 *
 * Note: each escape sequence counts as (sizeof/2) characters towards the
 *       field's character count byte in the CAN payload.
 * ──────────────────────────────────────────────────────────────────────────────── */
// below is data required to be included in every line - text formatting is based on those
const char DIS_leftadjusted[14]={0x00,0x1B,0x00,0x5B,0x00,0x66,0x00,0x53,0x00,0x5F,0x00,0x67,0x00,0x6D}, DIS_smallfont[14]={0x00,0x1B,0x00,0x5B,0x00,0x66,0x00,0x53,0x00,0x5F,0x00,0x64,0x00,0x6D}, DIS_centered[8]={0x00, 0x1B, 0x00, 0x5B, 0x00, 0x63, 0x00, 0x6D}, DIS_rightadjusted[8]={0x00, 0x1B, 0x00, 0x5B, 0x00, 0x72, 0x00, 0x6D};

/* utf8_to_utf16() — convert a null-terminated UTF-8 string to UTF-16 BE.
 *
 * Parameters:
 *   utf8_buffer   — source string (e.g. AVRCP metadata, snprintf'd sensor value)
 *   utf16_buffer  — destination buffer (must be at least 2× the UTF-8 length)
 *
 * Returns: the number of UTF-16 code units (characters) written.  The byte
 *          count written to utf16_buffer is (return value × 2).
 *
 * Encoding rules:
 *   1-byte (0xxxxxxx): codepoints U+0000–U+007F — stored as 0x00, byte
 *   2-byte (110xxxxx 10xxxxxx): U+0080–U+07FF
 *   3-byte (1110xxxx 10xxxxxx 10xxxxxx): U+0800–U+FFFF
 *   4-byte (11110xxx …): U+10000–U+10FFFF — stored as UTF-16 surrogate pair
 *     High surrogate: 0xD800 + (codepoint >> 10)
 *     Low surrogate:  0xDC00 + (codepoint & 0x3FF)
 *
 * Character filtering:
 *   Only codepoints in U+0000–U+024F and U+1E00–U+2C6F are written.
 *   All other codepoints are silently skipped.  This matches the character
 *   sets supported by the GM/Vauxhall display units (Western European and
 *   extended Latin alphabets).  Cyrillic (U+0400–U+04FF) is NOT supported.
 *
 * On invalid UTF-8 (unrecognised leading byte), the function returns
 * immediately with the count of characters processed so far.
 */
// converts an UTF-8 buffer to UTF-16, filters out unsupported chars, returns the amount of chars processed
unsigned int utf8_to_utf16(const char* utf8_buffer, char* utf16_buffer){
  unsigned int utf16_bytecount=0;
  while (*utf8_buffer!='\0'){
    uint32_t charint=0;
    if ((*utf8_buffer&0x80)==0x00){
      charint=*utf8_buffer&0x7F;
      utf8_buffer++;
    }
    else if ((*utf8_buffer&0xE0)==0xC0){
      charint=(*utf8_buffer & 0x1F)<<6;
      charint|=(*(utf8_buffer+1)&0x3F);
      utf8_buffer+=2;
    }
    else if ((*utf8_buffer&0xF0)==0xE0){
      charint=(*utf8_buffer&0x0F)<<12;
      charint|=(*(utf8_buffer+1)&0x3F)<<6;
      charint|=(*(utf8_buffer+2)&0x3F);
      utf8_buffer += 3;
    }
    else if ((*utf8_buffer&0xF8)==0xF0){
      charint=(*utf8_buffer&0x07)<<18;
      charint|=(*(utf8_buffer+1)&0x3F)<<12;
      charint|=(*(utf8_buffer+2)&0x3F)<<6;
      charint|=(*(utf8_buffer+3)&0x3F);
      utf8_buffer+=4;
    }
    else {
      return utf16_bytecount/2;
    }
 // --------------------------------------------------------------
    // Обработка символов: латиница — как есть, кириллица — транслитерация
    // --------------------------------------------------------------
    
    // Латинские и поддерживаемые символы — оставляем как есть
    if ((charint>=0x0000 && charint<=0x024F) || (charint>=0x1E00 && charint<=0x2C6F)){
      if (charint>=0x10000) {
        charint-=0x10000;
        utf16_buffer[utf16_bytecount++]=static_cast<char>((charint>>10)+0xD8);
        utf16_buffer[utf16_bytecount++]=static_cast<char>((charint>>2)&0xFF);
        utf16_buffer[utf16_bytecount++]=static_cast<char>(0xDC|((charint>>10)&0x03));
        utf16_buffer[utf16_bytecount++]=static_cast<char>((charint&0x03)<<6);
      }
      else {
        utf16_buffer[utf16_bytecount++]=static_cast<char>((charint>>8)&0xFF);
        utf16_buffer[utf16_bytecount++]=static_cast<char>(charint&0xFF);
      }
    }
    // Кириллица (U+0400 – U+04FF) — транслитерация в латиницу
    else if (charint >= 0x0400 && charint <= 0x04FF) {
      const char* latin = nullptr;
      switch(charint) {
        // ===== ЗАГЛАВНЫЕ БУКВЫ =====
        case 0x0410: latin = "A"; break;    // А
        case 0x0411: latin = "B"; break;    // Б
        case 0x0412: latin = "V"; break;    // В
        case 0x0413: latin = "G"; break;    // Г
        case 0x0414: latin = "D"; break;    // Д
        case 0x0415: latin = "E"; break;    // Е
        case 0x0401: latin = "Yo"; break;   // Ё
        case 0x0416: latin = "Zh"; break;   // Ж
        case 0x0417: latin = "Z"; break;    // З
        case 0x0418: latin = "I"; break;    // И
        case 0x0419: latin = "Y"; break;    // Й
        case 0x041A: latin = "K"; break;    // К
        case 0x041B: latin = "L"; break;    // Л
        case 0x041C: latin = "M"; break;    // М
        case 0x041D: latin = "N"; break;    // Н
        case 0x041E: latin = "O"; break;    // О
        case 0x041F: latin = "P"; break;    // П
        case 0x0420: latin = "R"; break;    // Р
        case 0x0421: latin = "S"; break;    // С
        case 0x0422: latin = "T"; break;    // Т
        case 0x0423: latin = "U"; break;    // У
        case 0x0424: latin = "F"; break;    // Ф
        case 0x0425: latin = "Kh"; break;   // Х
        case 0x0426: latin = "Ts"; break;   // Ц
        case 0x0427: latin = "Ch"; break;   // Ч
        case 0x0428: latin = "Sh"; break;   // Ш
        case 0x0429: latin = "Sh"; break;   // Щ
        case 0x042A: latin = ""; break;     // Ъ
        case 0x042B: latin = "Y"; break;    // Ы
        case 0x042C: latin = ""; break;     // Ь
        case 0x042D: latin = "E"; break;    // Э
        case 0x042E: latin = "Yu"; break;   // Ю
        case 0x042F: latin = "Ya"; break;   // Я
        // ===== СТРОЧНЫЕ БУКВЫ =====
        case 0x0430: latin = "a"; break;    // а
        case 0x0431: latin = "b"; break;    // б
        case 0x0432: latin = "v"; break;    // в
        case 0x0433: latin = "g"; break;    // г
        case 0x0434: latin = "d"; break;    // д
        case 0x0435: latin = "e"; break;    // е
        case 0x0451: latin = "yo"; break;   // ё
        case 0x0436: latin = "zh"; break;   // ж
        case 0x0437: latin = "z"; break;    // з
        case 0x0438: latin = "i"; break;    // и
        case 0x0439: latin = "y"; break;    // й
        case 0x043A: latin = "k"; break;    // к
        case 0x043B: latin = "l"; break;    // л
        case 0x043C: latin = "m"; break;    // м
        case 0x043D: latin = "n"; break;    // н
        case 0x043E: latin = "o"; break;    // о
        case 0x043F: latin = "p"; break;    // п
        case 0x0440: latin = "r"; break;    // р
        case 0x0441: latin = "s"; break;    // с
        case 0x0442: latin = "t"; break;    // т
        case 0x0443: latin = "u"; break;    // у
        case 0x0444: latin = "f"; break;    // ф
        case 0x0445: latin = "kh"; break;   // х
        case 0x0446: latin = "ts"; break;   // ц
        case 0x0447: latin = "ch"; break;   // ч
        case 0x0448: latin = "sh"; break;   // ш
        case 0x0449: latin = "sh"; break;   // щ
        case 0x044A: latin = ""; break;     // ъ
        case 0x044B: latin = "y"; break;    // ы
        case 0x044C: latin = ""; break;     // ь
        case 0x044D: latin = "e"; break;    // э
        case 0x044E: latin = "yu"; break;   // ю
        case 0x044F: latin = "ya"; break;   // я
        default: latin = ""; break;
      }
      // Записываем латинскую замену в UTF-16
      if (latin != nullptr) {
        for (const char* p = latin; *p != '\0'; p++) {
          utf16_buffer[utf16_bytecount++] = 0x00;
          utf16_buffer[utf16_bytecount++] = *p;
        }
      }
    }
    // Остальные символы (не латиница и не кириллица) — игнорируем
  }
  return utf16_bytecount/2;  // amount of chars processed
}

/* processDisplayMessage() — build a complete display payload in DisplayMsg[].
 *
 * Parameters:
 *   upper_line_buffer — UTF-8 text for the upper (album) line; nullptr = omit.
 *   middle_line_buffer — UTF-8 text for the middle (title) line; nullptr = omit.
 *   lower_line_buffer  — UTF-8 text for the lower (artist) line; nullptr = omit.
 *
 * Returns: total number of bytes written to DisplayMsg[] (= ISO 15765-2 payload
 *          size passed to prepareMultiPacket()).
 *
 * Message structure written to DisplayMsg[]:
 *   [0]   = total payload size in bytes (filled in at the end)
 *   [1]   = 0x40  (display write command)
 *   [2]   = 0x00
 *   [3]   = payload size − 3 (filled in at the end)
 *   [4]   = 0x03  (text display type)
 *   — Title field (middle line) —
 *   [5]   = 0x10  (field tag: title)
 *   [6]   = character count for field 0x10 (including formatting chars)
 *   [7…]  = DIS_leftadjusted escape (7 UTF-16 chars) + UTF-16 title text
 *   — Album field (upper line) —
 *   [n]   = 0x11  (field tag: album)
 *   [n+1] = character count for field 0x11
 *   [n+2…]= DIS_smallfont escape (7 UTF-16 chars) + UTF-16 album text
 *   — Artist field (lower line) —
 *   [m]   = 0x12  (field tag: artist)
 *   [m+1] = character count for field 0x12
 *   [m+2…]= DIS_smallfont escape (7 UTF-16 chars) + UTF-16 artist text
 *
 * Special cases:
 *   - If the middle line converts to 0 characters (empty or unsupported chars),
 *     it is replaced with "Playing" to indicate audio is active.
 *   - Formatting escape sequences are omitted for lines with 0 or 1 characters
 *     to save CAN frame bandwidth.
 *   - If the total byte count would produce a completely full last CAN frame
 *     (i.e. (last_byte_written+1) % 7 == 0), a padding space (0x00 0x20) is
 *     appended.  This is because some displays ignore messages whose last
 *     consecutive frame is completely full (see EHU32 wiki for details).
 *   - Maximum payload size is capped at 254 bytes (DisplayMsg[0] is a single
 *     byte; excess data is truncated but causes no harm).
 */
// converts UTF-8 strings from arguments to real UTF-16, then compiles a full display message with formatting; returns total bytes written as part of message payload
int processDisplayMessage(char* upper_line_buffer, char* middle_line_buffer, char* lower_line_buffer){
  static char utf16_middle_line[256], utf16_lower_line[256], utf16_upper_line[256];
  int upper_line_buffer_length=0, middle_line_buffer_length=0, lower_line_buffer_length=0;
  if(upper_line_buffer!=nullptr){                                           // converting UTF-8 strings to UTF-16 and calculating string lengths to keep track of processed data
    upper_line_buffer_length=utf8_to_utf16(upper_line_buffer, utf16_upper_line);
  }
  if(middle_line_buffer!=nullptr){
    middle_line_buffer_length=utf8_to_utf16(middle_line_buffer, utf16_middle_line);
    if(middle_line_buffer_length==0 || (middle_line_buffer_length==1 && utf16_middle_line[1]==0x20)){ // -> empty line (or unsupported chars)
      snprintf(middle_line_buffer, 8, "Playing");    // if the middle line was to be blank you can at least tell that there's audio being played
      middle_line_buffer_length=utf8_to_utf16(middle_line_buffer, utf16_middle_line);   // do this once again for the new string
    }
  }
  if(lower_line_buffer!=nullptr){
    lower_line_buffer_length=utf8_to_utf16(lower_line_buffer, utf16_lower_line);
  }

  #ifdef DEBUG_STRINGS               // debug stuff
    Serial.printf("\nTitle length: %d", middle_line_buffer_length);
    Serial.printf("\nAlbum length: %d", upper_line_buffer_length);
    Serial.printf("\nArtist length: %d", lower_line_buffer_length);
    Serial.println("\nTitle buffer in UTF-8:");
    for(int i=0;i<middle_line_buffer_length;i++){
      Serial.printf(" %02X", middle_line_buffer[i]);
    }
    Serial.println("\nTitle buffer in UTF-16:");
    for(int i=0;i<(middle_line_buffer_length*2);i++){
      Serial.printf(" %02X", utf16_middle_line[i]);
    }
    Serial.println("\nAlbum buffer in UTF-8:");
    for(int i=0;i<upper_line_buffer_length;i++){
      Serial.printf(" %02X", upper_line_buffer[i]);
    }
    Serial.println("\nAlbum buffer in UTF-16:");
    for(int i=0;i<(upper_line_buffer_length*2);i++){
      Serial.printf(" %02X", utf16_upper_line[i]);
    }
    Serial.println("\nArtist buffer in UTF-8:");
    for(int i=0;i<lower_line_buffer_length;i++){
      Serial.printf(" %02X", lower_line_buffer[i]);
    }
    Serial.println("\nArtist buffer in UTF-16:");
    for(int i=0;i<(lower_line_buffer_length*2);i++){
      Serial.printf(" %02X", utf16_lower_line[i]);
    }
  #endif

  memset(DisplayMsg, 0, sizeof(DisplayMsg));  // clearing the buffer

  //DisplayMsg[0]= message size in bytes
  DisplayMsg[1]=0x40; // COMMAND
  //DisplayMsg[2]=0x00 always
  //DisplayMsg[3]= message size -3
  DisplayMsg[4]=0x03; //type

  int last_byte_written=4;      // this tracks the current position in buffer, 4 because of the first four bytes which go as follows: [size] 40 00 [size-3]
  // SONG TITLE FIELD
  last_byte_written++;
  DisplayMsg[last_byte_written]=0x10;                 // specifying "title" field (middle line, or the only line of text in case of displays such as 1-line GID/BID/TID)
  last_byte_written++;                                                        // we skip DisplayMsg[6], its filled in the end (char count for id 0x10)
  if(middle_line_buffer_length>1){  // if the middle line data is just a space, don't apply formatting - saves 2 frames of data
    memcpy(DisplayMsg+last_byte_written+1, DIS_leftadjusted, sizeof(DIS_leftadjusted));
    last_byte_written+=sizeof(DIS_leftadjusted);
    DisplayMsg[6]=sizeof(DIS_leftadjusted)/2;
  }
  memcpy(DisplayMsg+last_byte_written+1, utf16_middle_line, middle_line_buffer_length*2);
  last_byte_written+=(middle_line_buffer_length*2);

  DisplayMsg[6]+=middle_line_buffer_length;  // this is static, char count = title+(formatting/2)

  int album_count_pos=10;
  // ALBUM FIELD
  last_byte_written++;
  DisplayMsg[last_byte_written]=0x11;             // specifying "album" field (upper line)
  last_byte_written++;
  album_count_pos=last_byte_written;
  if(upper_line_buffer_length>=1){  // if the upper line data is just a space, don't apply formatting - saves 2 frames of data
    memcpy(DisplayMsg+last_byte_written+1, DIS_smallfont, sizeof(DIS_smallfont));
    last_byte_written+=sizeof(DIS_smallfont);
    DisplayMsg[album_count_pos]=sizeof(DIS_smallfont)/2;
  }
  memcpy(DisplayMsg+last_byte_written+1, utf16_upper_line, upper_line_buffer_length*2);
  last_byte_written+=(upper_line_buffer_length*2);
  DisplayMsg[album_count_pos]+=upper_line_buffer_length;

  int artist_count_pos=album_count_pos;
    // ARTIST FIELD
  last_byte_written++;
  DisplayMsg[last_byte_written]=0x12;                             // specifying "artist" field (lower line)
  last_byte_written++;
  artist_count_pos=last_byte_written;
  if(lower_line_buffer_length>=1){  // if the lower line data is just a space, don't apply formatting - saves 2 frames of data
    memcpy(DisplayMsg+last_byte_written+1, DIS_smallfont, sizeof(DIS_smallfont));
    last_byte_written+=sizeof(DIS_smallfont);
    DisplayMsg[artist_count_pos]=sizeof(DIS_smallfont)/2;
  }
  memcpy(DisplayMsg+last_byte_written+1, utf16_lower_line, lower_line_buffer_length*2);
  last_byte_written+=(lower_line_buffer_length*2);
  DisplayMsg[artist_count_pos]+=lower_line_buffer_length;

  if((last_byte_written+1)%7==0){                   // if the amount of bytes were to result in a full packet (ie no unused bytes), add a char to overflow into the next packet
    DisplayMsg[artist_count_pos]+=1;          // workaround because if the packets are full the display would ignore the message. This is explained on the EHU32 wiki
    DisplayMsg[last_byte_written+1]=0x00; DisplayMsg[last_byte_written+2]=0x20;
    last_byte_written+=2;
  }
  if(last_byte_written>254){                      // message size can't be larger than 255 bytes, as the character specifying total payload is an 8 bit value
    last_byte_written=254;                        // we can send that data though, it will just be ignored, no damage is done
  }
  DisplayMsg[0]=last_byte_written+1;         // TOTAL PAYLOAD SIZE based on how many bytes have been written
  DisplayMsg[3]=DisplayMsg[0]-3;               // payload size written as part of the 4000 command
  return last_byte_written+1;                   // return the total message size
}
