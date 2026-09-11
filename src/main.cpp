#include "main.h"

#include <Adafruit_ADXL343.h>
#include <Adafruit_NeoTrellisM4.h>
#include <Adafruit_Sensor.h>
#include <Arduino.h>
#include <MIDIUSB.h>
#include <SPI.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <delay.h>

#include "Input.h"
#include "MenuMode.h"
#include "PatternPresets.h"
#include "Sequencer.h"
#include "SequencerMode.h"
#include "SleepMode.h"
#include "UIMode.h"
#include "colors.h"
#include "config.h"
#include "constants.h"
#include "utils.h"

ModeManager mode_manager;

Adafruit_ADXL343 accel = Adafruit_ADXL343(123, &Wire1);

// int xCC = 1; // choose a CC number to control with x axis tilting of the
// board.
//              // 1 is mod wheel, for example.

// int last_xbend = 0;
// int last_ybend = 0;

#ifdef INTERNAL_CLOCK
uint32_t clock_start_us = 0;
uint64_t internal_clock_ticks = 0;
#endif
uint32_t ppqn = 0;

Adafruit_NeoTrellisM4 trellis = Adafruit_NeoTrellisM4();

// The pixel buffer gets rewritten from scratch every UI frame, but the frame is
// almost always identical to the one before it: the grid only changes when the
// play head moves (125ms apart at 120 BPM) or a key edits the pattern, which is
// rare next to the 8ms frame timer. show() is the expensive half of the frame -
// it expands all 32 pixels into a 3KB DMA buffer and, if called again before
// the previous frame has finished, blocks on that transmit plus its 300us latch
// - so every pixel write goes through set_pixel(), which gamma corrects, then
// compares against a shadow of what the strip is already showing and only marks
// the frame dirty on a real change. show_pixels() skips clean frames entirely.
uint32_t static const NEO_PIXELS = 32;
uint32_t pixel_shadow[NEO_PIXELS];
volatile bool pixels_dirty = false;

void set_pixel(uint32_t key, uint32_t color) {
  if (key >= NEO_PIXELS) {
    return;
  }
  uint32_t const corrected = trellis.gamma32(color);
  if (pixel_shadow[key] == corrected) {
    return;
  }
  pixel_shadow[key] = corrected;
  pixels_dirty = true;
  trellis.setPixelColor(key, corrected);
}

void fill_pixels(uint32_t color) {
  for (uint32_t i = 0; i < NEO_PIXELS; i++) {
    set_pixel(i, color);
  }
}

void show_pixels() {
  if (!pixels_dirty) {
    return;
  }
  pixels_dirty = false;
  trellis.show();
}

Sequencer seq = Sequencer();

UndoBuffer undo_buffer;
UndoBuffer redo_buffer; // what undo overwrote, most recent group last

// begin_edit opens a new undo group for an edit made from the pads. Editing
// after an undo forks the history, so whatever could have been redone is gone.
void begin_edit() {
  undo_buffer.begin_group();
  redo_buffer.clear();
}

// record_before saves `before`, the state of pattern `pattern_idx` of `voice`
// prior to an edit, into the open undo group. It is skipped when the most
// recent entry already records that pattern in that state, which is what
// happens when the previous edit turned out to change nothing.
void record_before(uint32_t voice, uint32_t pattern_idx, const Pattern &before) {
  if (!undo_buffer.empty()) {
    UndoEntry const &last = undo_buffer.back();
    if (last.voice == voice && last.pattern_idx == pattern_idx && last.before == before) {
      return;
    }
  }
  undo_buffer.push(voice, pattern_idx, before);
}

// record_undo saves voice `voice`'s current pattern into the open undo group.
void record_undo(uint32_t voice) {
  Voice &v = seq.voices[voice];
  record_before(voice, v.pattern_idx, *v.pattern());
}

// create_undo_step opens a new undo group holding just the selected pattern,
// for edits that touch nothing else.
void create_undo_step() {
  begin_edit();
  record_undo(seq.voice_idx);
};

