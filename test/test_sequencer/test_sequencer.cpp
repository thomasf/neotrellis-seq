#include <algorithm>
#include <array>
#include <cstdint>
#include <unity.h>

#include "PatternPresets.h"
#include "Sequencer.cpp"
#include "Sequencer.h"
#include "config.h"

void setUp(void) {
  // runs before each test
}

void tearDown(void) {
  // runs after each test
}

static uint32_t mock_random_zero(uint32_t) { return 0; }

void test_step_basics(void) {
  Step s_default;
  TEST_ASSERT_EQUAL_UINT8(0, s_default.vel);

  Step s_val(110);
  TEST_ASSERT_EQUAL_UINT8(110, s_val.vel);

  Step s_copy(s_val);
  TEST_ASSERT_EQUAL_UINT8(110, s_copy.vel);
  TEST_ASSERT_TRUE(s_val == s_copy);

  Step s_other(90);
  TEST_ASSERT_FALSE(s_val == s_other);
}

void test_pattern_basics_and_equality(void) {
  Pattern p1;
  TEST_ASSERT_EQUAL_UINT32(16, p1.length);
  for (uint32_t i = 0; i < 16; i++) {
    TEST_ASSERT_EQUAL_UINT8(0, p1.steps[i].vel);
  }

  Pattern p2;
  TEST_ASSERT_TRUE(p1 == p2);

  p1.steps[2].vel = 80;
  TEST_ASSERT_FALSE(p1 == p2);

  p2.steps[2].vel = 80;
  TEST_ASSERT_TRUE(p1 == p2);

  p2.length = 8;
  TEST_ASSERT_FALSE(p1 == p2);
}

void test_pattern_shift(void) {
  Pattern p;
  p.length = 4;
  p.steps[0].vel = 10;
  p.steps[1].vel = 20;
  p.steps[2].vel = 30;
  p.steps[3].vel = 40;
  p.steps[4].vel = 99; // Beyond length

  // Shift right by 1
  p.shift(1);
  TEST_ASSERT_EQUAL_UINT8(40, p.steps[0].vel);
  TEST_ASSERT_EQUAL_UINT8(10, p.steps[1].vel);
  TEST_ASSERT_EQUAL_UINT8(20, p.steps[2].vel);
  TEST_ASSERT_EQUAL_UINT8(30, p.steps[3].vel);
  TEST_ASSERT_EQUAL_UINT8(99, p.steps[4].vel); // Preserved beyond length

  // Shift right by 2
  p.shift(2);
  TEST_ASSERT_EQUAL_UINT8(20, p.steps[0].vel);
  TEST_ASSERT_EQUAL_UINT8(30, p.steps[1].vel);
  TEST_ASSERT_EQUAL_UINT8(40, p.steps[2].vel);
  TEST_ASSERT_EQUAL_UINT8(10, p.steps[3].vel);

  // Shift left by 1 (equivalent to shift(-1))
  p.shift(-1);
  TEST_ASSERT_EQUAL_UINT8(30, p.steps[0].vel);
  TEST_ASSERT_EQUAL_UINT8(40, p.steps[1].vel);
  TEST_ASSERT_EQUAL_UINT8(10, p.steps[2].vel);
  TEST_ASSERT_EQUAL_UINT8(20, p.steps[3].vel);

  // Shift by length (4) -> no-op
  p.shift(4);
  TEST_ASSERT_EQUAL_UINT8(30, p.steps[0].vel);
  TEST_ASSERT_EQUAL_UINT8(40, p.steps[1].vel);
  TEST_ASSERT_EQUAL_UINT8(10, p.steps[2].vel);
  TEST_ASSERT_EQUAL_UINT8(20, p.steps[3].vel);

  // Shift with length < 2
  Pattern p_single;
  p_single.length = 1;
  p_single.steps[0].vel = 50;
  p_single.shift(1);
  TEST_ASSERT_EQUAL_UINT8(50, p_single.steps[0].vel);

  Pattern p_zero;
  p_zero.length = 0;
  p_zero.shift(2);
  TEST_ASSERT_EQUAL_UINT8(0, p_zero.steps[0].vel);
}

void test_pattern_invert(void) {
  Pattern p;
  p.length = 4;
  p.steps[0].vel = 100;
  p.steps[1].vel = 0;
  p.steps[2].vel = 50;
  p.steps[3].vel = 0;
  p.steps[4].vel = 77; // Beyond length

  p.invert();
  TEST_ASSERT_EQUAL_UINT8(0, p.steps[0].vel);
  TEST_ASSERT_EQUAL_UINT8(DEFAULT_VELOCITY, p.steps[1].vel);
  TEST_ASSERT_EQUAL_UINT8(0, p.steps[2].vel);
  TEST_ASSERT_EQUAL_UINT8(DEFAULT_VELOCITY, p.steps[3].vel);
  TEST_ASSERT_EQUAL_UINT8(77, p.steps[4].vel); // Preserved

  p.invert();
  TEST_ASSERT_EQUAL_UINT8(DEFAULT_VELOCITY, p.steps[0].vel);
  TEST_ASSERT_EQUAL_UINT8(0, p.steps[1].vel);
  TEST_ASSERT_EQUAL_UINT8(DEFAULT_VELOCITY, p.steps[2].vel);
  TEST_ASSERT_EQUAL_UINT8(0, p.steps[3].vel);
  TEST_ASSERT_EQUAL_UINT8(77, p.steps[4].vel);
}

