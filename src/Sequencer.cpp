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

void Pattern::reverse() {
  uint32_t const len = std::min<uint32_t>(length, steps.size());
  std::reverse(steps.begin(), steps.begin() + len);
}

void Pattern::euclid() {
  uint32_t const len = std::min<uint32_t>(length, steps.size());
  std::array<uint8_t, 16> vels;
  uint32_t k = 0;
  for (uint32_t i = 0; i < len; i++) {
    if (steps[i].vel > 0) {
      vels[k++] = steps[i].vel;
    }
    steps[i].vel = 0;
  }
  if (k == 0) {
    return;
  }
  // Bresenham form of the Euclidean rhythm E(k, len): step i sounds when the
  // running total i * k crosses a multiple of len, which lands k onsets as
  // evenly as integers allow with the first one on step 0.
  uint32_t next = 0;
  for (uint32_t i = 0; i < len; i++) {
    if ((i * k) % len < k) {
      steps[i].vel = vels[next++];
    }
  }
}

void Pattern::accent_every(uint32_t n) {
  uint32_t const len = std::min<uint32_t>(length, steps.size());
  for (uint32_t i = 0; i < len; i++) {
    if (steps[i].vel == 0) {
      continue;
    }
    bool const on_grid = n > 0 && i % n == 0;
    steps[i].vel = on_grid ? ACCENT_VELOCITY : DEFAULT_VELOCITY;
  }
}

void Pattern::clear_accents() {
  uint32_t const len = std::min<uint32_t>(length, steps.size());
  for (uint32_t i = 0; i < len; i++) {
    if (steps[i].vel >= ACCENT_VELOCITY) {
      steps[i].vel = DEFAULT_VELOCITY;
    }
  }
}

Voice::Voice() {
  pos = 0;
  pattern_idx = 0;
  is_playing = false;
  playing_note = 0;
  note_offset = 0;
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
  group_pending = true;
}

bool UndoBuffer::empty() const { return count == 0; }

void UndoBuffer::clear() {
  start = 0;
  count = 0;
  group_pending = true;
}

void UndoBuffer::begin_group() { group_pending = true; }

const UndoEntry &UndoBuffer::back() const {
  return entries[(start + count - 1) % capacity];
}

void UndoBuffer::push(uint32_t voice, uint32_t pattern_idx,
                      const Pattern &before) {
  if (count == capacity) {
    drop_oldest_group();
  }
  UndoEntry &e = entries[(start + count) % capacity];
  e.voice = voice;
  e.pattern_idx = pattern_idx;
  e.group_start = group_pending;
  e.before = before;
  count++;
  group_pending = false;
}

void UndoBuffer::pop() {
  if (count > 0) {
    count--;
  }
}

void UndoBuffer::drop_oldest_group() {
  // The oldest entry is a group start; drop it and everything up to the next.
  do {
    start = (start + 1) % capacity;
    count--;
  } while (count > 0 && !entries[start].group_start);
}

Sequencer::Sequencer() {
  voice_idx = 0;
  voice = &voices[0];
}
// sounds_like reports whether `a` and `b` sound on the same steps within
// `len`, ignoring velocity.
static bool sounds_like(const Pattern &a, const Pattern &b, uint32_t len) {
  for (uint32_t i = 0; i < len; i++) {
    if ((a.steps[i].vel > 0) != (b.steps[i].vel > 0)) {
      return false;
    }
  }
  return true;
}

void Sequencer::fill_empty(uint32_t voice) {
  Pattern &target = *voices[voice].pattern();
  uint32_t const len = std::min<uint32_t>(target.length, target.steps.size());

  // activity[i] sums, over the other voices, the weight of each voice that
  // sounds on step i. Patterns of other lengths are read wrapped, as they play
  // during the first pass of the target pattern. sparsest is the note count
  // of the other voice with the fewest notes, which sets how many notes a
  // fallback fill places.
  std::array<uint32_t, 16> activity{};
  uint32_t sparsest = UINT32_MAX;
  for (uint32_t other = 0; other < VOICES; other++) {
    if (other == voice) {
      continue;
    }
    Pattern const &p = *voices[other].pattern();
    uint32_t const plen = std::min<uint32_t>(p.length, p.steps.size());
    if (plen == 0) {
      continue;
    }
    uint32_t sounding = 0;
    for (uint32_t i = 0; i < plen; i++) {
      sounding += p.steps[i].vel > 0;
    }
    if (sounding > 0) {
      sparsest = std::min(sparsest, sounding);
    }
    // The weight is the share of silent steps, scaled to 0..16, so a voice
    // that never rests weighs nothing and a sparse one weighs nearly full.
    uint32_t const weight = 16 - (16 * sounding) / plen;
    if (weight == 0) {
      continue;
    }
    for (uint32_t i = 0; i < len; i++) {
      if (p.steps[i % plen].vel > 0) {
        activity[i] += weight;
      }
    }
  }

  for (uint32_t i = 0; i < target.steps.size(); i++) {
    target.steps[i].vel = 0;
  }
  if (len == 0) {
    return;
  }

  uint32_t free_steps = 0;
  for (uint32_t i = 0; i < len; i++) {
    free_steps += activity[i] == 0;
  }
  if (free_steps > 0) {
    for (uint32_t i = 0; i < len; i++) {
      if (activity[i] == 0) {
        target.steps[i].vel = DEFAULT_VELOCITY;
      }
    }
    return;
  }

  // Fallback: no free step. Place `count` notes, each at the unused step with
  // the least activity nearest to an evenly spaced ideal position, so the fill
  // lands in the quietest spots but stays spread out. sparsest is finite here
  // because at least one other voice sounds on every step.
  uint32_t const count = std::min(sparsest, len);
  for (uint32_t j = 0; j < count; j++) {
    uint32_t const ideal = (j * len) / count;
    uint32_t best = UINT32_MAX;
    uint32_t best_activity = UINT32_MAX;
    uint32_t best_distance = UINT32_MAX;
    for (uint32_t i = 0; i < len; i++) {
      if (target.steps[i].vel > 0) {
        continue;
      }
      uint32_t const forward = (i + len - ideal) % len;
      uint32_t const distance = std::min(forward, len - forward);
      if (activity[i] < best_activity ||
          (activity[i] == best_activity && distance < best_distance)) {
        best = i;
        best_activity = activity[i];
        best_distance = distance;
      }
    }
    target.steps[best].vel = DEFAULT_VELOCITY;
  }

  // A fill that has come out identical to another voice would double it
  // rather than complement it, which can only happen when every candidate
  // tied. Dropping the last note placed breaks the tie the cheapest way.
  for (uint32_t other = 0; other < VOICES; other++) {
    if (other == voice) {
      continue;
    }
    if (sounds_like(target, *voices[other].pattern(), len)) {
      for (uint32_t i = len; i > 0; i--) {
        if (target.steps[i - 1].vel > 0) {
          target.steps[i - 1].vel = 0;
          break;
        }
      }
      return;
    }
  }
}

void Sequencer::set_voice(uint32_t idx) {
  voice_idx = idx;
  voice = &voices[idx];
};
