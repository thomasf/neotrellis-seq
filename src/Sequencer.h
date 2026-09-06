#ifndef _SEQUENCER_H_
#define _SEQUENCER_H_

#include "config.h"
#include <algorithm>
#include <array>
#include <cstdint>

// Step represents a single sequencer step
class Step {
public:
  uint8_t vel = 0; // midi velocity
  Step() = default;
  Step(uint32_t value) : vel(static_cast<uint8_t>(value)) {}
  Step(const Step &s) = default;
  bool operator==(const Step &s) const { return vel == s.vel; }
};

// Pattern is a sequence of steps.
class Pattern {
public:
  uint32_t length = 16;       // pattern length, up to 16 steps
  std::array<Step, 16> steps; // pattern data
  // shift moves every step n places to the right (later in time) for n > 0, or
  // left for n < 0, wrapping within the pattern length. Steps past the length
  // are left alone.
  void shift(int32_t n);
  // shuffle puts the steps within the pattern length in a random order.
  // random_below(n) must return a uniform value in [0, n).
  void shuffle(uint32_t (*random_below)(uint32_t n));
  // invert silences every sounding step within the pattern length and turns
  // every silent one into a step at the default velocity.
  void invert();
  // reverse mirrors the steps within the pattern length, so the last step
  // becomes the first.
  void reverse();
  // euclid keeps the sounding steps within the pattern length but spreads
  // them as evenly as possible across it, starting on the first step. The
  // velocities keep their order, so accents travel with the notes.
  void euclid();
  // accent_every accents (ACCENT_VELOCITY) every n-th sounding step within
  // the pattern length, counting from the first step, and drops every other
  // sounding step to DEFAULT_VELOCITY. Silent steps stay silent. n == 0
  // accents nothing.
  void accent_every(uint32_t n);
  // clear_accents drops every accented step to DEFAULT_VELOCITY.
  void clear_accents();
  // echo gives every sounding step within the pattern length a copy
  // ECHO_STEPS later, wrapping, at half its velocity. A copy never lowers a
  // step that already sounds louder, so accents and the original notes are
  // safe. Pressing again echoes the echoes, so each press adds one more
  // repeat, each half as loud, until they halve away to nothing.
  void echo();
  // snap moves every sounding step within the pattern length one step toward
  // its nearest downbeat (steps 0, 4, 8 and 12), measured round the ring of
  // the length; a step halfway between two moves to the earlier one, and a
  // step on a downbeat stays. When two notes meet, the louder one survives.
  // Repeated presses quantise a smeared pattern onto the beats.
  void snap();
  // rule30 advances the steps within the pattern length one generation of
  // the Rule 30 cellular automaton, read as a ring so the last step is the
  // left neighbour of the first. A step's next state is decided by itself and
  // its two neighbours: it sounds if its left neighbour sounds and it and its
  // right neighbour are silent, or if its left neighbour is silent and it or
  // its right neighbour sounds. A step that keeps sounding keeps its velocity,
  // a newborn one gets DEFAULT_VELOCITY. Rule 30 is chaotic and never empties
  void rule30();
  // has_sounding_notes reports whether any step within the pattern length has
  // velocity > 0.
  bool has_sounding_notes() const;
  Pattern() = default;
  bool operator==(const Pattern &p) const {
    return length == p.length && steps == p.steps;
  }
};

// pattern_has_sounding_notes reports whether any step within the pattern length
// has velocity > 0.
bool pattern_has_sounding_notes(const Pattern &p);

