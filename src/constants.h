#ifndef CONSTANTS_H
#define CONSTANTS_H

#include "colors.h"
#include "config.h"

constexpr uint32_t voice_key_to_color(uint32_t key) {
  switch (key) {
  case KEY_VOICE_SELECT_0:
    return COLOR_VOC0;
  case KEY_VOICE_SELECT_1:
    return COLOR_VOC1;
  case KEY_VOICE_SELECT_2:
    return COLOR_VOC2;
  case KEY_VOICE_SELECT_3:
    return COLOR_VOC3;
  case KEY_VOICE_SELECT_4:
    return COLOR_VOC4;
  case KEY_VOICE_SELECT_5:
    return COLOR_VOC5;
  default:
    return 999;
  };
};

constexpr uint32_t voice_index_to_key(uint32_t idx) {
  switch (idx) {
  case 0:
    return KEY_VOICE_SELECT_0;
  case 1:
    return KEY_VOICE_SELECT_1;
  case 2:
    return KEY_VOICE_SELECT_2;
  case 3:
    return KEY_VOICE_SELECT_3;
  case 4:
    return KEY_VOICE_SELECT_4;
  case 5:
    return KEY_VOICE_SELECT_5;
  default:
    return 999;
  };
};

constexpr uint32_t voice_index_to_color(uint32_t idx) {
  switch (idx) {
  case 0:
    return COLOR_VOC0;
  case 1:
    return COLOR_VOC1;
  case 2:
    return COLOR_VOC2;
  case 3:
    return COLOR_VOC3;
  case 4:
    return COLOR_VOC4;
  case 5:
    return COLOR_VOC5;
  default:
    return 999;
  };
};
const uint32_t step_key[16] = {
    // row 0
    KEY_SEQ_POS_0, KEY_SEQ_POS_1, KEY_SEQ_POS_2, KEY_SEQ_POS_3,
    // row 1
    KEY_SEQ_POS_4, KEY_SEQ_POS_5, KEY_SEQ_POS_6, KEY_SEQ_POS_7,
    // row 2
    KEY_SEQ_POS_8, KEY_SEQ_POS_9, KEY_SEQ_POS_10, KEY_SEQ_POS_11,
    // row 3
    KEY_SEQ_POS_12, KEY_SEQ_POS_13, KEY_SEQ_POS_14, KEY_SEQ_POS_15

};

constexpr bool is_numpad_key(int key) {
  return (key >= KEY_SEQ_POS_0 && key <= KEY_SEQ_POS_3) ||
         (key >= KEY_SEQ_POS_4 && key <= KEY_SEQ_POS_7) ||
         (key >= KEY_SEQ_POS_8 && key <= KEY_SEQ_POS_11) ||
         (key >= KEY_SEQ_POS_12 && key <= KEY_SEQ_POS_15);
}

uint8_t static const _MIDI_MSG_START = 0xFA;
uint8_t static const _MIDI_MSG_STOP = 0xFC;
uint8_t static const _MIDI_MSG_CONT = 0xFB;
uint8_t static const _MIDI_MSG_CLOCK = 0xF8;

#endif
