#include "Sequencer.h"

void Pattern::shift(int32_t n) {
  int32_t const len = static_cast<int32_t>(std::min<uint32_t>(length, steps.size()));
  if (len < 2) {
    return;
  }
  // Normalise to a left rotation by k in [0, len), which is what std::rotate
  // does: a right shift by n is a left shift by len - n.
  int32_t k = ((-n % len) + len) % len;
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

void Pattern::humanize(uint32_t (*random_below)(uint32_t n)) {
  uint32_t const len = std::min<uint32_t>(length, steps.size());
  std::array<uint32_t, 16> sounding_indices;
  uint32_t sounding_count = 0;
  for (uint32_t i = 0; i < len; i++) {
    if (steps[i].vel > 0) {
      sounding_indices[sounding_count++] = i;
    }
  }
  if (sounding_count == 0) {
    return;
  }

  uint32_t changes = 0;
  for (uint32_t s = 0; s < sounding_count; s++) {
    uint32_t const i = sounding_indices[s];
    uint8_t const old_vel = steps[i].vel;
    uint8_t new_vel = old_vel;

    // Determine current dynamic tier:
    // 0: Ghost (<= 75), 1: Normal (76..114), 2: Accent (>= 115)
    uint32_t const tier = (old_vel >= 115) ? 2 : ((old_vel <= 75) ? 0 : 1);

    if (tier == 1) {
      // Normal hit: 25% ghost, 50% stay normal, 25% accent
      uint32_t const r = random_below(4);
      if (r == 0) {
        new_vel = GHOST_VELOCITY;
      } else if (r == 3) {
        new_vel = ACCENT_VELOCITY;
      } else {
        new_vel = DEFAULT_VELOCITY;
      }
    } else if (tier == 0) {
      // Ghost hit: 60% stay ghost, 40% rise to normal
      uint32_t const r = random_below(5);
      new_vel = (r < 2) ? DEFAULT_VELOCITY : GHOST_VELOCITY;
    } else {
      // Accent hit: 60% stay accent, 40% drop to normal
      uint32_t const r = random_below(5);
      new_vel = (r < 2) ? DEFAULT_VELOCITY : ACCENT_VELOCITY;
    }

    if (new_vel != old_vel) {
      steps[i].vel = new_vel;
      changes++;
    }
  }

  // If rolls resulted in no change, force a dynamic shift on one sounding note
  if (changes == 0) {
    uint32_t const target_idx = sounding_indices[random_below(sounding_count)];
    uint8_t const old_vel = steps[target_idx].vel;
    if (old_vel >= 115) {
      steps[target_idx].vel = DEFAULT_VELOCITY;
    } else if (old_vel <= 75) {
      steps[target_idx].vel = DEFAULT_VELOCITY;
    } else {
      steps[target_idx].vel = (random_below(2) == 0) ? ACCENT_VELOCITY : GHOST_VELOCITY;
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

Pattern *Voice::pattern() { return &patterns[pattern_idx]; }
const Pattern *Voice::pattern() const { return &patterns[pattern_idx]; }

Step Voice::step(uint32_t idx) { return pattern()->steps[idx]; }

Step Voice::step() { return pattern()->steps[pos]; }

static constexpr uint8_t SPIRAL_TABLE[16] = {0, 1, 2, 3, 7, 11, 15, 14, 13, 12, 8, 4, 5, 6, 10, 9};

static uint32_t filter_perm(const uint8_t table[16], uint32_t idx, uint32_t len) {
  if (len >= 16) {
    return table[idx % 16];
  }
  uint32_t count = 0;
  for (uint32_t i = 0; i < 16; i++) {
    if (table[i] < len) {
      if (count == idx) {
        return table[i];
      }
      count++;
    }
  }
  return idx % len;
}

PathModifier get_mutate_modifier(uint32_t bar4) {
  if (NUM_MUTATE_PATH_MODIFIERS == 0) {
    return PATH_NONE;
  }
  uint32_t const round = bar4 / NUM_MUTATE_PATH_MODIFIERS;
  uint32_t const slot = bar4 % NUM_MUTATE_PATH_MODIFIERS;

  std::array<uint8_t, NUM_MUTATE_PATH_MODIFIERS> perm{};
  for (size_t i = 0; i < NUM_MUTATE_PATH_MODIFIERS; i++) {
    perm[i] = static_cast<uint8_t>(i);
  }

  uint32_t seed = (round * 0x85ebca6bu) ^ 0x12345678u;
  for (size_t i = NUM_MUTATE_PATH_MODIFIERS - 1; i > 0; i--) {
    seed = seed * 1664525u + 1013904223u;
    size_t const j = (seed >> 16) % (i + 1);
    std::swap(perm[i], perm[j]);
  }

  return MUTATE_PATH_MODIFIERS[perm[slot]];
}

uint32_t Voice::cycle_length() const {
  uint32_t const len = pattern()->length;
  if (len <= 1) {
    return len == 0 ? 0 : 1;
  }
  uint32_t const base_cycle = ((path_modifiers & PATH_PINGPONG) && len > 1) ? (2 * len - 2) : len;
  if (path_modifiers & PATH_MUTATE) {
    uint32_t c = 4 * len * NUM_MUTATE_PATH_MODIFIERS;
    if (path_modifiers & PATH_PHASE) {
      c *= len;
    }
    if (path_modifiers & PATH_PINGPONG) {
      c *= (len - 1);
    }
    return c;
  }
  return (path_modifiers & PATH_PHASE) ? (base_cycle * len) : base_cycle;
}

uint32_t Voice::calculate_pos(uint32_t tick) const {
  uint32_t const len = pattern()->length;
  if (len <= 1) {
    return 0;
  }

  uint16_t mods = path_modifiers;
  if (mods & PATH_MUTATE) {
    uint32_t const bar4 = tick / (4 * len);
    PathModifier const mutant = get_mutate_modifier(bar4);
    mods = (mods & ~PATH_MUTATE) | mutant;
  }

  uint32_t const base_cycle = ((mods & PATH_PINGPONG) && len > 1) ? (2 * len - 2) : len;
  uint32_t const t = tick % base_cycle;
  uint32_t u = (mods & PATH_PINGPONG) ? ((t < len) ? t : (base_cycle - t)) : t;

  if (mods & PATH_PHASE) {
    uint32_t const shift = (voice_idx * 2 + (tick / base_cycle)) % len;
    u = (u + shift) % len;
  }

  if (mods & PATH_WEAVE) {
    uint32_t const cell = u / 4;
    static constexpr uint8_t CELL_OFF[4] = {0, 1, 2, 1};
    u = (cell * 2 + CELL_OFF[u % 4]) % len;
  }

  if (mods & PATH_BROKEN_THIRDS) {
    uint32_t const k = u / 2;
    u = (u % 2 == 0) ? (k % len) : ((k + 2) % len);
  }

  if (mods & PATH_BEAT_PINGPONG) {
    uint32_t const beat = (u / 4) * 4;
    static constexpr uint8_t BEAT_PP[4] = {0, 1, 1, 0};
    uint32_t const cand = beat + BEAT_PP[u % 4];
    if (cand < len) {
      u = cand;
    }
  }

  if (mods & PATH_DOWNBEAT_LOCK) {
    uint32_t const rem = u % 4;
    if (rem == 1) {
      if (u + 2 < len) {
        u += 2;
      }
    } else if (rem == 3) {
      u -= 2;
    }
  }

  if (mods & PATH_PAIR_SWAP) {
    if (u % 2 == 0) {
      if (u + 1 < len) {
        u += 1;
      }
    } else {
      u -= 1;
    }
  }

  if (mods & PATH_STUTTER) {
    if (u % 4 == 2) {
      u = u - 1;
    }
  }

  if (mods & PATH_TURNAROUND) {
    if (len >= 8 && u >= len - 4) {
      uint32_t const offset = u - (len - 4);
      u = (len - 1) - offset;
    } else if (len >= 4 && len < 8 && u >= len - 2) {
      uint32_t const offset = u - (len - 2);
      u = (len - 1) - offset;
    }
  }

  if (mods & PATH_PEDAL) {
    if (u % 2 == 0) {
      u = (u / 4) * 4;
    }
  }

  if (mods & PATH_DRUNKEN) {
    // Deterministic hash of tick for bounded random hesitation/anticipation
    // (+-1 step)
    uint32_t h = (tick ^ 0x9e3779b9u) * 0x85ebca6bu;
    h ^= h >> 13;
    uint32_t const roll = h % 100;
    if (roll >= 60 && roll < 80) {
      u = (u + len - 1) % len;
    } else if (roll >= 80 && roll < 95) {
      u = (u + 1) % len;
    }
  }

  if (mods & PATH_SPIRAL) {
    u = filter_perm(SPIRAL_TABLE, u, len);
  }

  return u;
}

Step Voice::advance() {
  uint32_t const len = pattern()->length;
  if (len == 0) {
    return Step(0);
  }
  if (seek_pending) {
    seek_pending = false;
  } else {
    play_head = (play_head + 1) % cycle_length();
    pos = calculate_pos(play_head);
  }
  return step();
}

void Voice::seek(uint32_t step) {
  uint32_t const len = pattern()->length;
  if (len == 0) {
    return;
  }
  play_head = step % cycle_length();
  pos = calculate_pos(play_head);
  seek_pending = true;
}

void Voice::replace_pattern(const Pattern &p) { patterns[pattern_idx] = p; }

bool UndoBuffer::empty() const { return count == 0; }

void UndoBuffer::clear() {
  start = 0;
  count = 0;
  group_pending = true;
}

void UndoBuffer::begin_group() { group_pending = true; }

const UndoEntry &UndoBuffer::back() const { return entries[(start + count - 1) % capacity]; }

void UndoBuffer::push(uint32_t voice, uint32_t pattern_idx, const Pattern &before) {
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
  for (uint32_t i = 0; i < VOICES; i++) {
    voices[i].voice_idx = i;
    voices[i].midi_note = ACTIVE_VOICE_MAP[i].base;
  }
}

void Sequencer::set_or_swap_note(uint32_t voice, uint8_t new_note) {
  if (voice >= VOICES) {
    return;
  }
  if (voices[voice].midi_note == new_note) {
    return;
  }
  for (uint32_t other = 0; other < VOICES; other++) {
    if (other != voice && voices[other].midi_note == new_note) {
      std::swap(voices[voice].midi_note, voices[other].midi_note);
      return;
    }
  }
  voices[voice].midi_note = new_note;
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

void Sequencer::fill_empty(uint32_t voice, uint32_t (*random_below)(uint32_t n)) {
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
      copy = other != voice && sounds_like(target, *voices[other].pattern(), len);
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

void Sequencer::drift(uint32_t voice, uint32_t (*random_below)(uint32_t n)) {
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
    uint32_t total_sounding = 0;
    for (uint32_t v = 0; v < VOICES; v++) {
      Pattern const &p = *voices[v].pattern();
      if (i < p.length && p.steps[i].vel > 0) {
        total_sounding++;
        if (!voices[v].is_protected) {
          sounding[count++] = v;
        }
      }
    }
    if (total_sounding >= 2 && count > 0) {
      voices[sounding[random_below(count)]].pattern()->steps[i].vel = 0;
    }
  }
}

void Sequencer::dropout(uint32_t (*random_below)(uint32_t n)) {
  std::array<uint32_t, VOICES> sounding;
  uint32_t count = 0;
  for (uint32_t v = 0; v < VOICES; v++) {
    if (voices[v].is_protected) {
      continue;
    }
    Pattern const &p = *voices[v].pattern();
    uint32_t const len = std::min<uint32_t>(p.length, p.steps.size());
    for (uint32_t i = 0; i < len; i++) {
      if (p.steps[i].vel > 0) {
        sounding[count++] = v;
        break;
      }
    }
  }
  if (count == 0) {
    return;
  }
  // Silence the first half, at least one, of a random order of the sounding
  // voices.
  uint32_t const drop = std::max<uint32_t>(count / 2, count > 0);
  for (uint32_t k = 0; k < drop; k++) {
    std::swap(sounding[k], sounding[k + random_below(count - k)]);
    Pattern &p = *voices[sounding[k]].pattern();
    uint32_t const len = std::min<uint32_t>(p.length, p.steps.size());
    for (uint32_t i = 0; i < len; i++) {
      p.steps[i].vel = 0;
    }
  }
}

void Sequencer::sync_lengths() {
  uint32_t const len = voice->pattern()->length;
  for (uint32_t v = 0; v < VOICES; v++) {
    if (voices[v].is_protected) {
      continue;
    }
    voices[v].pattern()->length = len;
  }
}

void Sequencer::polymeter(uint32_t (*random_below)(uint32_t n)) {
  std::array<uint32_t, 6> lengths = {5, 7, 9, 11, 13, 15};
  for (uint32_t i = lengths.size(); i > 1; i--) {
    std::swap(lengths[i - 1], lengths[random_below(i)]);
  }
  for (uint32_t v = 0; v < VOICES; v++) {
    if (voices[v].is_protected) {
      continue;
    }
    voices[v].pattern()->length = lengths[v % lengths.size()];
  }
}

// Board is every voice's current pattern as it was before a Life step.
using Board = std::array<Pattern, VOICES>;

bool pattern_has_sounding_notes(const Pattern &p) {
  uint32_t const len = std::min<uint32_t>(p.length, p.steps.size());
  for (uint32_t i = 0; i < len; i++) {
    if (p.steps[i].vel > 0) {
      return true;
    }
  }
  return false;
}

bool Pattern::has_sounding_notes() const { return pattern_has_sounding_notes(*this); }

void VirtualBoard::load(const std::array<Pattern, VOICES> &patterns) {
  for (uint32_t tr = 0; tr < TILE_ROWS; tr++) {
    for (uint32_t tc = 0; tc < TILE_COLS; tc++) {
      uint32_t const v = tile_to_voice((int32_t)tr, (int32_t)tc);
      Pattern const &p = patterns[v];
      uint32_t const plen = std::min<uint32_t>(p.length, p.steps.size());

      for (uint32_t sr = 0; sr < GRID_SIZE; sr++) {
        for (uint32_t sc = 0; sc < GRID_SIZE; sc++) {
          uint32_t const step_idx = sr * GRID_SIZE + sc;
          Step const s = (step_idx < plen) ? p.steps[step_idx] : Step(0);
          cells[tr * GRID_SIZE + sr][tc * GRID_SIZE + sc] = s;
        }
      }
    }
  }
}

void VirtualBoard::load(const std::array<Voice, VOICES> &voices) {
  std::array<Pattern, VOICES> pats;
  for (uint32_t v = 0; v < VOICES; v++) {
    pats[v] = *voices[v].pattern();
  }
  load(pats);
}

void VirtualBoard::step_life(uint32_t generations) {
  for (uint32_t g = 0; g < generations; g++) {
    std::array<std::array<Step, VIRTUAL_COLS>, VIRTUAL_ROWS> next_cells = cells;
    for (uint32_t r = 0; r < VIRTUAL_ROWS; r++) {
      for (uint32_t c = 0; c < VIRTUAL_COLS; c++) {
        uint32_t neighbours = 0;
        for (int dr = -1; dr <= 1; dr++) {
          for (int dc = -1; dc <= 1; dc++) {
            if (dr == 0 && dc == 0) {
              continue;
            }
            neighbours += (at((int32_t)r + dr, (int32_t)c + dc).vel > 0);
          }
        }
        bool const live = cells[r][c].vel > 0;
        if (live && (neighbours == 2 || neighbours == 3)) {
          continue; // survives, velocity and accent kept
        }
        next_cells[r][c].vel = (!live && neighbours == 3) ? DEFAULT_VELOCITY : 0;
      }
    }
    cells = next_cells;
  }
}

void VirtualBoard::extract_to_patterns(std::array<Pattern, VOICES> &patterns) const {
  for (uint32_t vr = 0; vr < CORE_ROWS; vr++) {
    for (uint32_t vc = 0; vc < CORE_COLS; vc++) {
      uint32_t const voice_idx = (vr / GRID_SIZE) * VOICE_COLS + (vc / GRID_SIZE);
      uint32_t const step_idx = (vr % GRID_SIZE) * GRID_SIZE + (vc % GRID_SIZE);
      Pattern &p = patterns[voice_idx];
      uint32_t const plen = std::min<uint32_t>(p.length, p.steps.size());
      if (step_idx < plen) {
        p.steps[step_idx] = get_viewport(vr, vc);
      }
    }
  }
}

void VirtualBoard::extract_to_voices(std::array<Voice, VOICES> &voices) const {
  for (uint32_t vr = 0; vr < CORE_ROWS; vr++) {
    for (uint32_t vc = 0; vc < CORE_COLS; vc++) {
      uint32_t const voice_idx = (vr / GRID_SIZE) * VOICE_COLS + (vc / GRID_SIZE);
      if (voices[voice_idx].is_protected) {
        continue;
      }
      uint32_t const step_idx = (vr % GRID_SIZE) * GRID_SIZE + (vc % GRID_SIZE);
      Pattern &p = *voices[voice_idx].pattern();
      uint32_t const plen = std::min<uint32_t>(p.length, p.steps.size());
      if (step_idx < plen) {
        p.steps[step_idx] = get_viewport(vr, vc);
      }
    }
  }
}

void Sequencer::life(int32_t pan_r, int32_t pan_c, uint32_t generations) {
  VirtualBoard vb;
  vb.load(voices);
  vb.set_viewport(pan_r, pan_c);
  vb.step_life(generations);
  vb.extract_to_voices(voices);
}
