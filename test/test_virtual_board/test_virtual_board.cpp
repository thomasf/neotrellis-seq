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

void test_step_coordinates(void) {
  for (uint32_t s = 0; s < 16; s++) {
    uint32_t r = VirtualBoard::step_to_row(s);
    uint32_t c = VirtualBoard::step_to_col(s);
    TEST_ASSERT_EQUAL_UINT32(s / 4, r);
    TEST_ASSERT_EQUAL_UINT32(s % 4, c);
    TEST_ASSERT_EQUAL_UINT32(s, VirtualBoard::coord_to_step(r, c));
  }
}

void test_tile_to_voice_mapping(void) {
  // Core (tr in 1..2, tc in 1..3) -> voices 0..5
  TEST_ASSERT_EQUAL_UINT32(0, VirtualBoard::tile_to_voice(1, 1));
  TEST_ASSERT_EQUAL_UINT32(1, VirtualBoard::tile_to_voice(1, 2));
  TEST_ASSERT_EQUAL_UINT32(2, VirtualBoard::tile_to_voice(1, 3));
  TEST_ASSERT_EQUAL_UINT32(3, VirtualBoard::tile_to_voice(2, 1));
  TEST_ASSERT_EQUAL_UINT32(4, VirtualBoard::tile_to_voice(2, 2));
  TEST_ASSERT_EQUAL_UINT32(5, VirtualBoard::tile_to_voice(2, 3));

  // Top halo (tr = 0) wraps from bottom row of voices (3, 4, 5)
  TEST_ASSERT_EQUAL_UINT32(3, VirtualBoard::tile_to_voice(0, 1)); // above V0 is V3
  TEST_ASSERT_EQUAL_UINT32(4, VirtualBoard::tile_to_voice(0, 2)); // above V1 is V4
  TEST_ASSERT_EQUAL_UINT32(5, VirtualBoard::tile_to_voice(0, 3)); // above V2 is V5
  TEST_ASSERT_EQUAL_UINT32(5, VirtualBoard::tile_to_voice(0, 0)); // top-left corner
  TEST_ASSERT_EQUAL_UINT32(3, VirtualBoard::tile_to_voice(0, 4)); // top-right corner

  // Bottom halo (tr = 3) wraps from top row of voices (0, 1, 2)
  TEST_ASSERT_EQUAL_UINT32(0, VirtualBoard::tile_to_voice(3, 1)); // below V3 is V0
  TEST_ASSERT_EQUAL_UINT32(1, VirtualBoard::tile_to_voice(3, 2)); // below V4 is V1
  TEST_ASSERT_EQUAL_UINT32(2, VirtualBoard::tile_to_voice(3, 3)); // below V5 is V2
  TEST_ASSERT_EQUAL_UINT32(2, VirtualBoard::tile_to_voice(3, 0)); // bottom-left corner
  TEST_ASSERT_EQUAL_UINT32(0, VirtualBoard::tile_to_voice(3, 4)); // bottom-right corner

  // Left halo (tc = 0) wraps from rightmost column of voices (2, 5)
  TEST_ASSERT_EQUAL_UINT32(2, VirtualBoard::tile_to_voice(1, 0)); // left of V0 is V2
  TEST_ASSERT_EQUAL_UINT32(5, VirtualBoard::tile_to_voice(2, 0)); // left of V3 is V5

  // Right halo (tc = 4) wraps from leftmost column of voices (0, 3)
  TEST_ASSERT_EQUAL_UINT32(0, VirtualBoard::tile_to_voice(1, 4)); // right of V2 is V0
  TEST_ASSERT_EQUAL_UINT32(3, VirtualBoard::tile_to_voice(2, 4)); // right of V5 is V3
}