// move_group restores the most recent group in `from`, each pattern to the
// voice and slot it came from whether or not the grid is showing it, and saves
// what it overwrites into `to` as one group. Undo and redo are this same walk
// in opposite directions.
void move_group(UndoBuffer &from, UndoBuffer &to) {
  to.begin_group();
  while (!from.empty()) {
    UndoEntry const &e = from.back();
    Pattern &target = seq.voices[e.voice].patterns[e.pattern_idx];
    to.push(e.voice, e.pattern_idx, target);
    target = e.before;
    bool const group_done = e.group_start;
    from.pop();
    if (group_done) {
      return;
    }
  }
};

void undo() { move_group(undo_buffer, redo_buffer); }
void redo() { move_group(redo_buffer, undo_buffer); }

Pattern copy_buffer = Pattern(); // for copy/paste

void copy_pattern() { copy_buffer = Pattern(*seq.voice->pattern()); }

void paste_single() {
  create_undo_step();
  seq.voice->replace_pattern(copy_buffer);
}

void paste_all_slots() {
  begin_edit();
  Voice &v = *seq.voice;
  for (uint32_t slot = 0; slot < v.patterns.size(); slot++) {
    record_before(seq.voice_idx, slot, v.patterns[slot]);
    v.patterns[slot] = Pattern(copy_buffer);
  }
}

void clear_pattern() {
  create_undo_step();
  for (auto &step : seq.voice->pattern()->steps) {
    step = Step(0);
  }
}

void clear_accents() {
  create_undo_step();
  seq.voice->pattern()->clear_accents();
}

// rewind_transport resets all playheads to step 0. If the transport is
// running, clock phase (ppqn) is preserved so the sequencer stays locked to the
// external MIDI clock grid and fires step 0 on the next step boundary. When
// stopped, locate(0) arms step 0 to fire on the very first clock after Start.
void rewind_transport() {
  notes_off();
  midi_flush();
  if (clock_running) {
    global_pos = 0;
    for (auto &voice : seq.voices) {
      voice.seek(0);
    }
  } else {
    locate(0);
  }
}

void open_menu() { mode_manager.push_mode(&menu_mode); }

void load_kit_preset(uint32_t kit_index) {
  if (kit_index >= 16) {
    return;
  }
  begin_edit();
  for (uint32_t voice = 0; voice < VOICES; voice++) {
    if (seq.is_protected(voice)) {
      continue;
    }
    record_undo(voice);
    PatternPresets::apply_kit_voice(voice, kit_index, seq.voices[voice].pattern());
    if (seq.voices[voice].current_page >= seq.voices[voice].page_count()) {
      seq.voices[voice].current_page = seq.voices[voice].page_count() - 1;
    }
  }
}

uint32_t random_below(uint32_t n) { return random(n); }

// Step key indices (row major) of the keys whose ALL version is a board
// transform rather than the single-voice transform applied to each voice:
// STEP 11 is drift alone and declutter with ALL, STEP 12 is unassigned alone
// and life with ALL, STEP 13 is unassigned alone and sync lengths with ALL.
// See transform_board.
constexpr uint32_t KEY_DRIFT_INDEX = 10;
constexpr uint32_t KEY_LIFE_INDEX = 11;
constexpr uint32_t KEY_SYNC_INDEX = 12;

// apply_transform applies the FN1 + STEP action for the step key at
// `index` (0-15, row major) to voice `voice`'s current pattern and reports
// whether the key is assigned:
//
//   row 0: shift left by 1, right by 1, left by 4, right by 4
//   row 1: reshapes: reverse, invert, humanize, fill empty (which
//          is random only when no step is free)
//   row 2: shuffle, echo, drift. With ALL, declutter and life instead, see
//          transform_board.
//   row 3: unassigned (sync lengths with ALL, see transform_board)
bool apply_transform(uint32_t voice, uint32_t index) {
  Pattern *const p = seq.voices[voice].pattern();
  switch (index) {
  // row 0: shifts
  case 0:
    p->shift(-1);
    break;
  case 1:
    p->shift(1);
    break;
  case 2:
    p->shift(-4);
    break;
  case 3:
    p->shift(4);
    break;

  // row 1: deterministic reshapes
  case 4:
    p->reverse();
    break;
  case 5:
    p->invert();
    break;
  case 6:
    p->humanize(random_below);
    break;
  case 7:
    seq.fill_empty(voice, random_below);
    break;

  // row 2: algorithms / generative
  case 8:
    p->shuffle(random_below);
    break;
  case 9:
    p->echo();
    break;
  case KEY_DRIFT_INDEX:
    seq.drift(voice, random_below);
    break;

  default:
    return false;
  }
  return true;
}

