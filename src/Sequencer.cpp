#include "Sequencer.h"

Step::Step() { vel = 0; };

Step::Step(uint32_t value) { vel = value; };

Step::Step(const Step &s) { vel = s.vel; };

Pattern::Pattern() { length = 16; };

Pattern::Pattern(const Pattern &p) {
  length = p.length;
  steps = p.steps;
};

Voice::Voice() {
  pos = 0;
  pattern_idx = 0;
  is_playing = false;
}

Step Voice::step(uint32_t idx) { return pattern()->steps[idx]; }

Step Voice::step() { return pattern()->steps[pos]; }

Step Voice::advance() {
  pos = (pos + 1) % pattern()->length;
  return step();
}

void Voice::replace_pattern(const Pattern p) { patterns[pattern_idx] = p; }

Sequencer::Sequencer() {
  voice_idx = 0;
  voice = &voices[0];
}
void Sequencer::set_voice(uint32_t idx) {
  voice_idx = idx;
  voice = &voices[idx];
};