void test_pattern_reverse(void) {
  Pattern p;
  p.length = 4;
  p.steps[0].vel = 10;
  p.steps[1].vel = 20;
  p.steps[2].vel = 30;
  p.steps[3].vel = 40;
  p.steps[4].vel = 88; // Beyond length

  p.reverse();
  TEST_ASSERT_EQUAL_UINT8(40, p.steps[0].vel);
  TEST_ASSERT_EQUAL_UINT8(30, p.steps[1].vel);
  TEST_ASSERT_EQUAL_UINT8(20, p.steps[2].vel);
  TEST_ASSERT_EQUAL_UINT8(10, p.steps[3].vel);
  TEST_ASSERT_EQUAL_UINT8(88, p.steps[4].vel); // Preserved

  p.reverse();
  TEST_ASSERT_EQUAL_UINT8(10, p.steps[0].vel);
  TEST_ASSERT_EQUAL_UINT8(20, p.steps[1].vel);
  TEST_ASSERT_EQUAL_UINT8(30, p.steps[2].vel);
  TEST_ASSERT_EQUAL_UINT8(40, p.steps[3].vel);
}

void test_pattern_euclid(void) {
  Pattern p;
  p.length = 16;
  // Put 4 notes at arbitrary steps
  p.steps[1].vel = 100;
  p.steps[3].vel = 110;
  p.steps[7].vel = 120;
  p.steps[11].vel = 130;

  p.euclid();
  // 4 notes evenly spread over 16 steps should land on 0, 4, 8, 12
  TEST_ASSERT_EQUAL_UINT8(100, p.steps[0].vel);
  TEST_ASSERT_EQUAL_UINT8(110, p.steps[4].vel);
  TEST_ASSERT_EQUAL_UINT8(120, p.steps[8].vel);
  TEST_ASSERT_EQUAL_UINT8(130, p.steps[12].vel);

  // All other steps within length 16 should be 0
  for (uint32_t i = 0; i < 16; i++) {
    if (i % 4 != 0) {
      TEST_ASSERT_EQUAL_UINT8(0, p.steps[i].vel);
    }
  }

  // Test 3 notes over 8 steps: E(3, 8) lands on 0, 3, 6
  Pattern p3;
  p3.length = 8;
  p3.steps[0].vel = 60;
  p3.steps[1].vel = 70;
  p3.steps[2].vel = 80;
  p3.steps[8].vel = 99; // Beyond length

  p3.euclid();
  TEST_ASSERT_EQUAL_UINT8(60, p3.steps[0].vel);
  TEST_ASSERT_EQUAL_UINT8(0, p3.steps[1].vel);
  TEST_ASSERT_EQUAL_UINT8(0, p3.steps[2].vel);
  TEST_ASSERT_EQUAL_UINT8(70, p3.steps[3].vel);
  TEST_ASSERT_EQUAL_UINT8(0, p3.steps[4].vel);
  TEST_ASSERT_EQUAL_UINT8(0, p3.steps[5].vel);
  TEST_ASSERT_EQUAL_UINT8(80, p3.steps[6].vel);
  TEST_ASSERT_EQUAL_UINT8(0, p3.steps[7].vel);
  TEST_ASSERT_EQUAL_UINT8(99, p3.steps[8].vel); // Untouched

  // Test empty pattern: no notes
  Pattern p_empty;
  p_empty.length = 16;
  p_empty.euclid();
  for (uint32_t i = 0; i < 16; i++) {
    TEST_ASSERT_EQUAL_UINT8(0, p_empty.steps[i].vel);
  }
}

void test_pattern_accent_every_and_clear_accents(void) {
  Pattern p;
  p.length = 6;
  p.steps[0].vel = 80;
  p.steps[1].vel = 80;
  p.steps[2].vel = 0;
  p.steps[3].vel = 80;
  p.steps[4].vel = 80;
  p.steps[5].vel = 80;
  p.steps[6].vel = 99; // Beyond length

  // Accent every 2nd step
  p.accent_every(2);
  TEST_ASSERT_EQUAL_UINT8(ACCENT_VELOCITY, p.steps[0].vel);  // 0 % 2 == 0
  TEST_ASSERT_EQUAL_UINT8(DEFAULT_VELOCITY, p.steps[1].vel); // 1 % 2 != 0
  TEST_ASSERT_EQUAL_UINT8(0, p.steps[2].vel); // silent stays silent
  TEST_ASSERT_EQUAL_UINT8(DEFAULT_VELOCITY, p.steps[3].vel); // 3 % 2 != 0
  TEST_ASSERT_EQUAL_UINT8(ACCENT_VELOCITY, p.steps[4].vel);  // 4 % 2 == 0
  TEST_ASSERT_EQUAL_UINT8(DEFAULT_VELOCITY, p.steps[5].vel); // 5 % 2 != 0
  TEST_ASSERT_EQUAL_UINT8(99, p.steps[6].vel); // beyond length untouched

  // Clear accents
  p.clear_accents();
  TEST_ASSERT_EQUAL_UINT8(DEFAULT_VELOCITY, p.steps[0].vel);
  TEST_ASSERT_EQUAL_UINT8(DEFAULT_VELOCITY, p.steps[1].vel);
  TEST_ASSERT_EQUAL_UINT8(0, p.steps[2].vel);
  TEST_ASSERT_EQUAL_UINT8(DEFAULT_VELOCITY, p.steps[4].vel);
  TEST_ASSERT_EQUAL_UINT8(99, p.steps[6].vel);

  // Accent with n = 0 (accents nothing, sets all sounding to DEFAULT_VELOCITY)
  p.steps[0].vel = 50;  // ghost
  p.steps[1].vel = 127; // accent
  p.accent_every(0);
  TEST_ASSERT_EQUAL_UINT8(DEFAULT_VELOCITY, p.steps[0].vel);
  TEST_ASSERT_EQUAL_UINT8(DEFAULT_VELOCITY, p.steps[1].vel);
}

