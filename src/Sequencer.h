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
  Pattern();
  Pattern(const Pattern &p);
  bool operator==(const Pattern &p) const;
};

// Voice is a collection of patterns
class Voice {
public:
  std::array<Pattern, 16> patterns;
  bool is_playing;                       // a note is currently being played
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

// UndoBuffer holds the most recent patterns for undo. It is a fixed size ring
// buffer over an inline array, so it performs no heap allocation at all: once
// full, pushing overwrites the oldest entry.
class UndoBuffer {
public:
  static constexpr uint32_t capacity = UNDO_LENGTH;
  bool empty() const;          // true when there is nothing to undo
  void clear();                // drop every entry
  const Pattern &back() const; // most recent entry, only valid when !empty()
  void push(const Pattern &p); // add an entry, dropping the oldest when full
  void pop();                  // remove the most recent entry
  UndoBuffer();

private:
  std::array<Pattern, capacity> entries;
  uint32_t start; // index of the oldest entry
  uint32_t count; // number of entries currently in use
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
