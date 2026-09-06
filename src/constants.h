#ifndef CONSTANTS_H
#define CONSTANTS_H

#include "colors.h"
#include "config.h"

// Voice pads in voice order (0..5)
inline constexpr uint32_t VOICE_KEYS[VOICES] = {
    KEY_VOICE_SELECT_0, KEY_VOICE_SELECT_1, KEY_VOICE_SELECT_2,
    KEY_VOICE_SELECT_3, KEY_VOICE_SELECT_4, KEY_VOICE_SELECT_5,
};

inline constexpr uint32_t VOICE_COLORS[VOICES] = {
    COLOR_VOC0, COLOR_VOC1, COLOR_VOC2, COLOR_VOC3, COLOR_VOC4, COLOR_VOC5,
};

inline constexpr uint32_t VOICE_UNSET_COLORS[VOICES] = {
    COLOR_VOC0_UNSET, COLOR_VOC1_UNSET, COLOR_VOC2_UNSET,
    COLOR_VOC3_UNSET, COLOR_VOC4_UNSET, COLOR_VOC5_UNSET,
};

inline constexpr uint32_t VOICE_SET_COLORS[VOICES] = {
    COLOR_VOC0_SET, COLOR_VOC1_SET, COLOR_VOC2_SET,
    COLOR_VOC3_SET, COLOR_VOC4_SET, COLOR_VOC5_SET,
};

inline constexpr uint32_t VOICE_ACCENT_COLORS[VOICES] = {
    COLOR_VOC0_ACCENT, COLOR_VOC1_ACCENT, COLOR_VOC2_ACCENT,
    COLOR_VOC3_ACCENT, COLOR_VOC4_ACCENT, COLOR_VOC5_ACCENT,
};

// voice_key_to_index maps a VOICE pad to its voice, or VOICES for any other
// key, so `voice_key_to_index(key) < VOICES` doubles as an is-voice-key test.
constexpr uint32_t voice_key_to_index(uint32_t key) {
  switch (key) {
  case KEY_VOICE_SELECT_0:
    return 0;
  case KEY_VOICE_SELECT_1:
    return 1;
  case KEY_VOICE_SELECT_2:
    return 2;
  case KEY_VOICE_SELECT_3:
    return 3;
  case KEY_VOICE_SELECT_4:
    return 4;
  case KEY_VOICE_SELECT_5:
    return 5;
  default:
    return VOICES;
  }
}

constexpr uint32_t voice_key_to_color(uint32_t key) {
  uint32_t const idx = voice_key_to_index(key);
  return idx < VOICES ? VOICE_COLORS[idx] : COLOR_OFF;
}

constexpr uint32_t voice_index_to_key(uint32_t idx) {
  return idx < VOICES ? VOICE_KEYS[idx] : 0;
}

constexpr uint32_t voice_index_to_color(uint32_t idx) {
  return idx < VOICES ? VOICE_COLORS[idx] : COLOR_OFF;
}

// Step grid shades for the selected voice: a silent step, a step with a note,
// and a step with an accented note.
constexpr uint32_t voice_index_to_unset_color(uint32_t idx) {
  return idx < VOICES ? VOICE_UNSET_COLORS[idx] : COLOR_OFF;
}

constexpr uint32_t voice_index_to_set_color(uint32_t idx) {
  return idx < VOICES ? VOICE_SET_COLORS[idx] : COLOR_OFF;
}

constexpr uint32_t voice_index_to_accent_color(uint32_t idx) {
  return idx < VOICES ? VOICE_ACCENT_COLORS[idx] : COLOR_OFF;
}

inline constexpr uint32_t step_key[16] = {
    // row 0
    KEY_SEQ_POS_0,
    KEY_SEQ_POS_1,
    KEY_SEQ_POS_2,
    KEY_SEQ_POS_3,
    // row 1
    KEY_SEQ_POS_4,
    KEY_SEQ_POS_5,
    KEY_SEQ_POS_6,
    KEY_SEQ_POS_7,
    // row 2
    KEY_SEQ_POS_8,
    KEY_SEQ_POS_9,
    KEY_SEQ_POS_10,
    KEY_SEQ_POS_11,
    // row 3
    KEY_SEQ_POS_12,
    KEY_SEQ_POS_13,
    KEY_SEQ_POS_14,
    KEY_SEQ_POS_15,
};

constexpr bool is_numpad_key(uint32_t key) {
  return (key >= KEY_SEQ_POS_0 && key <= KEY_SEQ_POS_3) ||
         (key >= KEY_SEQ_POS_4 && key <= KEY_SEQ_POS_7) ||
         (key >= KEY_SEQ_POS_8 && key <= KEY_SEQ_POS_11) ||
         (key >= KEY_SEQ_POS_12 && key <= KEY_SEQ_POS_15);
}

// System common / real time status bytes
constexpr uint8_t _MIDI_MSG_SPP = 0xF2; // song position pointer
constexpr uint8_t _MIDI_MSG_CLOCK = 0xF8;
constexpr uint8_t _MIDI_MSG_START = 0xFA;
constexpr uint8_t _MIDI_MSG_CONT = 0xFB;
constexpr uint8_t _MIDI_MSG_STOP = 0xFC;
constexpr uint8_t _MIDI_MSG_RESET = 0xFF;

// USB-MIDI code index numbers (low nibble of the packet header). They say how
// many bytes of the packet are used and what kind of message it is.
constexpr uint8_t _USB_MIDI_CIN_SYSCOM_3 = 0x03; // 3 byte system common
constexpr uint8_t _USB_MIDI_CIN_SINGLE_5 = 0x05; // 1 byte syscom/sysex end
constexpr uint8_t _USB_MIDI_CIN_NOTE_OFF = 0x08;
constexpr uint8_t _USB_MIDI_CIN_NOTE_ON = 0x09;
constexpr uint8_t _USB_MIDI_CIN_SINGLE = 0x0F; // 1 byte, system real time

// A MIDI beat, the unit of Song Position Pointer, is a sixteenth note.
constexpr uint32_t MIDI_CLOCKS_PER_BEAT = 6;

#endif // CONSTANTS_H
