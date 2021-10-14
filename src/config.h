#ifndef CONFIG_H
#define CONFIG_H

#include <cstdint>

int static const MIDI_CHANNEL = 0;
int static const MIDI_NOTE_OFF_VELOCITY = 64;

int static const UNDO_LENGTH = 16;

// Set the value of first note, C is a good choice. Lowest C is 0.
// 36 is a good default. 48 is a high range. Set to 24 for a bass machine.
int static const FIRST_MIDI_NOTE = 36;

/* #define INTERNAL_CLOCK 1 */
int static const BPM = 120; // tempo for internal clock mode
int static const STEP_DURATION = 120;

// 24 = quarter note, 12 = eighth notes, 6 = sixteenth notes, 8 = eight note
// triplets
int static const CLOCK_DIVISION = 6;

int static const KEY_VOICE_SELECT_0 = 14;
int static const KEY_VOICE_SELECT_1 = 15;
int static const KEY_VOICE_SELECT_2 = 22;
int static const KEY_VOICE_SELECT_3 = 23;
int static const KEY_VOICE_SELECT_4 = 30;
int static const KEY_VOICE_SELECT_5 = 31;
int static const KEY_VOICE_SELECT_ALL = 7;

int static const KEY_PATTERN_LEN = 29;
int static const KEY_PATTERN_POS = 21;
int static const KEY_COPY = 4;
int static const KEY_PASTE = 5;
int static const KEY_CLEAR = 12;
int static const KEY_UNDO = 13;
int static const KEY_ROTATE = 28;

int static const KEY_SEQ_POS_0 = 0;
int static const KEY_SEQ_POS_1 = 1;
int static const KEY_SEQ_POS_2 = 2;
int static const KEY_SEQ_POS_3 = 3;
int static const KEY_SEQ_POS_4 = 8;
int static const KEY_SEQ_POS_5 = 9;
int static const KEY_SEQ_POS_6 = 10;
int static const KEY_SEQ_POS_7 = 11;

int static const KEY_SEQ_POS_8 = 16;
int static const KEY_SEQ_POS_9 = 17;
int static const KEY_SEQ_POS_10 = 18;
int static const KEY_SEQ_POS_11 = 19;
int static const KEY_SEQ_POS_12 = 24;
int static const KEY_SEQ_POS_13 = 25;
int static const KEY_SEQ_POS_14 = 26;
int static const KEY_SEQ_POS_15 = 27;

#define VOICES 6

#endif