void test_pattern_echo(void) {
  Pattern p;
  p.length = 8;
  p.steps[0].vel = 100;
  p.steps[2].vel = 70;
  p.steps[8].vel = 88; // Beyond length

  p.echo(); // ECHO_STEPS is 2
  // step 0 (100) echoes to step 2 with vel 50. But step 2 has 70 > 50, so step
  // 2 stays 70.
  TEST_ASSERT_EQUAL_UINT8(100, p.steps[0].vel);
  TEST_ASSERT_EQUAL_UINT8(70, p.steps[2].vel);
  // step 2 (70) echoes to step 4 with vel 35. Step 4 was 0, so becomes 35.
  TEST_ASSERT_EQUAL_UINT8(35, p.steps[4].vel);
  TEST_ASSERT_EQUAL_UINT8(88, p.steps[8].vel); // Untouched

  // Echo again
  p.echo();
  // step 4 (35) echoes to step 6 with vel 17.
  TEST_ASSERT_EQUAL_UINT8(17, p.steps[6].vel);

  // Echo on zero length
  Pattern p_zero;
  p_zero.length = 0;
  p_zero.echo(); // should not crash
}

void test_pattern_snap(void) {
  Pattern p;
  p.length = 16;
  // Step 0 is a downbeat, stays
  p.steps[0].vel = 100;
  // Step 1 moves to 0 (closer to 0 than 4)
  p.steps[1].vel = 80;
  // Step 3 moves to 4 (closer to 4 than 0)
  p.steps[3].vel = 90;
  // Step 7 moves to 8 (closer to 8 than 4)
  p.steps[7].vel = 70;
  // Step 10 is halfway between 8 and 12, moves to 9 (towards downbeat 8)
  p.steps[10].vel = 60;

  p.snap();
  // Step 0 collision: max(100, 80) = 100
  TEST_ASSERT_EQUAL_UINT8(100, p.steps[0].vel);
  TEST_ASSERT_EQUAL_UINT8(0, p.steps[1].vel);
  TEST_ASSERT_EQUAL_UINT8(0, p.steps[3].vel);
  TEST_ASSERT_EQUAL_UINT8(90, p.steps[4].vel);
  TEST_ASSERT_EQUAL_UINT8(0, p.steps[7].vel);
  TEST_ASSERT_EQUAL_UINT8(70, p.steps[8].vel);
  TEST_ASSERT_EQUAL_UINT8(60, p.steps[9].vel);
  TEST_ASSERT_EQUAL_UINT8(0, p.steps[10].vel);
}

void test_pattern_rule30(void) {
  Pattern p;
  p.length = 8;
  p.steps[0].vel = 100; // Single sounding step
  p.steps[8].vel = 77;  // Beyond length

  p.rule30();
  // For [1, 0, 0, 0, 0, 0, 0, 0]:
  // step 0: left=0, self=1, right=0 -> survives with 100
  // step 1: left=1, self=0, right=0 -> born with DEFAULT_VELOCITY
  // step 7: left=0, self=0, right=1 -> born with DEFAULT_VELOCITY
  // all others: 0
  TEST_ASSERT_EQUAL_UINT8(100, p.steps[0].vel);
  TEST_ASSERT_EQUAL_UINT8(DEFAULT_VELOCITY, p.steps[1].vel);
  TEST_ASSERT_EQUAL_UINT8(0, p.steps[2].vel);
  TEST_ASSERT_EQUAL_UINT8(0, p.steps[6].vel);
  TEST_ASSERT_EQUAL_UINT8(DEFAULT_VELOCITY, p.steps[7].vel);
  TEST_ASSERT_EQUAL_UINT8(77, p.steps[8].vel); // Preserved beyond length
}

void test_pattern_shuffle(void) {
  Pattern p;
  p.length = 4;
  p.steps[0].vel = 10;
  p.steps[1].vel = 20;
  p.steps[2].vel = 30;
  p.steps[3].vel = 40;
  p.steps[4].vel = 99; // Beyond length

  // Using mock_random_zero:
  // i = 4: swap(3, 0) -> [40, 20, 30, 10]
  // i = 3: swap(2, 0) -> [30, 20, 40, 10]
  // i = 2: swap(1, 0) -> [20, 30, 40, 10]
  p.shuffle(mock_random_zero);
  TEST_ASSERT_EQUAL_UINT8(20, p.steps[0].vel);
  TEST_ASSERT_EQUAL_UINT8(30, p.steps[1].vel);
  TEST_ASSERT_EQUAL_UINT8(40, p.steps[2].vel);
  TEST_ASSERT_EQUAL_UINT8(10, p.steps[3].vel);
  TEST_ASSERT_EQUAL_UINT8(99, p.steps[4].vel);
}