// apply_accent_transform applies the FN1 + ACCENT + STEP action for
// the step key at `index` to voice `voice`'s current pattern: loads the
// corresponding pattern preset.
bool apply_accent_transform(uint32_t voice, uint32_t index) {
  if (voice >= VOICES || index >= 16) {
    return false;
  }
  PatternPresets::apply_preset(voice, seq.voices[voice].pattern(), index);
  if (seq.voices[voice].current_page >= seq.voices[voice].page_count()) {
    seq.voices[voice].current_page = seq.voices[voice].page_count() - 1;
  }
  return true;
}

// seed_random reseeds the stock generator, which is deterministic from boot,
// from the time of the key press, which is as random as the player.
void seed_random() { randomSeed(micros()); }

// transform_pattern applies `transform` for step key `index` to the selected
// pattern as one undo step. An unassigned key records nothing.
void transform_pattern(Transform transform, uint32_t index) {
  Pattern const before = *seq.voice->pattern();
  seed_random();
  if (transform(seq.voice_idx, index)) {
    begin_edit();
    record_before(seq.voice_idx, seq.voice->pattern_idx, before);
  }
}

// transform_all_patterns applies `transform` for step key `index` to every
// voice's current pattern (FN1 + ALL + STEP) as one undo group.
void transform_all_patterns(Transform transform, uint32_t index) {
  seed_random();
  begin_edit();
  for (uint32_t voice = 0; voice < VOICES; voice++) {
    if (seq.is_protected(voice)) {
      continue;
    }
    Voice &v = seq.voices[voice];
    Pattern const before = *v.pattern();
    if (transform(voice, index)) {
      record_before(voice, v.pattern_idx, before);
    }
  }
}

// transform_board applies the FN1 + ALL + STEP action for the keys
// whose all-voice version acts on the board as a whole, declutter, life and
// sync lengths, as one undo group, and reports whether `index` is one of
// them. They do not go through transform_all_patterns because they are not a
// single-voice transform repeated: life computes every row from the same
// board, declutter picks among the voices sounding on a step, and sync
// lengths copies the selected voice's length to the rest.
bool transform_board(uint32_t index) {
  if (index != KEY_DRIFT_INDEX && index != KEY_LIFE_INDEX && index != KEY_SYNC_INDEX) {
    return false;
  }
  seed_random();
  begin_edit();
  for (uint32_t voice = 0; voice < VOICES; voice++) {
    if (!seq.is_protected(voice)) {
      record_undo(voice);
    }
  }
  if (index == KEY_DRIFT_INDEX) {
    seq.declutter(random_below);
  } else if (index == KEY_LIFE_INDEX) {
    seq.life();
  } else {
    seq.sync_lengths();
  }
  return true;
}

// dropout_patterns silences half of the sounding voices' current patterns
// (FN1 + ALL + CLEAR) as one undo group.
void dropout_patterns() {
  seed_random();
  begin_edit();
  for (uint32_t voice = 0; voice < VOICES; voice++) {
    if (!seq.is_protected(voice)) {
      record_undo(voice);
    }
  }
  seq.dropout(random_below);
}

// polymeter_patterns deals a different odd length to every voice's current
// pattern (FN1 + ALL + LEN) as one undo group.
void polymeter_patterns() {
  seed_random();
  begin_edit();
  for (uint32_t voice = 0; voice < VOICES; voice++) {
    if (!seq.is_protected(voice)) {
      record_undo(voice);
    }
  }
  seq.polymeter(random_below);
}

// swap_pattern exchanges the selected voice's current pattern with voice
// `other`'s current pattern (FN1 + VOICE) as one undo group.
void swap_pattern(uint32_t other) {
  if (other >= VOICES || other == seq.voice_idx) {
    return;
  }
  begin_edit();
  record_undo(seq.voice_idx);
  record_undo(other);
  std::swap(*seq.voice->pattern(), *seq.voices[other].pattern());
}

// get_voice_midi_note returns the MIDI note number for a given voice.
uint8_t get_voice_midi_note(uint32_t voice) {
  if (voice >= VOICES) {
    return 0;
  }
  return seq.voices[voice].midi_note;
}

