#include <unity.h>
#include <algorithm>
#include <array>
#include <cstdint>

#include "config.h"
#include "Sequencer.h"
#include "Sequencer.cpp"

void setUp(void) {
  // runs before each test
}

void tearDown(void) {
  // runs after each test
}

static uint32_t mock_random_zero(uint32_t) { return 0; }

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

  uint32_t const expected[16] = {
      0, 1, 2, 3, 7, 11, 15, 14, 13, 12, 8, 4, 5, 6, 10, 9};
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

  uint32_t const expected[16] = {
      0, 4, 8, 12, 1, 5, 9, 13, 2, 6, 10, 14, 3, 7, 11, 15};
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

  uint32_t const expected[16] = {
      0, 3, 6, 9, 12, 15, 2, 5, 8, 11, 14, 1, 4, 7, 10, 13};
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
  uint32_t const spiral_in[16] = {
      0, 1, 2, 3, 7, 11, 15, 14, 13, 12, 8, 4, 5, 6, 10, 9};
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
  RUN_TEST(test_pattern_has_sounding_notes);
  RUN_TEST(test_voice_protect_flags);
  RUN_TEST(test_voice_protect_declutter);
  RUN_TEST(test_voice_protect_dropout);
  RUN_TEST(test_voice_protect_sync_lengths);
  RUN_TEST(test_voice_protect_polymeter);
  RUN_TEST(test_voice_protect_life);
  RUN_TEST(test_path_modifier_flags);
  RUN_TEST(test_path_modifier_pingpong);
  RUN_TEST(test_path_modifier_spiral);
  RUN_TEST(test_path_modifier_vertical);
  RUN_TEST(test_path_modifier_stride);
  RUN_TEST(test_path_modifier_pingpong_spiral_combination);
  RUN_TEST(test_path_modifier_all_four_combination);
  return UNITY_END();
}