// Voice is a collection of patterns
class Voice {
public:
  std::array<Pattern, 16> patterns;
  bool is_playing = false;                // a note is currently being played
  uint8_t playing_note = 0;               // the note is_playing refers to
  uint8_t note_offset = 0;                // semitones added to the base note
  uint32_t pattern_idx = 0;               // current pattern index
  Pattern *pattern();                     // current pattern
  const Pattern *pattern() const;         // current pattern (const)
  void replace_pattern(const Pattern &p); // replace current pattern
  uint32_t pos = 0;                       // current position
  Step advance();                         // advance to next step (see seek)
  Step step();                            // get current step value
  Step step(uint32_t idx);                // get current step value for pos
  // seek moves the play head to step (wrapped to the pattern length) and
  // arms it: the next advance() plays that step instead of the one after it.
  // This is how Start, Song Position Pointer and the POS key land on a step
  // exactly, given that steps are played by advancing onto them.
  void seek(uint32_t step);
  Voice() = default;

private:
  bool seek_pending = false; // advance() plays pos as is, set by seek()
};

// UndoEntry records one pattern as it was before an edit, and where it lives.
struct UndoEntry {
  uint8_t voice;
  uint8_t pattern_idx;
  bool group_start; // first entry of the group this edit belongs to
  Pattern before;
};

// UndoBuffer is an edit history: a fixed size ring buffer of UndoEntry over an
// inline array, so it performs no heap allocation at all. Entries are grouped,
// and undo restores a whole group at once, so an edit that touches several
// patterns (a swap, an all-voice transform) is undone in one step. The same
// type holds the redo history, where each group is what an undo overwrote.
// The oldest entry is always a group start: when the ring is full, pushing
// drops the oldest group whole rather than leaving a headless tail that would
// otherwise be undone together with whatever came before it.
class UndoBuffer {
public:
  static constexpr uint32_t capacity = UNDO_LENGTH;
  bool empty() const; // true when there is nothing to undo
  void clear();       // drop every entry
  // begin_group makes the next push start a new group. Every push until the
  // following begin_group belongs to that group.
  void begin_group();
  // push records `before` as the state of pattern `pattern_idx` of `voice`
  // prior to an edit, in the current group.
  void push(uint32_t voice, uint32_t pattern_idx, const Pattern &before);
  const UndoEntry &back() const; // most recent entry, only valid when !empty()
  void pop();                    // remove the most recent entry
  UndoBuffer() = default;

private:
  std::array<UndoEntry, capacity> entries;
  uint32_t start = 0;        // index of the oldest entry
  uint32_t count = 0;        // number of entries currently in use
  bool group_pending = true; // the next push starts a new group
  void drop_oldest_group();
};

// VirtualBoard represents an expanded Game of Life board composed of the 6
// voices arranged in a 2x3 grid of 4x4 step pads, wrapped toroidally with a
// 1-tile halo into a 4x5 tile (16x20 cell) virtual board.
//
// Toroidal tile layout wrapping the center 2x3 core (V0..V5):
//   X5 X3 X4 X5 X3   <- top halo (row above V0..V2 is V3..V5)
//   X2 V0 V1 V2 X0   <- core row 0
//   X5 V3 V4 V5 X3   <- core row 1
//   X2 X0 X1 X2 X0   <- bottom halo (row below V3..V5 is V0..V2)
//
// In this virtual board:
// - Conway's Game of Life evolves with 2D spatial locality matching the
//   physical 4x4 button layout of the NeoTrellis M4 keypad.
// - Panning (pan_r, pan_c) allows navigating the viewport over the virtual
// board.
class VirtualBoard {
public:
  static constexpr uint32_t VOICE_ROWS = 2;
  static constexpr uint32_t VOICE_COLS = 3;
  static constexpr uint32_t GRID_SIZE = 4; // 4x4 steps per voice pattern
  static constexpr uint32_t CORE_ROWS = VOICE_ROWS * GRID_SIZE; // 8
  static constexpr uint32_t CORE_COLS = VOICE_COLS * GRID_SIZE; // 12

  // 1-tile halo around the 2x3 core: 4x5 tiles
  static constexpr uint32_t TILE_ROWS = 4;
  static constexpr uint32_t TILE_COLS = 5;
  static constexpr uint32_t VIRTUAL_ROWS = TILE_ROWS * GRID_SIZE; // 16
  static constexpr uint32_t VIRTUAL_COLS = TILE_COLS * GRID_SIZE; // 20