void setup() {
  Serial.begin(115200);
#ifdef DEBUG
  while (!Serial)
    ;
  Serial.println("DEBUG");
#endif
  trellis.autoUpdateNeoPixels(false);
  trellis.begin();
  trellis.setBrightness(200);
  fill_pixels(COLOR_OFF);

  /* if(!accel.begin()) { */
  /*   Serial.println("No accelerometer found"); */
  /*   while(1); */
  /* } */

  mode_manager.switch_mode(&sequencer_mode);

  show_pixels();
#ifdef INTERNAL_CLOCK
  clock_start_us = micros();
#endif
  init_timer();
}

uint32_t global_pos = 0;

// Step grid shades for the selected voice, see select_voice().
uint32_t seq_color_set = COLOR_VOC0_SET;
uint32_t seq_color_bg = COLOR_VOC0_UNSET;
uint32_t seq_color_accent = COLOR_VOC0_ACCENT;

// select_voice makes voice `idx` the one the grid shows and edits.
void select_voice(uint32_t idx) {
  seq.set_voice(idx);
  if (seq.voice->current_page >= seq.voice->page_count()) {
    seq.voice->current_page = seq.voice->page_count() - 1;
  }
  seq_color_set = voice_index_to_set_color(idx);
  seq_color_bg = voice_index_to_unset_color(idx);
  seq_color_accent = voice_index_to_accent_color(idx);
}

// USB-MIDI wraps every message in a 4 byte packet and lets one bulk transfer
// carry up to 16 of them. MIDIUSB's sendMIDI() makes a transfer per packet, and
// USBDevice.send() on this core blocks until the host has collected the
// previous one (giving up after 70ms when nothing is listening), so the twelve
// note offs and note ons a step can produce meant up to eleven round trips to
// the host inside the clock path. The messages of a step are collected here
// and go out as a single transfer. MidiUSB.flush() plays no part in that: on
// SAMD it is a no-op, the transfer is armed by the send itself.
uint32_t static const MIDI_OUT_CAPACITY = 16; // packets per bulk transfer
midiEventPacket_t midi_out_buf[MIDI_OUT_CAPACITY];
uint32_t midi_out_len = 0;

void midi_flush() {
  if (midi_out_len == 0) {
    return;
  }
  MidiUSB.write(reinterpret_cast<uint8_t *>(midi_out_buf),
                midi_out_len * sizeof(midiEventPacket_t));
  midi_out_len = 0;
}

void midi_queue(uint8_t cin, uint8_t status, uint8_t data1, uint8_t data2) {
  if (midi_out_len == MIDI_OUT_CAPACITY) {
    midi_flush();
  }
  midi_out_buf[midi_out_len++] = {cin, status, data1, data2};
}

void midi_note_on(uint8_t note, uint8_t velocity) {
  midi_queue(_USB_MIDI_CIN_NOTE_ON, 0x90 | MIDI_CHANNEL, std::min(note, uint8_t(0x7F)),
             std::min(velocity, uint8_t(0x7F)));
}

void midi_note_off(uint8_t note, uint8_t velocity) {
  midi_queue(_USB_MIDI_CIN_NOTE_OFF, 0x80 | MIDI_CHANNEL, std::min(note, uint8_t(0x7F)),
             std::min(velocity, uint8_t(0x7F)));
}

// notes_off queues a note off for every sounding voice. Callers flush. It is
// normally called one clock ahead of the step (see on_midi_clock) so the offs
// travel on their own and the transfer at the step carries only the note ons;
// run_step() calls it too, as a fallback for anything still sounding when a
// step arrives without that lead-in tick, such as right after Start.
void notes_off() {
  for (auto &v : seq.voices) {
    if (v.is_playing) {
      // The note that was sent, not the one the voice would send now: the
      // offset may have been toggled while it sounded.
      midi_note_off(v.playing_note, MIDI_NOTE_OFF_VELOCITY);
      v.is_playing = false;
    }
  }
}

volatile uint8_t voice_flash_mask = 0;
bool is_voice_select_hl_period = false;

bool is_voice_flashing(uint32_t voice) {
  if (voice >= VOICES) {
    return false;
  }
  return (voice_flash_mask & (1U << voice)) != 0;
}

