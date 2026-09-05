#include "Sequencer.h"

Step::Step() { vel = 0; };

Step::Step(uint32_t value) { vel = value; };

Step::Step(const Step &s) { vel = s.vel; };

bool Step::operator==(const Step &s) const { return vel == s.vel; }

Pattern::Pattern() { length = 16; };

Pattern::Pattern(const Pattern &p) {
  length = p.length;
  steps = p.steps;
};

bool Pattern::operator==(const Pattern &p) const {
  return length == p.length && steps == p.steps;
}

void Pattern::shift(int n) {
  int const len = std::min<uint32_t>(length, steps.size());
  if (len < 2) {
    return;
  }
  // Normalise to a left rotation by k in [0, len), which is what std::rotate
  // does: a right shift by n is a left shift by len - n.
  int k = ((-n % len) + len) % len;
  if (k == 0) {
    return;
  }
  std::rotate(steps.begin(), steps.begin() + k, steps.begin() + len);
}

void Pattern::shuffle(uint32_t (*random_below)(uint32_t n)) {
  // Fisher-Yates over the steps within the pattern length.
  uint32_t const len = std::min<uint32_t>(length, steps.size());
  for (uint32_t i = len; i > 1; i--) {
    uint32_t const j = random_below(i);
    std::swap(steps[i - 1], steps[j]);
  }
}

void Pattern::invert() {
  uint32_t const len = std::min<uint32_t>(length, steps.size());
  for (uint32_t i = 0; i < len; i++) {
    steps[i].vel = steps[i].vel == 0 ? DEFAULT_VELOCITY : 0;
  }
}

Voice::Voice() {
  pos = 0;
  pattern_idx = 0;
  is_playing = false;
  seek_pending = false;
}

Pattern *Voice::pattern() { return &patterns[pattern_idx]; }

Step Voice::step(uint32_t idx) { return pattern()->steps[idx]; }

Step Voice::step() { return pattern()->steps[pos]; }

Step Voice::advance() {
  if (seek_pending) {
    seek_pending = false;
  } else {
    pos = (pos + 1) % pattern()->length;
  }
  return step();
}

void Voice::seek(uint32_t step) {
  pos = step % pattern()->length;
  seek_pending = true;
}

void Voice::replace_pattern(const Pattern p) { patterns[pattern_idx] = p; }

UndoBuffer::UndoBuffer() {
  start = 0;
  count = 0;
}

bool UndoBuffer::empty() const { return count == 0; }

void UndoBuffer::clear() {
  start = 0;
  count = 0;
}

const Pattern &UndoBuffer::back() const {
  return entries[(start + count - 1) % capacity];
}

void UndoBuffer::push(const Pattern &p) {
  if (count == capacity) {
    // full, so the oldest entry becomes the newest one
    entries[start] = p;
    start = (start + 1) % capacity;
    return;
  }
  entries[(start + count) % capacity] = p;
  count++;
}

void UndoBuffer::pop() {
  if (count > 0) {
    count--;
  }
}

Sequencer::Sequencer() {
  voice_idx = 0;
  voice = &voices[0];
}
void Sequencer::set_voice(uint32_t idx) {
  voice_idx = idx;
  voice = &voices[idx];
};