  // Viewport offset in steps/cells
  int32_t pan_r = 0;
  int32_t pan_c = 0;

  // The virtual board grid
  std::array<std::array<Step, VIRTUAL_COLS>, VIRTUAL_ROWS> cells{};

  VirtualBoard() = default;

  // Coordinate mapping helpers
  static constexpr uint32_t step_to_row(uint32_t step) {
    return (step % 16) / GRID_SIZE;
  }
  static constexpr uint32_t step_to_col(uint32_t step) {
    return (step % 16) % GRID_SIZE;
  }
  static constexpr uint32_t coord_to_step(uint32_t r, uint32_t c) {
    return (r % GRID_SIZE) * GRID_SIZE + (c % GRID_SIZE);
  }

  // Maps tile (tr, tc) in 0..3 x 0..4 to voice index 0..5 on the 2x3 torus
  static constexpr uint32_t tile_to_voice(int32_t tr, int32_t tc) {
    int32_t const core_tr =
        ((tr - 1) % (int32_t)VOICE_ROWS + (int32_t)VOICE_ROWS) %
        (int32_t)VOICE_ROWS;
    int32_t const core_tc =
        ((tc - 1) % (int32_t)VOICE_COLS + (int32_t)VOICE_COLS) %
        (int32_t)VOICE_COLS;
    return (uint32_t)(core_tr * VOICE_COLS + core_tc);
  }

  // Load voices into the virtual board.
  void load(const std::array<Voice, VOICES> &voices);

  // Load from an array of patterns directly
  void load(const std::array<Pattern, VOICES> &patterns);

  // Access cell with toroidal wrapping on the 16x20 virtual board
  Step at(int32_t r, int32_t c) const {
    int32_t const wr = ((r % (int32_t)VIRTUAL_ROWS) + (int32_t)VIRTUAL_ROWS) %
                       (int32_t)VIRTUAL_ROWS;
    int32_t const wc = ((c % (int32_t)VIRTUAL_COLS) + (int32_t)VIRTUAL_COLS) %
                       (int32_t)VIRTUAL_COLS;
    return cells[wr][wc];
  }

  Step &at(int32_t r, int32_t c) {
    int32_t const wr = ((r % (int32_t)VIRTUAL_ROWS) + (int32_t)VIRTUAL_ROWS) %
                       (int32_t)VIRTUAL_ROWS;
    int32_t const wc = ((c % (int32_t)VIRTUAL_COLS) + (int32_t)VIRTUAL_COLS) %
                       (int32_t)VIRTUAL_COLS;
    return cells[wr][wc];
  }

  // Pan the viewport by (dr, dc)
  void pan(int32_t dr, int32_t dc) {
    pan_r += dr;
    pan_c += dc;
  }

  void pan_tiles(int32_t dtr, int32_t dtc) {
    pan_r += dtr * (int32_t)GRID_SIZE;
    pan_c += dtc * (int32_t)GRID_SIZE;
  }

  void set_viewport(int32_t r, int32_t c) {
    pan_r = r;
    pan_c = c;
  }

  void reset_viewport() {
    pan_r = 0;
    pan_c = 0;
  }

  // Read cell relative to viewport: vr in [0..7], vc in [0..11]
  Step get_viewport(uint32_t vr, uint32_t vc) const {
    return at((int32_t)GRID_SIZE + pan_r + (int32_t)vr,
              (int32_t)GRID_SIZE + pan_c + (int32_t)vc);
  }

  // Advance the virtual board by generations of Conway's Game of Life
  void step_life(uint32_t generations = 1);

  // Extract viewport (or center core if pan == 0) back into voices or patterns
  void extract_to_voices(std::array<Voice, VOICES> &voices) const;
  void extract_to_patterns(std::array<Pattern, VOICES> &patterns) const;
};