// run_step moves every voice to its next step (or the step it was seeked to)
// and sends its note ons in one USB transfer.
void run_step() {
  notes_off();
  voice_flash_mask = 0;
  for (uint32_t voice = 0; voice < VOICES; voice++) {
    Voice &v = seq.voices[voice];
    Step const current_step = v.advance();
    if (current_step.vel > 0) {
      v.playing_note = get_voice_midi_note(voice);
      midi_note_on(v.playing_note, current_step.vel);
      v.is_playing = true;
      voice_flash_mask |= (1U << voice);
    }
  }
  is_voice_select_hl_period = (voice_flash_mask != 0);
  midi_flush();
}

static uint32_t step_flash_until_ms = 0;
static uint32_t step_flash_step = 0;
static uint32_t step_flash_color = COLOR_RED;

void trigger_step_flash(uint32_t step_idx, uint32_t color) {
  step_flash_step = step_idx;
  step_flash_color = color;
  step_flash_until_ms = millis() + 180;
}

bool is_step_flashing() { return millis() < step_flash_until_ms; }

uint32_t get_step_flash_step() { return step_flash_step; }

uint32_t get_step_flash_color() { return step_flash_color; }

// render_pixels repaints the step grid and clears an expired note highlight.
// Nothing in here talks to MIDI, so it only needs to run at the UI frame rate.
void render_pixels() {
  noInterrupts();
  for (uint32_t i = 0; i < VOICES; i++) {
    if (is_voice_flashing(i)) {
      uint32_t const flash_color = seq.voices[i].is_protected ? COLOR_RED : COLOR_PPOS;
      set_pixel(voice_index_to_key(i), flash_color);
    } else {
      set_pixel(voice_index_to_key(i), voice_index_to_color(i));
    }
  }

  uint32_t const page = seq.voice->current_page;
  uint32_t const page_start = page * STEPS_PER_PAGE;

  for (uint32_t i = 0; i < 16; i++) {
    uint32_t const step_idx = page_start + i;

    if (step_idx == seq.voice->pos) {
      set_pixel(step_key[i], COLOR_PPOS);
    } else if (seq.voice->pattern()->length <= step_idx) {
      set_pixel(step_key[i], COLOR_OFF);
    } else if (seq.voice->step(step_idx).vel >= ACCENT_VELOCITY) {
      set_pixel(step_key[i], seq_color_accent);
    } else if (seq.voice->step(step_idx).vel > 0) {
      set_pixel(step_key[i], seq_color_set);
    } else {
      set_pixel(step_key[i], seq_color_bg);
    }
  }

  if (is_step_flashing() && step_flash_step < 16) {
    set_pixel(step_key[step_flash_step], step_flash_color);
  }

  interrupts();
}

uint32_t held_keys_mask = 0;

uint32_t get_held_voice() {
  for (uint32_t v = 0; v < VOICES; v++) {
    if (held_keys_mask & (1UL << voice_index_to_key(v))) {
      return v;
    }
  }
  return VOICES;
}

// handle_keys drains the keypad event queue and dispatches via the active
// UIMode.
void handle_keys() {
  while (trellis.available()) {
    keypadEvent e = trellis.read();
    uint8_t key = e.bit.KEY;
    bool pressed = (e.bit.EVENT == KEY_JUST_PRESSED);

    debug_print("key", key);
    if (pressed) {
      debug_print("key_pressed", key);
      held_keys_mask |= (1UL << key);
    } else {
      debug_print("key_released", key);
      held_keys_mask &= ~(1UL << key);
    }

    KeyContext ctx{key, pressed, held_keys_mask & ~(1UL << key)};

    noInterrupts();
    mode_manager.handle_key(ctx);
    interrupts();
  }
}

// A master may keep sending clock while its transport is stopped (the spec
// allows it so arpeggiators and the like stay in sync), so steps only advance
// between Start/Continue and Stop. The device boots running so a master that
// sends nothing but clock still drives it.
bool clock_running = true;

