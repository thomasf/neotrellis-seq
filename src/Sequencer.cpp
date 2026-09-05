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

void Pattern::echo() {
  uint32_t const len = std::min<uint32_t>(length, steps.size());
  if (len == 0) {
    return;
  }
  std::array<Step, 16> const before = steps;
  for (uint32_t i = 0; i < len; i++) {
    uint8_t const vel = before[i].vel / 2;
    Step &target = steps[(i + ECHO_STEPS) % len];
    if (vel > target.vel) {
      target.vel = vel;
    }
  }
}

void Pattern::rule30() {
  uint32_t const len = std::min<uint32_t>(length, steps.size());
  if (len == 0) {
    return;
  }
  std::array<bool, 16> before;
  for (uint32_t i = 0; i < len; i++) {
    before[i] = steps[i].vel > 0;
  }
  for (uint32_t i = 0; i < len; i++) {
    bool const left = before[(i + len - 1) % len];
    bool const self = before[i];
    bool const right = before[(i + 1) % len];
    // Rule 30 as a table over the window (left, self, right), bit 0 for
    // (off, off, off) up to bit 7 for (on, on, on): 00011110.
    uint32_t const window = (left << 2) | (self << 1) | right;
    bool const next = (30 >> window) & 1;
    if (next && !self) {
      steps[i].vel = DEFAULT_VELOCITY;
    } else if (!next) {
      steps[i].vel = 0;
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

void Sequencer::fill_empty(uint32_t voice,
                           uint32_t (*random_below)(uint32_t n)) {
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

  // Fallback: no free step. Place `count` notes, each on a step drawn at
  // random from the unused steps with the least activity. sparsest is finite
  // here because at least one other voice sounds on every step. A draw that
  // comes out identical to another voice would double it rather than
  // complement it; that is only likely when everything ties, so draw again a
  // few times and, failing that, drop a note to break the tie.
  uint32_t const count = std::min(sparsest, len);
  for (uint32_t attempt = 0; attempt < 8; attempt++) {
    for (uint32_t i = 0; i < len; i++) {
      target.steps[i].vel = 0;
    }
    for (uint32_t j = 0; j < count; j++) {
      uint32_t lowest = UINT32_MAX;
      for (uint32_t i = 0; i < len; i++) {
        if (target.steps[i].vel == 0) {
          lowest = std::min(lowest, activity[i]);
        }
      }
      uint32_t candidates = 0;
      for (uint32_t i = 0; i < len; i++) {
        candidates += target.steps[i].vel == 0 && activity[i] == lowest;
      }
      uint32_t pick = random_below(candidates);
      for (uint32_t i = 0; i < len; i++) {
        if (target.steps[i].vel == 0 && activity[i] == lowest && pick-- == 0) {
          target.steps[i].vel = DEFAULT_VELOCITY;
          break;
        }
      }
    }
    bool copy = false;
    for (uint32_t other = 0; other < VOICES && !copy; other++) {
      copy =
          other != voice && sounds_like(target, *voices[other].pattern(), len);
    }
    if (!copy) {
      return;
    }
  }
  for (uint32_t i = len; i > 0; i--) {
    if (target.steps[i - 1].vel > 0) {
      target.steps[i - 1].vel = 0;
      return;
    }
  }
}

void Sequencer::set_voice(uint32_t idx) {
  voice_idx = idx;
  voice = &voices[idx];
};

void Sequencer::rule30(uint32_t voice, uint32_t (*random_below)(uint32_t n)) {
  if (voice >= VOICES) {
    return;
  }
  Pattern &target = *voices[voice].pattern();
  uint32_t const len = std::min<uint32_t>(target.length, target.steps.size());
  uint32_t budget = 0;
  for (uint32_t i = 0; i < len; i++) {
    budget += target.steps[i].vel > 0;
  }
  target.rule30();

  // crowd[i] counts the other voices sounding on step i, read wrapped as they
  // play during the first pass of this pattern.
  std::array<uint32_t, 16> crowd{};
  for (uint32_t other = 0; other < VOICES; other++) {
    if (other == voice) {
      continue;
    }
    Pattern const &p = *voices[other].pattern();
    uint32_t const plen = std::min<uint32_t>(p.length, p.steps.size());
    if (plen == 0) {
      continue;
    }
    for (uint32_t i = 0; i < len; i++) {
      crowd[i] += p.steps[i % plen].vel > 0;
    }
  }

  // Keep `budget` of the sounding steps, the quietest crowd first. Within one
  // crowd level the steps are shuffled so a partial take is a random one.
  std::array<uint32_t, 16> order;
  uint32_t sounding = 0;
  for (uint32_t i = 0; i < len; i++) {
    if (target.steps[i].vel > 0) {
      order[sounding++] = i;
    }
  }
  for (uint32_t i = sounding; i > 1; i--) {
    std::swap(order[i - 1], order[random_below(i)]);
  }
  std::stable_sort(order.begin(), order.begin() + sounding,
                   [&](uint32_t a, uint32_t b) { return crowd[a] < crowd[b]; });
  for (uint32_t k = budget; k < sounding; k++) {
    target.steps[order[k]].vel = 0;
  }
}

void Sequencer::mutate(uint32_t voice, uint32_t (*random_below)(uint32_t n)) {
  if (voice >= VOICES) {
    return;
  }
  Pattern &target = *voices[voice].pattern();
  uint32_t const len = std::min<uint32_t>(target.length, target.steps.size());
  if (len < 2) {
    return;
  }

  // busy[i] is set when another voice that rests somewhere sounds on step i.
  std::array<bool, 16> busy{};
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
    if (sounding == plen) {
      continue;
    }
    for (uint32_t i = 0; i < len; i++) {
      busy[i] = busy[i] || p.steps[i % plen].vel > 0;
    }
  }

  // Every note can move to either neighbour, so at most 32 moves.
  struct Move {
    uint8_t from;
    uint8_t to;
  };
  std::array<Move, 32> moves;
  uint32_t count = 0;
  for (uint32_t i = 0; i < len; i++) {
    if (target.steps[i].vel == 0) {
      continue;
    }
    for (int d = -1; d <= 1; d += 2) {
      uint32_t const to = (i + len + d) % len;
      if (target.steps[to].vel == 0 && !busy[to]) {
        moves[count++] = {(uint8_t)i, (uint8_t)to};
      }
    }
  }
  if (count == 0) {
    return;
  }
  Move const m = moves[random_below(count)];
  target.steps[m.to] = target.steps[m.from];
  target.steps[m.from].vel = 0;
}

void Sequencer::declutter(uint32_t (*random_below)(uint32_t n)) {
  for (uint32_t i = 0; i < 16; i++) {
    std::array<uint32_t, VOICES> sounding;
    uint32_t count = 0;
    for (uint32_t v = 0; v < VOICES; v++) {
      Pattern const &p = *voices[v].pattern();
      if (i < p.length && p.steps[i].vel > 0) {
        sounding[count++] = v;
      }
    }
    if (count >= 2) {
      voices[sounding[random_below(count)]].pattern()->steps[i].vel = 0;
    }
  }
}

void Sequencer::polymeter(uint32_t (*random_below)(uint32_t n)) {
  std::array<uint32_t, 6> lengths = {5, 7, 9, 11, 13, 15};
  for (uint32_t i = lengths.size(); i > 1; i--) {
    std::swap(lengths[i - 1], lengths[random_below(i)]);
  }
  for (uint32_t v = 0; v < VOICES; v++) {
    voices[v].pattern()->length = lengths[v % lengths.size()];
  }
}

// Board is every voice's current pattern as it was before a Life step.
typedef std::array<Pattern, VOICES> Board;

// life_row writes the next Life generation of row `v` of `before` into
// `target`, the pattern that row belongs to.
static void life_row(Board const &before, Pattern &target, uint32_t v) {
  uint32_t const len = std::min<uint32_t>(target.length, target.steps.size());
  // alive reports whether row `row` sounds at column `col` of the row being
  // computed. Both indices wrap: the column within `len`, then within the
  // row's own length, as fill_empty reads them.
  auto const alive = [&](uint32_t row, int col) -> bool {
    Pattern const &p = before[row];
    uint32_t const plen = std::min<uint32_t>(p.length, p.steps.size());
    if (plen == 0) {
      return false;
    }
    uint32_t const c = (uint32_t)(((col % (int)len) + (int)len) % (int)len);
    return p.steps[c % plen].vel > 0;
  };

  for (uint32_t i = 0; i < len; i++) {
    uint32_t neighbours = 0;
    for (int dv = -1; dv <= 1; dv++) {
      uint32_t const row = (v + VOICES + dv) % VOICES;
      for (int di = -1; di <= 1; di++) {
        if (dv == 0 && di == 0) {
          continue;
        }
        neighbours += alive(row, (int)i + di);
      }
    }
    bool const live = before[v].steps[i].vel > 0;
    if (live && (neighbours == 2 || neighbours == 3)) {
      continue; // survives, velocity kept
    }
    target.steps[i].vel = !live && neighbours == 3 ? DEFAULT_VELOCITY : 0;
  }
}

static Board snapshot(std::array<Voice, VOICES> &voices) {
  Board board;
  for (uint32_t v = 0; v < VOICES; v++) {
    board[v] = *voices[v].pattern();
  }
  return board;
}

void Sequencer::life() {
  Board const before = snapshot(voices);
  for (uint32_t v = 0; v < VOICES; v++) {
    life_row(before, *voices[v].pattern(), v);
  }
}