void test_voice_pattern_management(void) {
  Voice v;
  TEST_ASSERT_EQUAL_UINT32(0, v.pattern_idx);
  TEST_ASSERT_EQUAL_PTR(&v.patterns[0], v.pattern());

  // Edit current pattern
  v.pattern()->steps[0].vel = 100;
  TEST_ASSERT_EQUAL_UINT8(100, v.step(0).vel);

  // Switch pattern slot
  v.pattern_idx = 1;
  TEST_ASSERT_EQUAL_PTR(&v.patterns[1], v.pattern());
  TEST_ASSERT_EQUAL_UINT8(0, v.step(0).vel);

  // Replace pattern
  Pattern p;
  p.length = 8;
  p.steps[3].vel = 111;
  v.replace_pattern(p);
  TEST_ASSERT_EQUAL_UINT32(8, v.pattern()->length);
  TEST_ASSERT_EQUAL_UINT8(111, v.step(3).vel);

  // Check that slot 0 is untouched
  v.pattern_idx = 0;
  TEST_ASSERT_EQUAL_UINT8(100, v.step(0).vel);
}

void test_voice_seek_and_advance(void) {
  Voice v;
  v.pattern()->length = 4;
  v.pattern()->steps[0].vel = 10;
  v.pattern()->steps[1].vel = 20;
  v.pattern()->steps[2].vel = 30;
  v.pattern()->steps[3].vel = 40;

  // Initial step before any seek/advance
  TEST_ASSERT_EQUAL_UINT32(0, v.pos);

  // Seek to step 2
  v.seek(2);
  TEST_ASSERT_EQUAL_UINT32(2, v.pos);

  // First advance after seek consumes the seek_pending without advancing pos
  Step s = v.advance();
  TEST_ASSERT_EQUAL_UINT32(2, v.pos);
  TEST_ASSERT_EQUAL_UINT8(30, s.vel);

  // Subsequent advance advances pos to 3
  s = v.advance();
  TEST_ASSERT_EQUAL_UINT32(3, v.pos);
  TEST_ASSERT_EQUAL_UINT8(40, s.vel);

  // Wrap to 0
  s = v.advance();
  TEST_ASSERT_EQUAL_UINT32(0, v.pos);
  TEST_ASSERT_EQUAL_UINT8(10, s.vel);

  // Seek with value larger than length wraps
  v.seek(5); // 5 % 4 = 1
  TEST_ASSERT_EQUAL_UINT32(1, v.pos);

  // Zero-length pattern
  v.pattern()->length = 0;
  v.seek(0);
  s = v.advance();
  TEST_ASSERT_EQUAL_UINT8(0, s.vel);
}

void test_undo_buffer_basics(void) {
  UndoBuffer undo;
  TEST_ASSERT_TRUE(undo.empty());

  Pattern p0;
  p0.steps[0].vel = 100;
  undo.push(0, 0, p0);
  TEST_ASSERT_FALSE(undo.empty());
  TEST_ASSERT_EQUAL_UINT8(0, undo.back().voice);
  TEST_ASSERT_EQUAL_UINT8(0, undo.back().pattern_idx);
  TEST_ASSERT_TRUE(undo.back().group_start);
  TEST_ASSERT_EQUAL_UINT8(100, undo.back().before.steps[0].vel);

  // Second push without begin_group is part of same group
  Pattern p1;
  p1.steps[1].vel = 50;
  undo.push(1, 0, p1);
  TEST_ASSERT_FALSE(undo.back().group_start);
  TEST_ASSERT_EQUAL_UINT8(1, undo.back().voice);

  // Begin new group
  undo.begin_group();
  Pattern p2;
  p2.steps[2].vel = 70;
  undo.push(2, 0, p2);
  TEST_ASSERT_TRUE(undo.back().group_start);
  TEST_ASSERT_EQUAL_UINT8(2, undo.back().voice);

  // Pop entries
  undo.pop();
  TEST_ASSERT_EQUAL_UINT8(1, undo.back().voice);
  undo.pop();
  TEST_ASSERT_EQUAL_UINT8(0, undo.back().voice);
  undo.pop();
  TEST_ASSERT_TRUE(undo.empty());

  // Clear
  undo.push(0, 0, p0);
  TEST_ASSERT_FALSE(undo.empty());
  undo.clear();
  TEST_ASSERT_TRUE(undo.empty());
}