// locate lands every voice on the step that `clocks` (a clock count from the
// start of the song) falls in and sets ppqn so the following ticks fire the
// right steps. The first clock after Start/Continue is clock `clocks` itself:
// when that is the first clock of a step the step is armed to fire on it, else
// the step after it fires when the count reaches the next boundary.
void locate(uint32_t clocks) {
  uint32_t const step = clocks / CLOCK_DIVISION;
  uint32_t const into_step = clocks % CLOCK_DIVISION;
  uint32_t const next_step = into_step == 0 ? step : step + 1;
  global_pos = next_step;
  for (auto &voice : seq.voices) {
    voice.seek(next_step);
  }
  ppqn = (into_step + CLOCK_DIVISION - 1) % CLOCK_DIVISION;
  voice_flash_mask = 0;
  is_voice_select_hl_period = false;
}

// The note offs go out on the clock before the step, so a step's worth of
// offs and ons is spread over two transfers a clock apart (about 20ms at 120
// BPM) instead of landing on the receiver all at once. A voice's gate is thus
// CLOCK_DIVISION - 1 clocks long.
void on_midi_clock() {
  if (!clock_running) {
    return;
  }
  ++ppqn;
  if (ppqn >= 2) {
    voice_flash_mask = 0;
    is_voice_select_hl_period = false;
  }
  if (ppqn == CLOCK_DIVISION - 1) {
    notes_off();
    midi_flush();
  } else if (ppqn >= CLOCK_DIVISION) {
    ppqn = 0;
    global_pos++;
    run_step();
  }
}

// Start plays from the beginning, and per the spec the receiver waits for the
// first clock after it before playing: that clock is the downbeat.
void on_midi_start() {
  locate(0);
  clock_running = true;
}

// Continue picks up where Stop left off (or where a Song Position Pointer put
// us), so the tick count within the step is kept.
void on_midi_continue() { clock_running = true; }

void on_midi_stop() {
  clock_running = false;
  voice_flash_mask = 0;
  is_voice_select_hl_period = false;
  notes_off();
  midi_flush();
}

void stop_playback() { on_midi_stop(); }

void start_playback() { on_midi_continue(); }

void toggle_playback() {
  if (clock_running) {
    on_midi_stop();
  } else {
    on_midi_continue();
  }
}

// Song Position Pointer arrives while stopped, ahead of a Continue, and counts
// MIDI beats (sixteenths) from the start of the song.
void on_midi_song_position(uint32_t beats) { locate(beats * MIDI_CLOCKS_PER_BEAT); }

// System Reset: stop, silence and rewind.
void on_midi_reset() {
  on_midi_stop();
  locate(0);
}

void handle_midi_in(midiEventPacket_t const &event) {
  if (mode_manager.current_mode() == &sleep_mode) {
    return;
  }
  // The high nibble of the header is the cable number; the low nibble says
  // which message type the packet carries. System real time messages come as
  // single byte packets, and only those are consulted for byte1, so a data
  // byte of some other message can never pass for a clock.
  uint8_t const cin = event.header & 0x0F;
  if (cin == _USB_MIDI_CIN_SINGLE || cin == _USB_MIDI_CIN_SINGLE_5) {
    switch (event.byte1) {
    case _MIDI_MSG_CLOCK:
      on_midi_clock();
      break;
    case _MIDI_MSG_START:
      on_midi_start();
      break;
    case _MIDI_MSG_CONT:
      on_midi_continue();
      break;
    case _MIDI_MSG_STOP:
      on_midi_stop();
      break;
    case _MIDI_MSG_RESET:
      on_midi_reset();
      break;
    }
  } else if (cin == _USB_MIDI_CIN_SYSCOM_3 && event.byte1 == _MIDI_MSG_SPP) {
    // 14 bit value, LSB first
    on_midi_song_position(event.byte2 | (uint32_t(event.byte3) << 7));
  }
}

// service_clock consumes the pending clock messages and runs the steps they
// call for. This is the timing critical path and wants to be called as often
// as possible: clock ticks are only 2.1ms apart at 120 BPM.
void service_clock() {
#ifdef INTERNAL_CLOCK
  // In internal clock mode, generate 24 PPQN clock ticks locked to BPM using
  // microsecond precision. 60,000,000 us / (BPM * 24) = 2,500,000 / BPM us per
  // tick. Calculating target timestamp directly from total elapsed ticks
  // completely eliminates cumulative drift and integer truncation errors.
  uint32_t const now_us = micros();
  uint32_t target_us =
      clock_start_us +
      static_cast<uint32_t>((static_cast<uint64_t>(internal_clock_ticks + 1) * 2500000ULL) / BPM);

  while (static_cast<int32_t>(now_us - target_us) >= 0) {
    ++internal_clock_ticks;
    on_midi_clock();
    target_us =
        clock_start_us +
        static_cast<uint32_t>((static_cast<uint64_t>(internal_clock_ticks + 1) * 2500000ULL) / BPM);
  }
#else
  // read() returns a zeroed packet once the queue is empty; 0 is not a valid
  // code index number so it cannot be mistaken for a message.
  midiEventPacket_t event = MidiUSB.read();
  while (event.header != 0) {
    handle_midi_in(event);
    event = MidiUSB.read();
  }
#endif
}

