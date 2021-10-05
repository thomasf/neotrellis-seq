#ifndef _SEQUENCER_H_
#define _SEQUENCER_H_

#include "config.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

// Step represents a single sequencer step
class Step {
public:
  uint8_t vel; // midi velocity
  Step();
  Step(uint32_t value);
  Step(const Step &s);
};

// Pattern is a sequence of steps.
class Pattern {
public:
  uint32_t length;            // pattern length, up to 16 steps
  std::array<Step, 16> steps; // pattern data
  Pattern();
  Pattern(const Pattern &p);
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
  Step advance();                        // advace to next step
  Step step();                           // get current step value
  Step step(uint32_t idx);               // get current step value for pos
  Voice();
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