void test_undo_buffer_drop_oldest_group(void) {
  UndoBuffer undo;
  // Group 0: 2 entries (voice 4 and voice 5)
  undo.begin_group();
  Pattern p;
  undo.push(4, 0, p);
  undo.push(5, 0, p);

  // Push 254 more single-entry groups on voices 0..2 so total entries = 256
  // (capacity)
  for (uint32_t i = 1; i <= 254; i++) {
    undo.begin_group();
    undo.push(i % 3, 0, p);
  }

  // At this point buffer is exactly full with 256 entries.
  // Pushing one more entry must trigger drop_oldest_group, which should drop
  // the entire Group 0 (both entries: voice 4 and voice 5), leaving 255
  // entries.
  undo.begin_group();
  undo.push(3, 0, p);

  // Unwind all entries to verify Group 0 was completely dropped
  bool found_group_0 = false;
  uint32_t remaining_count = 0;
  while (!undo.empty()) {
    remaining_count++;
    const UndoEntry &e = undo.back();
    if (e.voice == 4 || e.voice == 5) {
      found_group_0 = true;
    }
    undo.pop();
  }
  TEST_ASSERT_FALSE(found_group_0);
  TEST_ASSERT_EQUAL_UINT32(255, remaining_count);
}

void test_sequencer_voice_selection(void) {
  Sequencer seq;
  TEST_ASSERT_EQUAL_UINT32(0, seq.voice_idx);
  TEST_ASSERT_EQUAL_PTR(&seq.voices[0], seq.voice);

  seq.set_voice(4);
  TEST_ASSERT_EQUAL_UINT32(4, seq.voice_idx);
  TEST_ASSERT_EQUAL_PTR(&seq.voices[4], seq.voice);
}

void test_sequencer_fill_empty(void) {
  Sequencer seq;
  // Voice 1 sounds on steps 0, 1, 2, 3
  for (uint32_t i = 0; i < 4; i++) {
    seq.voices[1].pattern()->steps[i].vel = 100;
  }
  // All other voices empty.
  // Fill empty for Voice 0: steps 0..3 are occupied by Voice 1, so Voice 0
  // should get DEFAULT_VELOCITY on free steps 4..15 and 0 on steps 0..3.
  seq.fill_empty(0, mock_random_zero);

  for (uint32_t i = 0; i < 4; i++) {
    TEST_ASSERT_EQUAL_UINT8(0, seq.voices[0].pattern()->steps[i].vel);
  }
  for (uint32_t i = 4; i < 16; i++) {
    TEST_ASSERT_EQUAL_UINT8(DEFAULT_VELOCITY,
                            seq.voices[0].pattern()->steps[i].vel);
  }

  // A voice that sounds on every step (e.g. continuous hats) is ignored
  for (uint32_t i = 0; i < 16; i++) {
    seq.voices[2].pattern()->steps[i].vel = 80;
  }
  // Clear Voice 1
  for (uint32_t i = 0; i < 16; i++) {
    seq.voices[1].pattern()->steps[i].vel = 0;
  }
  // Fill empty for Voice 0 again. Since Voice 2 is full, it's ignored, so all
  // steps are free!
  seq.fill_empty(0, mock_random_zero);
  for (uint32_t i = 0; i < 16; i++) {
    TEST_ASSERT_EQUAL_UINT8(DEFAULT_VELOCITY,
                            seq.voices[0].pattern()->steps[i].vel);
  }
}

void test_sequencer_drift(void) {
  Sequencer seq;
  // Voice 0 has a single note at step 4
  seq.voices[0].pattern()->steps[4].vel = 110;

  // Drift with mock_random_zero picks the first available move:
  // Steps available: step 3 (d = -1) and step 5 (d = 1).
  // Move 0 is step 3.
  seq.drift(0, mock_random_zero);

  TEST_ASSERT_EQUAL_UINT8(0, seq.voices[0].pattern()->steps[4].vel);
  TEST_ASSERT_EQUAL_UINT8(
      110, seq.voices[0].pattern()->steps[3].vel); // Kept velocity

  // Now block step 2 with Voice 1
  seq.voices[1].pattern()->steps[2].vel = 90;
  // Drift Voice 0 again. Step 3's left neighbour (step 2) is blocked by
  // Voice 1. The only available move is step 4 (d = 1).
  seq.drift(0, mock_random_zero);
  TEST_ASSERT_EQUAL_UINT8(0, seq.voices[0].pattern()->steps[3].vel);
  TEST_ASSERT_EQUAL_UINT8(110, seq.voices[0].pattern()->steps[4].vel);

  // If no moves are available, pattern is unchanged
  seq.voices[1].pattern()->steps[3].vel = 90;
  seq.voices[1].pattern()->steps[5].vel = 90;
  seq.drift(0, mock_random_zero);
  TEST_ASSERT_EQUAL_UINT8(110, seq.voices[0].pattern()->steps[4].vel);
}

void test_sequencer_rule30(void) {
  Sequencer seq;
  seq.voices[0].pattern()->length = 8;
  seq.voices[0].pattern()->steps[0].vel = 100; // Budget = 1 note

  // Rule 30 on [1, 0, 0, 0, 0, 0, 0, 0] gives notes at 0, 1, 7.
  // Let Voice 1 sound on steps 0 and 1, making step 7 the quietest.
  seq.voices[1].pattern()->length = 8;
  seq.voices[1].pattern()->steps[0].vel = 90;
  seq.voices[1].pattern()->steps[1].vel = 90;

  seq.rule30(0, mock_random_zero);

  // Budget of 1 note must be maintained, and quietest step (7) is chosen.
  TEST_ASSERT_EQUAL_UINT8(0, seq.voices[0].pattern()->steps[0].vel);
  TEST_ASSERT_EQUAL_UINT8(0, seq.voices[0].pattern()->steps[1].vel);
  TEST_ASSERT_EQUAL_UINT8(DEFAULT_VELOCITY,
                          seq.voices[0].pattern()->steps[7].vel);
}