// init_timer configures SAMD51 TC3 hardware timer to service the clock engine
// at a steady 20 kHz (every 50 microseconds).
//
// By driving service_clock() from a hardware interrupt rather than polling it
// in loop(), the timing of incoming MIDI clocks and internal step advancement
// is completely decoupled from UI operations (keypad scanning settling delays,
// NeoPixel DMA buffer expansion, and latch wait states). Jitter drops from
// millisecond-scale down to sub-50 microseconds.
void init_timer() {
  // Enable APB bus clock for TC3 in MCLK
  MCLK->APBBMASK.reg |= MCLK_APBBMASK_TC3;

  // Route 120 MHz GCLK0 to TC3
  GCLK->PCHCTRL[TC3_GCLK_ID].reg = GCLK_PCHCTRL_GEN_GCLK0_Val | (1 << GCLK_PCHCTRL_CHEN_Pos);
  while (!GCLK->PCHCTRL[TC3_GCLK_ID].bit.CHEN)
    ;

  // Reset TC3
  TC3->COUNT16.CTRLA.bit.ENABLE = 0;
  while (TC3->COUNT16.SYNCBUSY.bit.ENABLE)
    ;
  TC3->COUNT16.CTRLA.bit.SWRST = 1;
  while (TC3->COUNT16.SYNCBUSY.bit.SWRST || TC3->COUNT16.CTRLA.bit.SWRST)
    ;

  // Configure TC3: 16-bit mode, prescaler DIV16 (7.5 MHz)
  TC3->COUNT16.CTRLA.reg = TC_CTRLA_MODE_COUNT16 | TC_CTRLA_PRESCALER_DIV16;
  while (TC3->COUNT16.SYNCBUSY.bit.ENABLE)
    ;

  // Match frequency mode (MFRQ): counter resets to 0 upon reaching CC0
  TC3->COUNT16.WAVE.reg = TC_WAVE_WAVEGEN_MFRQ;

  // 7,500,000 Hz * 0.000050 s = 375 counts (CC0 = 375 - 1 = 374 for 50 us / 20
  // kHz)
  TC3->COUNT16.CC[0].reg = 374;
  while (TC3->COUNT16.SYNCBUSY.bit.CC0)
    ;

  // Enable Match Channel 0 interrupt
  TC3->COUNT16.INTENSET.bit.MC0 = 1;

  // Enable TC3
  TC3->COUNT16.CTRLA.bit.ENABLE = 1;
  while (TC3->COUNT16.SYNCBUSY.bit.ENABLE)
    ;

  // NVIC priority 2: below USB interrupts (priority 0), above loop()
  NVIC_SetPriority(TC3_IRQn, 2);
  NVIC_EnableIRQ(TC3_IRQn);
}

extern "C" void TC3_Handler(void) {
  if (TC3->COUNT16.INTFLAG.bit.MC0) {
    TC3->COUNT16.INTFLAG.bit.MC0 = 1;
    service_clock();
  }
}

// The UI runs on a frame timer (~125Hz) in the background. Clock servicing is
// handled exclusively by the TC3 hardware timer interrupt at 20 kHz, so
// blocking operations here (NeoPixel show() DMA latching, keypad settling)
// cannot induce any jitter in MIDI clock reception or note dispatch.
uint32_t static const UI_FRAME_INTERVAL = 8; // ms, ~125Hz
uint32_t last_ui_frame = 0;

void loop() {
  uint32_t now = millis();
  if (now - last_ui_frame < UI_FRAME_INTERVAL) {
    return;
  }
  last_ui_frame = now;

  trellis.tick();
  handle_keys();

  mode_manager.render();
  show_pixels();
}