void test_game_of_life_oscillator_and_velocities(void) {
  std::array<Pattern, VOICES> pats{};
  // Create a horizontal blinker in Voice 0 (tile 1, 1):
  // steps 4, 5, 6 (row 1 of Voice 0)
  pats[0].steps[4].vel = 100;
  pats[0].steps[5].vel = 110;
  pats[0].steps[6].vel = 120;

  VirtualBoard vb;
  vb.load(pats);

  // Generation 1: horizontal -> vertical blinker
  vb.step_life(1);

  std::array<Pattern, VOICES> evolved{};
  vb.extract_to_patterns(evolved);

  // Step 1 (top of blinker) born with DEFAULT_VELOCITY
  TEST_ASSERT_EQUAL_UINT8(DEFAULT_VELOCITY, evolved[0].steps[1].vel);
  // Step 5 (center) survived and kept original velocity 110
  TEST_ASSERT_EQUAL_UINT8(110, evolved[0].steps[5].vel);
  // Step 9 (bottom of blinker) born with DEFAULT_VELOCITY
  TEST_ASSERT_EQUAL_UINT8(DEFAULT_VELOCITY, evolved[0].steps[9].vel);
  // Steps 4 and 6 died
  TEST_ASSERT_EQUAL_UINT8(0, evolved[0].steps[4].vel);
  TEST_ASSERT_EQUAL_UINT8(0, evolved[0].steps[6].vel);

  // Generation 2: vertical -> horizontal blinker
  vb.step_life(1);
  vb.extract_to_patterns(evolved);

  TEST_ASSERT_EQUAL_UINT8(DEFAULT_VELOCITY, evolved[0].steps[4].vel);
  TEST_ASSERT_EQUAL_UINT8(110, evolved[0].steps[5].vel); // center survived again!
  TEST_ASSERT_EQUAL_UINT8(DEFAULT_VELOCITY, evolved[0].steps[6].vel);
  TEST_ASSERT_EQUAL_UINT8(0, evolved[0].steps[1].vel);
  TEST_ASSERT_EQUAL_UINT8(0, evolved[0].steps[9].vel);
}

void test_cross_voice_life_evolution(void) {
  std::array<Pattern, VOICES> pats{};
  // Create a 2x2 stable Block that straddles Voice 0 and Voice 1:
  // Voice 0 right edge: step 7 (row 1, col 3) and step 11 (row 2, col 3)
  // Voice 1 left edge: step 4 (row 1, col 0) and step 8 (row 2, col 0)
  pats[0].steps[7].vel = 100;
  pats[0].steps[11].vel = 105;
  pats[1].steps[4].vel = 110;
  pats[1].steps[8].vel = 115;

  VirtualBoard vb;
  vb.load(pats);

  // A 2x2 block is a still life: it should survive indefinitely
  vb.step_life(3);

  std::array<Pattern, VOICES> evolved{};
  vb.extract_to_patterns(evolved);

  TEST_ASSERT_EQUAL_UINT8(100, evolved[0].steps[7].vel);
  TEST_ASSERT_EQUAL_UINT8(105, evolved[0].steps[11].vel);
  TEST_ASSERT_EQUAL_UINT8(110, evolved[1].steps[4].vel);
  TEST_ASSERT_EQUAL_UINT8(115, evolved[1].steps[8].vel);
}

void test_viewport_panning(void) {
  std::array<Pattern, VOICES> pats{};
  pats[0].steps[0].vel = 88; // top-left of voice 0

  VirtualBoard vb;
  vb.load(pats);

  // Pan viewport by +1 column
  vb.pan(0, 1);
  std::array<Pattern, VOICES> extracted{};
  vb.extract_to_patterns(extracted);

  // Voice 0 step 0 now sees the next column (which is empty)
  TEST_ASSERT_EQUAL_UINT8(0, extracted[0].steps[0].vel);

  // Pan by -1 column (offset -1 from origin)
  vb.set_viewport(0, -1);
  vb.extract_to_patterns(extracted);
  // Voice 0 step 1 now lands on the note originally at col 0
  TEST_ASSERT_EQUAL_UINT8(88, extracted[0].steps[1].vel);

  // Reset viewport
  vb.reset_viewport();
  vb.extract_to_patterns(extracted);
  TEST_ASSERT_EQUAL_UINT8(88, extracted[0].steps[0].vel);
}

void test_pattern_length_preservation(void) {
  std::array<Pattern, VOICES> pats{};
  pats[0].length = 8; // pattern only 8 steps long
  // Put a note past length
  pats[0].steps[12].vel = 77;

  VirtualBoard vb;
  vb.load(pats);
  vb.step_life(1);

  std::array<Pattern, VOICES> extracted = pats;
  vb.extract_to_patterns(extracted);

  // Step 12 was past length; it must be completely preserved
  TEST_ASSERT_EQUAL_UINT8(77, extracted[0].steps[12].vel);
  TEST_ASSERT_EQUAL_UINT32(8, extracted[0].length);
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

int main(int argc, char **argv) {
  UNITY_BEGIN();
  RUN_TEST(test_step_coordinates);
  RUN_TEST(test_tile_to_voice_mapping);
  RUN_TEST(test_game_of_life_oscillator_and_velocities);
  RUN_TEST(test_cross_voice_life_evolution);
  RUN_TEST(test_viewport_panning);
  RUN_TEST(test_pattern_length_preservation);
  RUN_TEST(test_pattern_has_sounding_notes);
  return UNITY_END();
}