void test_pattern_presets_voice_and_kits(void) {
  Pattern p;
  // Apply preset 0 (Four on Floor) to voice 0
  PatternPresets::apply_preset(0, &p, 0);
  TEST_ASSERT_EQUAL_UINT32(16, p.length);
  TEST_ASSERT_EQUAL_UINT8(ACCENT_VELOCITY, p.steps[0].vel);
  TEST_ASSERT_EQUAL_UINT8(0, p.steps[1].vel);
  TEST_ASSERT_EQUAL_UINT8(ACCENT_VELOCITY, p.steps[4].vel);
  TEST_ASSERT_EQUAL_UINT8(ACCENT_VELOCITY, p.steps[8].vel);
  TEST_ASSERT_EQUAL_UINT8(ACCENT_VELOCITY, p.steps[12].vel);

  // Apply preset 0 (Classic Backbeat) to voice 1
  PatternPresets::apply_preset(1, &p, 0);
  TEST_ASSERT_EQUAL_UINT8(0, p.steps[0].vel);
  TEST_ASSERT_EQUAL_UINT8(ACCENT_VELOCITY, p.steps[4].vel);
  TEST_ASSERT_EQUAL_UINT8(ACCENT_VELOCITY, p.steps[12].vel);

  // Bounds checking: invalid voice, invalid preset, nullptr
  Pattern p_copy = p;
  PatternPresets::apply_preset(6, &p, 0);
  TEST_ASSERT_TRUE(p == p_copy);

  PatternPresets::apply_preset(0, &p, 16);
  TEST_ASSERT_TRUE(p == p_copy);

  PatternPresets::apply_preset(0, nullptr, 0); // No crash

  // Apply kit 1 (French Touch Disco), voice 0 (Four on the Floor)
  PatternPresets::apply_kit_voice(0, 1, &p);
  TEST_ASSERT_EQUAL_UINT32(16, p.length);
  TEST_ASSERT_EQUAL_UINT8(ACCENT_VELOCITY, p.steps[0].vel);
  TEST_ASSERT_EQUAL_UINT8(ACCENT_VELOCITY, p.steps[4].vel);

  // Apply kit 0 (Classic Chicago House), voice 1 (Clap on 4 and 12)
  PatternPresets::apply_kit_voice(1, 0, &p);
  TEST_ASSERT_EQUAL_UINT32(16, p.length);
  TEST_ASSERT_EQUAL_UINT8(0, p.steps[0].vel);
  TEST_ASSERT_EQUAL_UINT8(ACCENT_VELOCITY, p.steps[4].vel);
  TEST_ASSERT_EQUAL_UINT8(ACCENT_VELOCITY, p.steps[12].vel);

  // Kit bounds checking
  p_copy = p;
  PatternPresets::apply_kit_voice(6, 0, &p);
  TEST_ASSERT_TRUE(p == p_copy);
  PatternPresets::apply_kit_voice(0, 16, &p);
  TEST_ASSERT_TRUE(p == p_copy);
  PatternPresets::apply_kit_voice(0, 0, nullptr); // No crash
}

void test_pattern_has_sounding_notes(void) {
  Pattern p{};
  TEST_ASSERT_FALSE(pattern_has_sounding_notes(p));
  TEST_ASSERT_FALSE(p.has_sounding_notes());

  p.steps[3].vel = DEFAULT_VELOCITY;
  TEST_ASSERT_TRUE(pattern_has_sounding_notes(p));
  TEST_ASSERT_TRUE(p.has_sounding_notes());

  // Note beyond length should not count
  Pattern p_short{};
  p_short.length = 4;
  p_short.steps[8].vel = DEFAULT_VELOCITY;
  TEST_ASSERT_FALSE(pattern_has_sounding_notes(p_short));
  TEST_ASSERT_FALSE(p_short.has_sounding_notes());
}

void test_voice_protect_flags(void) {
  Sequencer seq;
  for (uint32_t v = 0; v < VOICES; v++) {
    TEST_ASSERT_FALSE(seq.is_protected(v));
  }
  seq.set_protected(1, true);
  TEST_ASSERT_TRUE(seq.is_protected(1));
  TEST_ASSERT_TRUE(seq.voices[1].is_protected);

  seq.toggle_protected(1);
  TEST_ASSERT_FALSE(seq.is_protected(1));

  seq.toggle_protected(2);
  TEST_ASSERT_TRUE(seq.is_protected(2));
}