// Sequencer is the main data type
class Sequencer {
public:
  std::array<Voice, VOICES> voices; // all voices
  Voice *voice;                     // current voice
  uint32_t voice_idx;               // current voice index
  void set_voice(uint32_t idx);     // set the currenlty active voice by index
  // fill_empty rewrites voice `voice`'s current pattern to play in the gaps the
  // other voices leave: every step within its length where no other voice
  // sounds gets a note, the rest are cleared. Busy voices count for less: a
  // voice that sounds on almost every step, a hi-hat say, is near silent for
  // this purpose, and one that sounds on every step is ignored.
  //
  // When no step is free there is no right answer, so it places as many notes
  // as the sparsest other voice has, on steps drawn at random from the least
  // crowded ones. That gives a different fill on every press and never an
  // exact copy of another voice's steps. random_below(n) must return a
  // uniform value in [0, n).
  void fill_empty(uint32_t voice, uint32_t (*random_below)(uint32_t n));
  // rule30 advances voice `voice`'s current pattern one generation of Rule 30
  // (Pattern::rule30) and then trims the result back to the number of notes
  // the pattern had, so the voice keeps its density and role. The notes it
  // keeps are those on the steps where the fewest other voices sound, read
  // wrapped as in fill_empty; ties are broken at random. random_below(n) must
  // return a uniform value in [0, n).
  void rule30(uint32_t voice, uint32_t (*random_below)(uint32_t n));
  // drift moves one note of voice `voice`'s current pattern one step left or
  // right, chosen at random among the moves that land on a step where the
  // voice is silent and no other voice sounds, read wrapped as in fill_empty.
  // A voice that sounds on every step is ignored, as there, so a running
  // hi-hat blocks nothing. The note keeps its velocity. When no move fits,
  // nothing changes. random_below(n) must return a uniform value in [0, n).
  void drift(uint32_t voice, uint32_t (*random_below)(uint32_t n));
  // declutter silences, on every step where two or more voices sound, one of
  // those voices chosen at random, so each press thins the pile-ups by one
  // voice and repeated presses end with at most one voice per step. Steps
  // are compared by index as the grid shows them, each voice within its own
  // length. random_below(n) must return a uniform value in [0, n).
  void declutter(uint32_t (*random_below)(uint32_t n));
  // dropout silences half of the voices whose current pattern sounds, rounded
  // down but at least one, chosen at random, within each pattern's length.
  // Six sounding voices go to three, then two, then one, then none: the kit
  // breaks down in stages, always passing through a single voice before it
  // falls silent. random_below(n) must return a uniform value in [0, n).
  void dropout(uint32_t (*random_below)(uint32_t n));
  // sync_lengths sets every voice's current pattern length to the selected
  // voice's. The steps are untouched. The way back from polymeter.
  void sync_lengths();
  // polymeter gives every voice's current pattern a different odd length, the
  // six values 5, 7, 9, 11, 13 and 15 dealt out in random order, so the voices
  // drift against each other and only line up again after many bars. The
  // steps themselves are untouched, so undo or LEN + STEP 16 brings a pattern
  // back whole. random_below(n) must return a uniform value in [0, n).
  void polymeter(uint32_t (*random_below)(uint32_t n));
  // life advances every voice's current pattern one generation of Conway's
  // Game of Life on an expanded 2x3 virtual board of 4x4 voice grids with
  // toroidal halo wrapping. A cell is alive when its step sounds. Each cell
  // survives with two or three live neighbours and keeps its velocity, so
  // accents travel; a dead cell with exactly three is born at DEFAULT_VELOCITY;
  // any other cell dies. Steps past a pattern's length are left alone.
  // Optional parameters allow viewport panning and multi-generation stepping.
  void life(int32_t pan_r = 0, int32_t pan_c = 0, uint32_t generations = 1);
  Sequencer();
};

#endif
