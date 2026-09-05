#ifndef _SEQUENCER_H_
#define _SEQUENCER_H_

#include "config.h"
#include <algorithm>
#include <array>
#include <cstdint>

// Step represents a single sequencer step
class Step {
public:
  uint8_t vel; // midi velocity
  Step();
  Step(uint32_t value);
  Step(const Step &s);
  bool operator==(const Step &s) const;
};

// Pattern is a sequence of steps.
class Pattern {
public:
  uint32_t length;            // pattern length, up to 16 steps
  std::array<Step, 16> steps; // pattern data
  // shift moves every step n places to the right (later in time) for n > 0, or
  // left for n < 0, wrapping within the pattern length. Steps past the length
  // are left alone.
  void shift(int n);
  // shuffle puts the steps within the pattern length in a random order.
  // random_below(n) must return a uniform value in [0, n).
  void shuffle(uint32_t (*random_below)(uint32_t n));
  // invert silences every sounding step within the pattern length and turns
  // every silent one into a step at the default velocity.
  void invert();
  // reverse mirrors the steps within the pattern length, so the last step
  // becomes the first.
  void reverse();
  // euclid keeps the sounding steps within the pattern length but spreads
  // them as evenly as possible across it, starting on the first step. The
  // velocities keep their order, so accents travel with the notes.
  void euclid();
  // accent_every accents (ACCENT_VELOCITY) every n-th sounding step within
  // the pattern length, counting from the first step, and drops every other
  // sounding step to DEFAULT_VELOCITY. Silent steps stay silent. n == 0
  // accents nothing.
  void accent_every(uint32_t n);
  // clear_accents drops every accented step to DEFAULT_VELOCITY.
  void clear_accents();
  Pattern();
  Pattern(const Pattern &p);
  bool operator==(const Pattern &p) const;
};

// Voice is a collection of patterns
class Voice {
public:
  std::array<Pattern, 16> patterns;
  bool is_playing;                       // a note is currently being played
  uint8_t playing_note;                  // the note is_playing refers to
  uint8_t note_offset;                   // semitones added to the base note
  uint32_t pattern_idx;                  // current pattern index
  Pattern *pattern();                    // current pattern
  void replace_pattern(const Pattern p); // replace current pattern
  uint32_t pos;                          // current position
  Step advance();                        // advance to next step (see seek)
  Step step();                           // get current step value
  Step step(uint32_t idx);               // get current step value for pos
  // seek moves the play head to step (wrapped to the pattern length) and
  // arms it: the next advance() plays that step instead of the one after it.
  // This is how Start, Song Position Pointer and the POS key land on a step
  // exactly, given that steps are played by advancing onto them.
  void seek(uint32_t step);
  Voice();

private:
  bool seek_pending; // advance() plays pos as is, set by seek()
};

// UndoEntry records one pattern as it was before an edit, and where it lives.
struct UndoEntry {
  uint8_t voice;
  uint8_t pattern_idx;
  bool group_start; // first entry of the group this edit belongs to
  Pattern before;
};

// UndoBuffer is an edit history: a fixed size ring buffer of UndoEntry over an
// inline array, so it performs no heap allocation at all. Entries are grouped,
// and undo restores a whole group at once, so an edit that touches several
// patterns (a swap, an all-voice transform) is undone in one step. The same
// type holds the redo history, where each group is what an undo overwrote.
// The oldest entry is always a group start: when the ring is full, pushing
// drops the oldest group whole rather than leaving a headless tail that would
// otherwise be undone together with whatever came before it.
class UndoBuffer {
public:
  static constexpr uint32_t capacity = UNDO_LENGTH;
  bool empty() const; // true when there is nothing to undo
  void clear();       // drop every entry
  // begin_group makes the next push start a new group. Every push until the
  // following begin_group belongs to that group.
  void begin_group();
  // push records `before` as the state of pattern `pattern_idx` of `voice`
  // prior to an edit, in the current group.
  void push(uint32_t voice, uint32_t pattern_idx, const Pattern &before);
  const UndoEntry &back() const; // most recent entry, only valid when !empty()
  void pop();                    // remove the most recent entry
  UndoBuffer();

private:
  std::array<UndoEntry, capacity> entries;
  uint32_t start;     // index of the oldest entry
  uint32_t count;     // number of entries currently in use
  bool group_pending; // the next push starts a new group
  void drop_oldest_group();
};

// Sequencer is the main data type
class Sequencer {
public:
  std::array<Voice, VOICES> voices; // all voices
  Voice *voice;                     // current voice
  uint32_t voice_idx;               // current voice index
  void set_voice(uint32_t idx);     // set the currenlty active voice by index
  Sequencer();
};

#endif