void test_voice_protect_declutter(void) {
  Sequencer seq;
  // Put a note at step 0 for Voice 0 and Voice 1
  seq.voices[0].pattern()->steps[0].vel = 100;
  seq.voices[1].pattern()->steps[0].vel = 100;

  // Protect Voice 0
  seq.set_protected(0, true);

  // Run declutter
  seq.declutter(mock_random_zero);

  // Voice 0 must be preserved, Voice 1 must be cleared
  TEST_ASSERT_EQUAL_UINT8(100, seq.voices[0].pattern()->steps[0].vel);
  TEST_ASSERT_EQUAL_UINT8(0, seq.voices[1].pattern()->steps[0].vel);

  // Both protected: neither cleared
  seq.voices[0].pattern()->steps[0].vel = 100;
  seq.voices[1].pattern()->steps[0].vel = 100;
  seq.set_protected(1, true);

  seq.declutter(mock_random_zero);
  TEST_ASSERT_EQUAL_UINT8(100, seq.voices[0].pattern()->steps[0].vel);
  TEST_ASSERT_EQUAL_UINT8(100, seq.voices[1].pattern()->steps[0].vel);
}

void test_voice_protect_dropout(void) {
  Sequencer seq;
  for (uint32_t v = 0; v < VOICES; v++) {
    seq.voices[v].pattern()->steps[0].vel = 100;
  }
  // Protect Voice 0 and Voice 2
  seq.set_protected(0, true);
  seq.set_protected(2, true);

  seq.dropout(mock_random_zero);

  // Protected voices must not have been dropped
  TEST_ASSERT_EQUAL_UINT8(100, seq.voices[0].pattern()->steps[0].vel);
  TEST_ASSERT_EQUAL_UINT8(100, seq.voices[2].pattern()->steps[0].vel);
}

void test_voice_protect_sync_lengths(void) {
  Sequencer seq;
  seq.voice->pattern()->length = 16;
  seq.voices[1].pattern()->length = 7;
  seq.voices[2].pattern()->length = 9;

  // Protect Voice 1
  seq.set_protected(1, true);

  seq.sync_lengths();

  // Voice 1 stays 7; Voice 2 becomes 16
  TEST_ASSERT_EQUAL_UINT32(7, seq.voices[1].pattern()->length);
  TEST_ASSERT_EQUAL_UINT32(16, seq.voices[2].pattern()->length);
}

void test_voice_protect_polymeter(void) {
  Sequencer seq;
  seq.voices[3].pattern()->length = 12;
  seq.set_protected(3, true);

  seq.polymeter(mock_random_zero);

  // Protected Voice 3 length must remain unchanged
  TEST_ASSERT_EQUAL_UINT32(12, seq.voices[3].pattern()->length);
}

void test_voice_protect_life(void) {
  Sequencer seq;
  // Blinkers in Voice 0 and Voice 1
  seq.voices[0].pattern()->steps[4].vel = 100;
  seq.voices[0].pattern()->steps[5].vel = 110;
  seq.voices[0].pattern()->steps[6].vel = 120;

  seq.voices[1].pattern()->steps[4].vel = 100;
  seq.voices[1].pattern()->steps[5].vel = 110;
  seq.voices[1].pattern()->steps[6].vel = 120;

  // Protect Voice 0
  seq.set_protected(0, true);

  seq.life();

  // Voice 0 pattern must be completely unchanged
  TEST_ASSERT_EQUAL_UINT8(100, seq.voices[0].pattern()->steps[4].vel);
  TEST_ASSERT_EQUAL_UINT8(110, seq.voices[0].pattern()->steps[5].vel);
  TEST_ASSERT_EQUAL_UINT8(120, seq.voices[0].pattern()->steps[6].vel);

  // Unprotected Voice 1 evolved
  TEST_ASSERT_EQUAL_UINT8(0, seq.voices[1].pattern()->steps[4].vel);
  TEST_ASSERT_EQUAL_UINT8(110, seq.voices[1].pattern()->steps[5].vel);
  TEST_ASSERT_EQUAL_UINT8(0, seq.voices[1].pattern()->steps[6].vel);
}

void test_path_modifier_flags(void) {
  Sequencer seq;
  TEST_ASSERT_FALSE(seq.has_path_modifier(0, PATH_PINGPONG));
  seq.toggle_path_modifier(0, PATH_PINGPONG);
  TEST_ASSERT_TRUE(seq.has_path_modifier(0, PATH_PINGPONG));
  seq.set_path_modifier(0, PATH_SPIRAL, true);
  TEST_ASSERT_TRUE(seq.has_path_modifier(0, PATH_SPIRAL));
  TEST_ASSERT_TRUE(seq.has_path_modifier(0, PATH_PINGPONG));
  seq.toggle_path_modifier(0, PATH_PINGPONG);
  TEST_ASSERT_FALSE(seq.has_path_modifier(0, PATH_PINGPONG));
  TEST_ASSERT_TRUE(seq.has_path_modifier(0, PATH_SPIRAL));
}

void test_path_modifier_pingpong(void) {
  Voice v;
  v.pattern()->length = 4;
  v.path_modifiers = PATH_PINGPONG;

  // For length 4, cycle is 2*4 - 2 = 6 steps: 0, 1, 2, 3, 2, 1
  uint32_t const expected[] = {0, 1, 2, 3, 2, 1, 0, 1, 2, 3, 2, 1};
  v.seek(0);
  for (uint32_t i = 0; i < 12; i++) {
    v.advance();
    TEST_ASSERT_EQUAL_UINT32(expected[i], v.pos);
  }
}

