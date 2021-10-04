#ifndef CONFIG_H
#define CONFIG_H

#define MIDI_CHANNEL 0 // default channel # is 0
#define MIDI_NOTE_OFF_VELOCITY 64
// Set the value of first note, C is a good choice. Lowest C is 0.
// 36 is a good default. 48 is a high range. Set to 24 for a bass machine.
#define FIRST_MIDI_NOTE 36

#define KEY_VOICE_SELECT_0 14
#define KEY_VOICE_SELECT_1 15
#define KEY_VOICE_SELECT_2 22
#define KEY_VOICE_SELECT_3 23
#define KEY_VOICE_SELECT_4 30
#define KEY_VOICE_SELECT_5 31

#define KEY_PATTERN_LEN 29
#define KEY_PATTERN_POS 21
#define KEY_COPY 4
#define KEY_PASTE 5
#define KEY_CLEAR 12
#define KEY_UNDO 13
#define KEY_ROTATE 28

#define KEY_SEQ_POS_0 0
#define KEY_SEQ_POS_1 1
#define KEY_SEQ_POS_2 2
#define KEY_SEQ_POS_3 3
#define KEY_SEQ_POS_4 8
#define KEY_SEQ_POS_5 9
#define KEY_SEQ_POS_6 10
#define KEY_SEQ_POS_7 11

#define KEY_SEQ_POS_8 16
#define KEY_SEQ_POS_9 17
#define KEY_SEQ_POS_10 18
#define KEY_SEQ_POS_11 19
#define KEY_SEQ_POS_12 24
#define KEY_SEQ_POS_13 25
#define KEY_SEQ_POS_14 26
#define KEY_SEQ_POS_15 27

#define VOICES 6

#endif
