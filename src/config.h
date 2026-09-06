#ifndef CONFIG_H
#define CONFIG_H

#include <cstddef>
#include <cstdint>

constexpr uint8_t MIDI_CHANNEL = 0;
constexpr uint8_t MIDI_NOTE_OFF_VELOCITY = 64;
// Velocity given to a step when it is switched on from the pad or by invert.
constexpr uint8_t DEFAULT_VELOCITY = 99;
// Velocity of an accented step (ACCENT + STEP).
constexpr uint8_t ACCENT_VELOCITY = 127;
// Velocity given to a ghost / muted step.
constexpr uint8_t GHOST_VELOCITY = 50;
// FN + STEP 10 (echo) copies every note this many steps later at half
// its velocity. 2 is an eighth note at sixteenth-note steps.
constexpr uint32_t ECHO_STEPS = 2;

// Undo history depth in patterns. An all-voice transform records one entry per
// voice, so this holds a mix of a few of those and many single edits.
constexpr uint32_t UNDO_LENGTH = 256;

// Set the value of first note, C is a good choice. Lowest C is 0.
// 36 is a good default. 48 is a high range. Set to 24 for a bass machine.
constexpr uint8_t FIRST_MIDI_NOTE = 36;
// FN + the selected voice's pad moves that voice's note up by this many
// semitones, or back. 6 reaches the second bank of a 12 pad drum map.
constexpr uint8_t ALT_NOTE_OFFSET = 6;

// =============================================================================
// Playback Path Modifiers
// =============================================================================
// Path modifiers alter how a voice's playhead traverses its pattern steps in
// real time without modifying pattern data.
enum PathModifier : uint16_t {
  PATH_NONE          = 0,
  PATH_PINGPONG      = 1 << 0,  // forward and then backward
  PATH_SPIRAL        = 1 << 1,  // spiral (outside-in)
  PATH_STUTTER       = 1 << 2,  // micro-hesitation / stutter (0, 1, 1, 3)
  PATH_PHASE         = 1 << 3,  // phase slip / rotational drift (+1 step shift each cycle)
  PATH_WEAVE         = 1 << 4,  // local pendulum / weave ("two steps forward, one step back")
  PATH_DRUNKEN       = 1 << 5,  // drunken walk with forward drift / human micro-hesitation
  PATH_BEAT_PINGPONG = 1 << 6,  // quarter-note subdivided ping-pong (0, 1, 1, 0)
  PATH_BROKEN_THIRDS = 1 << 7,  // broken thirds / knight's hop (+2, -1)
  PATH_DOWNBEAT_LOCK = 1 << 8,  // downbeat lock with reversed weak beats (0, 3, 2, 1)
  PATH_PAIR_SWAP     = 1 << 9,  // interleaved / pair-swapped off-beat syncopation (1, 0, 3, 2)
  PATH_TURNAROUND    = 1 << 10, // bar-4 turnaround / auto-fill (last beat reverses)
  PATH_PEDAL         = 1 << 11, // pedal-point / anchor bounce (0, 1, 0, 3)
  PATH_MUTATE        = 1 << 12, // randomly switch between modifiers every 4 bars
};

// Choose which path modifiers are active in the firmware and mapped to Menu pads.
// Modifiers are assigned sequentially to step pads starting at PATH_MODIFIER_FIRST_STEP
// (by default pad index 4, which is Row 1 / Step 5 in the 1-based UI display).
// Up to 12 modifier slots (steps 4..15) across Rows 1, 2, and 3 are supported.
constexpr PathModifier ACTIVE_PATH_MODIFIERS[] = {
    PATH_PINGPONG, // Menu Step 5 (pad 4)
    PATH_SPIRAL,   // Menu Step 6 (pad 5)
    PATH_PHASE,    // Menu Step 7 (pad 6)
    PATH_MUTATE,   // Menu Step 8 (pad 7)
};
constexpr uint32_t NUM_ACTIVE_PATH_MODIFIERS =
    sizeof(ACTIVE_PATH_MODIFIERS) / sizeof(ACTIVE_PATH_MODIFIERS[0]);
constexpr uint32_t PATH_MODIFIER_FIRST_STEP = 4;

// Modifiers that PATH_MUTATE (Step 8) switches between every 4 bars in random order:
constexpr PathModifier MUTATE_PATH_MODIFIERS[] = {
    PATH_STUTTER,
    PATH_WEAVE,
    PATH_DRUNKEN,
    PATH_BEAT_PINGPONG,
    PATH_BROKEN_THIRDS,
    PATH_DOWNBEAT_LOCK,
    PATH_PAIR_SWAP,
    PATH_TURNAROUND,
    PATH_PEDAL,
};
constexpr uint32_t NUM_MUTATE_PATH_MODIFIERS =
    sizeof(MUTATE_PATH_MODIFIERS) / sizeof(MUTATE_PATH_MODIFIERS[0]);

/* #define INTERNAL_CLOCK 1 */
constexpr uint32_t BPM = 120; // tempo for internal clock mode

// 24 = quarter note, 12 = eighth notes, 6 = sixteenth notes, 8 = eight note
// triplets
constexpr uint32_t CLOCK_DIVISION = 6;

constexpr uint32_t KEY_VOICE_SELECT_0 = 14;
constexpr uint32_t KEY_VOICE_SELECT_1 = 15;
constexpr uint32_t KEY_VOICE_SELECT_2 = 22;
constexpr uint32_t KEY_VOICE_SELECT_3 = 23;
constexpr uint32_t KEY_VOICE_SELECT_4 = 30;
constexpr uint32_t KEY_VOICE_SELECT_5 = 31;
constexpr uint32_t KEY_VOICE_SELECT_ALL = 7;

constexpr uint32_t KEY_PATTERN_LEN = 29;
constexpr uint32_t KEY_PATTERN_POS = 21;
constexpr uint32_t KEY_COPY = 4;
constexpr uint32_t KEY_PASTE = 5;
constexpr uint32_t KEY_MENU = 6;
constexpr uint32_t KEY_CLEAR = 12;
constexpr uint32_t KEY_UNDO = 13;
constexpr uint32_t KEY_FN = 28;
constexpr uint32_t KEY_ACCENT = 20;

constexpr uint32_t KEY_SEQ_POS_0 = 0;
constexpr uint32_t KEY_SEQ_POS_1 = 1;
constexpr uint32_t KEY_SEQ_POS_2 = 2;
constexpr uint32_t KEY_SEQ_POS_3 = 3;
constexpr uint32_t KEY_SEQ_POS_4 = 8;
constexpr uint32_t KEY_SEQ_POS_5 = 9;
constexpr uint32_t KEY_SEQ_POS_6 = 10;
constexpr uint32_t KEY_SEQ_POS_7 = 11;

constexpr uint32_t KEY_SEQ_POS_8 = 16;
constexpr uint32_t KEY_SEQ_POS_9 = 17;
constexpr uint32_t KEY_SEQ_POS_10 = 18;
constexpr uint32_t KEY_SEQ_POS_11 = 19;
constexpr uint32_t KEY_SEQ_POS_12 = 24;
constexpr uint32_t KEY_SEQ_POS_13 = 25;
constexpr uint32_t KEY_SEQ_POS_14 = 26;
constexpr uint32_t KEY_SEQ_POS_15 = 27;

#define VOICES 6

#endif // CONFIG_H