void test_path_modifier_spiral(void) {
  Voice v;
  v.pattern()->length = 16;
  v.path_modifiers = PATH_SPIRAL;

  uint32_t const expected[16] = {0,  1,  2, 3, 7, 11, 15, 14,
                                 13, 12, 8, 4, 5, 6,  10, 9};
  v.seek(0);
  for (uint32_t i = 0; i < 16; i++) {
    v.advance();
    TEST_ASSERT_EQUAL_UINT32(expected[i], v.pos);
  }
}

void test_path_modifier_vertical(void) {
  Voice v;
  v.pattern()->length = 16;
  v.path_modifiers = PATH_VERTICAL;

  uint32_t const expected[16] = {0, 4, 8,  12, 1, 5, 9,  13,
                                 2, 6, 10, 14, 3, 7, 11, 15};
  v.seek(0);
  for (uint32_t i = 0; i < 16; i++) {
    v.advance();
    TEST_ASSERT_EQUAL_UINT32(expected[i], v.pos);
  }
}

void test_path_modifier_stride(void) {
  Voice v;
  v.pattern()->length = 16;
  v.path_modifiers = PATH_STRIDE;

  uint32_t const expected[16] = {0, 3,  6,  9, 12, 15, 2,  5,
                                 8, 11, 14, 1, 4,  7,  10, 13};
  v.seek(0);
  for (uint32_t i = 0; i < 16; i++) {
    v.advance();
    TEST_ASSERT_EQUAL_UINT32(expected[i], v.pos);
  }
}

void test_path_modifier_pingpong_spiral_combination(void) {
  Voice v;
  v.pattern()->length = 16;
  v.path_modifiers = PATH_PINGPONG | PATH_SPIRAL;

  // First 16 steps should follow spiral inward
  uint32_t const spiral_in[16] = {0,  1,  2, 3, 7, 11, 15, 14,
                                  13, 12, 8, 4, 5, 6,  10, 9};
  v.seek(0);
  for (uint32_t i = 0; i < 16; i++) {
    v.advance();
    TEST_ASSERT_EQUAL_UINT32(spiral_in[i], v.pos);
  }
  // Then bounces back outward (14 steps: from index 14 down to 1)
  for (int i = 14; i >= 1; i--) {
    v.advance();
    TEST_ASSERT_EQUAL_UINT32(spiral_in[i], v.pos);
  }
  // Back to start (0)
  v.advance();
  TEST_ASSERT_EQUAL_UINT32(spiral_in[0], v.pos);
}

void test_path_modifier_all_four_combination(void) {
  Voice v;
  v.pattern()->length = 16;
  v.path_modifiers = PATH_PINGPONG | PATH_SPIRAL | PATH_VERTICAL | PATH_STRIDE;

  // Cycle length is 30. Ensure all indices are in [0, 16)
  v.seek(0);
  std::array<bool, 16> visited{};
  for (uint32_t i = 0; i < 30; i++) {
    v.advance();
    TEST_ASSERT_TRUE(v.pos < 16);
    visited[v.pos] = true;
  }
  // All 16 steps must be visited over the cycle
  for (uint32_t i = 0; i < 16; i++) {
    TEST_ASSERT_TRUE(visited[i]);
  }
}

int main(int argc, char **argv) {
  UNITY_BEGIN();

  // Step & Pattern Basics
  RUN_TEST(test_step_basics);
  RUN_TEST(test_pattern_basics_and_equality);
  RUN_TEST(test_pattern_has_sounding_notes);

  // Pattern Transformations
  RUN_TEST(test_pattern_shift);
  RUN_TEST(test_pattern_invert);
  RUN_TEST(test_pattern_reverse);
  RUN_TEST(test_pattern_euclid);
  RUN_TEST(test_pattern_accent_every_and_clear_accents);
  RUN_TEST(test_pattern_echo);
  RUN_TEST(test_pattern_snap);
  RUN_TEST(test_pattern_rule30);
  RUN_TEST(test_pattern_shuffle);

  // Voice Management & Playback
  RUN_TEST(test_voice_pattern_management);
  RUN_TEST(test_voice_seek_and_advance);
  RUN_TEST(test_path_modifier_flags);
  RUN_TEST(test_path_modifier_pingpong);
  RUN_TEST(test_path_modifier_spiral);
  RUN_TEST(test_path_modifier_vertical);
  RUN_TEST(test_path_modifier_stride);
  RUN_TEST(test_path_modifier_pingpong_spiral_combination);
  RUN_TEST(test_path_modifier_all_four_combination);

  // Undo Buffer
  RUN_TEST(test_undo_buffer_basics);
  RUN_TEST(test_undo_buffer_drop_oldest_group);

  // Sequencer Core Operations & Protection
  RUN_TEST(test_sequencer_voice_selection);
  RUN_TEST(test_sequencer_fill_empty);
  RUN_TEST(test_sequencer_drift);
  RUN_TEST(test_sequencer_rule30);
  RUN_TEST(test_voice_protect_flags);
  RUN_TEST(test_voice_protect_declutter);
  RUN_TEST(test_voice_protect_dropout);
  RUN_TEST(test_voice_protect_sync_lengths);
  RUN_TEST(test_voice_protect_polymeter);
  RUN_TEST(test_voice_protect_life);

  // Pattern Presets & Kits
  RUN_TEST(test_pattern_presets_voice_and_kits);

  return UNITY_END();
}
