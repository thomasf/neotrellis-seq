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

#include "Sequencer.h"
#include "colors.h"
#include "config.h"
#include "constants.h"
#include "utils.h"

Adafruit_ADXL343 accel = Adafruit_ADXL343(123, &Wire1);

// int xCC = 1; // choose a CC number to control with x axis tilting of the
// board.
//              // 1 is mod wheel, for example.

// int last_xbend = 0;
// int last_ybend = 0;

unsigned long start_time;
unsigned long last_step_time;

uint32_t beat_interval = 60000L / BPM;
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
bool pixels_dirty = false;

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
uint32_t current_voice = 0;

void setup_default_patterns() {

  auto static const N = Step(100);
  auto static const n = Step(60);
  auto static const _ = Step(0);

  auto const p1 = 15;

  seq.voices[0].patterns[p1].steps = std::array<Step, 16>{
      N, _, _, _, N, _, _, _, N, _, _, _, N, _, _, _,
  };
  seq.voices[1].patterns[p1].steps = std::array<Step, 16>{
      _, _, _, _, N, _, _, _, _, _, _, _, N, _, _, _,
  };

  seq.voices[2].patterns[p1].steps = std::array<Step, 16>{
      n, N, _, _, n, N, _, _, n, N, _, _, n, N, _, n,
  };
  seq.voices[3].patterns[p1].steps = std::array<Step, 16>{
      _, _, n, _, _, _, N, _, _, _, n, _, _, _, N, n,
  };

  seq.voices[4].patterns[p1].steps = std::array<Step, 16>{
      _, _, _, _, _, _, _, _, _, N, _, _, _, n, n, _,
  };
  seq.voices[5].patterns[p1].steps = std::array<Step, 16>{
      _, _, n, _, _, N, _, _, n, _, n, n, _, _, _, _,
  };
};

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
void record_before(uint32_t voice, uint32_t pattern_idx,
                   const Pattern &before) {
  if (!undo_buffer.empty()) {
    UndoEntry const &last = undo_buffer.back();
    if (last.voice == voice && last.pattern_idx == pattern_idx &&
        last.before == before) {
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

uint32_t random_below(uint32_t n) { return random(n); }

// Step key indices (row major) of the two keys whose ALL version is a board
// transform rather than the single-voice transform applied to each voice:
// STEP 11 is mutate alone and declutter with ALL, STEP 12 rule 30 alone and
// life with ALL. See transform_board.
uint32_t static const KEY_MUTATE_INDEX = 10;
uint32_t static const KEY_LIFE_INDEX = 11;

// apply_transform applies the TRANSFORM + STEP action for the step key at
// `index` (0-15, row major) to voice `voice`'s current pattern and reports
// whether the key is assigned:
//
//   row 0: shift left by 1, right by 1, left by 4, right by 4
//   row 1: deterministic reshapes: reverse, invert, euclid, fill empty (which
//          is random only when no step is free)
//   row 2: shuffle, echo, mutate, rule 30 with the note count locked. With
//          ALL the last two are declutter and life instead, see
//          transform_board.
//   row 3: unassigned
bool apply_transform(uint32_t voice, uint32_t index) {
  Pattern *const p = seq.voices[voice].pattern();
  if (index == 0) {
    p->shift(-1);
  } else if (index == 1) {
    p->shift(1);
  } else if (index == 2) {
    p->shift(-4);
  } else if (index == 3) {
    p->shift(4);
  } else if (index == 4) {
    p->reverse();
  } else if (index == 5) {
    p->invert();
  } else if (index == 6) {
    p->euclid();
  } else if (index == 7) {
    seq.fill_empty(voice, random_below);
  } else if (index == 8) {
    p->shuffle(random_below);
  } else if (index == 9) {
    p->echo();
  } else if (index == KEY_MUTATE_INDEX) {
    seq.mutate(voice, random_below);
  } else if (index == KEY_LIFE_INDEX) {
    seq.rule30(voice, random_below);
  } else {
    return false;
  }
  return true;
}

// apply_accent_transform applies the TRANSFORM + ACCENT + STEP action for
// the step key at `index` to voice `voice`'s current pattern: accent every
// (index + 1)-th step.
bool apply_accent_transform(uint32_t voice, uint32_t index) {
  seq.voices[voice].pattern()->accent_every(index + 1);
  return true;
}

// Transform is one of the apply_* functions above: it applies the action for
// step key `index` to a voice's current pattern and reports whether that key
// is assigned.
typedef bool (*Transform)(uint32_t voice, uint32_t index);

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
// voice's current pattern (TRANSFORM + ALL + STEP) as one undo group.
void transform_all_patterns(Transform transform, uint32_t index) {
  seed_random();
  begin_edit();
  for (uint32_t voice = 0; voice < VOICES; voice++) {
    Voice &v = seq.voices[voice];
    Pattern const before = *v.pattern();
    if (transform(voice, index)) {
      record_before(voice, v.pattern_idx, before);
    }
  }
}

// transform_board applies the TRANSFORM + ALL + STEP action for the keys
// whose all-voice version acts on the board as a whole, declutter and life,
// as one undo group, and reports whether `index` is one of them. They do not
// go through transform_all_patterns because they must see every voice's
// pattern as it was before any of them changed: life computes every row from
// the same board, and declutter picks among the voices sounding on a step.
bool transform_board(uint32_t index) {
  if (index != KEY_MUTATE_INDEX && index != KEY_LIFE_INDEX) {
    return false;
  }
  seed_random();
  begin_edit();
  for (uint32_t voice = 0; voice < VOICES; voice++) {
    record_undo(voice);
  }
  if (index == KEY_MUTATE_INDEX) {
    seq.declutter(random_below);
  } else {
    seq.life();
  }
  return true;
}

// dropout_patterns silences half of the sounding voices' current patterns
// (TRANSFORM + ALL + CLEAR) as one undo group.
void dropout_patterns() {
  seed_random();
  begin_edit();
  for (uint32_t voice = 0; voice < VOICES; voice++) {
    record_undo(voice);
  }
  seq.dropout(random_below);
}

// polymeter_patterns deals a different odd length to every voice's current
// pattern (TRANSFORM + ALL + LEN) as one undo group.
void polymeter_patterns() {
  seed_random();
  begin_edit();
  for (uint32_t voice = 0; voice < VOICES; voice++) {
    record_undo(voice);
  }
  seq.polymeter(random_below);
}

// swap_pattern exchanges the selected voice's current pattern with voice
// `other`'s current pattern (TRANSFORM + VOICE) as one undo group.
void swap_pattern(uint32_t other) {
  if (other >= VOICES || other == seq.voice_idx) {
    return;
  }
  begin_edit();
  record_undo(seq.voice_idx);
  record_undo(other);
  std::swap(*seq.voice->pattern(), *seq.voices[other].pattern());
}

// toggle_note_offset moves voice `voice` between its base note and the base
// note plus ALT_NOTE_OFFSET (TRANSFORM + the selected voice's pad). It is not
// a pattern edit, so undo does not see it; the same chord moves back.
void toggle_note_offset(uint32_t voice) {
  Voice &v = seq.voices[voice];
  v.note_offset = v.note_offset == 0 ? ALT_NOTE_OFFSET : 0;
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

  for (int i = 0; i < VOICES; i++) {
    set_pixel(voice_index_to_key(i), voice_index_to_color(i));
  }

  set_pixel(KEY_PATTERN_LEN, COLOR_PMOD);
  set_pixel(KEY_PATTERN_POS, COLOR_PMOD);
  set_pixel(KEY_TRANSFORM, COLOR_PMOD);
  set_pixel(KEY_ACCENT, COLOR_PMOD);

  set_pixel(KEY_COPY, COLOR_PACT);
  set_pixel(KEY_PASTE, COLOR_PACT);
  set_pixel(KEY_CLEAR, COLOR_PACT);
  set_pixel(KEY_UNDO, COLOR_PACT);

  set_pixel(KEY_VOICE_SELECT_ALL, COLOR_PPOS);

  setup_default_patterns();

  show_pixels();
  start_time = millis();
  last_step_time = start_time;
}

uint32_t global_pos = 0;

// Step grid shades for the selected voice, see select_voice().
uint32_t seq_color_set = COLOR_VOC0_SET;
uint32_t seq_color_bg = COLOR_VOC0_UNSET;
uint32_t seq_color_accent = COLOR_VOC0_ACCENT;

// select_voice makes voice `idx` the one the grid shows and edits.
void select_voice(uint32_t idx) {
  seq.set_voice(idx);
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
  midi_queue(_USB_MIDI_CIN_NOTE_ON, 0x90 | MIDI_CHANNEL,
             std::min(note, uint8_t(0x7F)), std::min(velocity, uint8_t(0x7F)));
}

void midi_note_off(uint8_t note, uint8_t velocity) {
  midi_queue(_USB_MIDI_CIN_NOTE_OFF, 0x80 | MIDI_CHANNEL,
             std::min(note, uint8_t(0x7F)), std::min(velocity, uint8_t(0x7F)));
}

// notes_off queues a note off for every sounding voice. Callers flush. It is
// normally called one clock ahead of the step (see on_midi_clock) so the offs
// travel on their own and the transfer at the step carries only the note ons;
// run_step() calls it too, as a fallback for anything still sounding when a
// step arrives without that lead-in tick, such as right after Start.
void notes_off() {
  for (int voice = 0; voice < VOICES; voice++) {
    Voice &v = seq.voices[voice];
    if (v.is_playing) {
      // The note that was sent, not the one the voice would send now: the
      // offset may have been toggled while it sounded.
      midi_note_off(v.playing_note, MIDI_NOTE_OFF_VELOCITY);
      v.is_playing = false;
    }
  }
}

bool is_voice_select_hl_period = false;
// run_step moves every voice to its next step (or the step it was seeked to)
// and sends its note ons in one USB transfer.
void run_step() {
  notes_off();
  for (int voice = 0; voice < VOICES; voice++) {
    Voice &v = seq.voices[voice];
    Step const current_step = v.advance();
    if (current_step.vel > 0) {
      v.playing_note = FIRST_MIDI_NOTE + voice + v.note_offset;
      midi_note_on(v.playing_note, current_step.vel);
      set_pixel(voice_index_to_key(voice), COLOR_PPOS);
      v.is_playing = true;
      is_voice_select_hl_period = true;
    }
  }
  midi_flush();
}

// render_pixels repaints the step grid and clears an expired note highlight.
// Nothing in here talks to MIDI, so it only needs to run at the UI frame rate.
void render_pixels() {
  if (is_voice_select_hl_period && ppqn >= 2) {
    is_voice_select_hl_period = false;
    for (int i = 0; i < VOICES; i++) {
      set_pixel(voice_index_to_key(i), voice_index_to_color(i));
    }
  }

  for (uint32_t i = 0; i < 16; i++) {

    if (i == seq.voice->pos) {
      set_pixel(step_key[i], COLOR_PPOS);
    } else if (seq.voice->pattern()->length <= i) {
      set_pixel(step_key[i], COLOR_OFF);
    } else if (seq.voice->step(i).vel >= ACCENT_VELOCITY) {
      set_pixel(step_key[i], seq_color_accent);
    } else if (seq.voice->step(i).vel > 0) {
      set_pixel(step_key[i], seq_color_set);
    } else {
      set_pixel(step_key[i], seq_color_bg);
    }
  }
}

// handle_keys drains the keypad event queue and applies the edits.
void handle_keys() {
  while (trellis.available()) {
    keypadEvent e = trellis.read();
    int key = e.bit.KEY;
    debug_print("key", key);

    if (e.bit.EVENT == KEY_JUST_PRESSED) {
      debug_print("key_pressed", key);

      if (key == KEY_PATTERN_LEN && trellis.isPressed(KEY_TRANSFORM) &&
          trellis.isPressed(KEY_VOICE_SELECT_ALL)) {
        polymeter_patterns();
      } else if (trellis.isPressed(KEY_PATTERN_LEN)) {
        if (is_numpad_key(key)) {
          uint32_t index = index_of(step_key, 16, key);

          create_undo_step();
          seq.voice->pattern()->length = index + 1;
        };
      } else if (trellis.isPressed(KEY_PATTERN_POS) && is_numpad_key(key)) {
        uint32_t index = index_of(step_key, 16, key);
        if (trellis.isPressed(KEY_VOICE_SELECT_ALL)) {
          for (int voice = 0; voice < VOICES; voice++) {
            seq.voices[voice].seek(index);
          }
        } else {
          seq.voice->seek(index);
        };

      } else if (trellis.isPressed(KEY_TRANSFORM) &&
                 voice_key_to_index(key) < VOICES) {
        uint32_t const voice = voice_key_to_index(key);
        if (voice == seq.voice_idx) {
          toggle_note_offset(voice);
        } else {
          swap_pattern(voice);
        }

      } else {

        if (voice_key_to_index(key) < VOICES) {
          select_voice(voice_key_to_index(key));

        } else if (key == KEY_UNDO) {
          if (trellis.isPressed(KEY_TRANSFORM)) {
            redo();
          } else {
            undo();
          }

        } else if (key == KEY_COPY) {
          copy_buffer = Pattern(*seq.voice->pattern());

        } else if (key == KEY_PASTE) {
          if (trellis.isPressed(KEY_TRANSFORM)) {
            // Paste into all 16 pattern slots of the selected voice as one
            // undo group.
            begin_edit();
            Voice &v = *seq.voice;
            for (uint32_t slot = 0; slot < v.patterns.size(); slot++) {
              record_before(seq.voice_idx, slot, v.patterns[slot]);
              v.patterns[slot] = Pattern(copy_buffer);
            }
          } else {
            create_undo_step();
            seq.voice->replace_pattern(Pattern(copy_buffer));
          }

        } else if (key == KEY_CLEAR && trellis.isPressed(KEY_TRANSFORM) &&
                   trellis.isPressed(KEY_VOICE_SELECT_ALL)) {
          dropout_patterns();

        } else if (key == KEY_CLEAR) {
          create_undo_step();
          if (trellis.isPressed(KEY_ACCENT)) {
            seq.voice->pattern()->clear_accents();
          } else {
            for (int i = 0; i < 16; i++) {
              seq.voice->pattern()->steps[i] = Step(0);
            }
          }
        } else if (is_numpad_key(key)) {

          uint32_t index = index_of(step_key, 16, key);
          debug_print("index", index);

          if (trellis.isPressed(KEY_TRANSFORM)) {
            Transform const transform = trellis.isPressed(KEY_ACCENT)
                                            ? apply_accent_transform
                                            : apply_transform;
            if (trellis.isPressed(KEY_VOICE_SELECT_ALL)) {
              if (trellis.isPressed(KEY_ACCENT) || !transform_board(index)) {
                transform_all_patterns(transform, index);
              }
            } else {
              transform_pattern(transform, index);
            }

          } else if (trellis.isPressed(KEY_ACCENT)) {
            // Accent moves a step between loud and normal and switches a
            // silent one on loud; it never switches a step off.
            create_undo_step();
            Step &step = seq.voice->pattern()->steps[index];
            step.vel = step.vel >= ACCENT_VELOCITY ? DEFAULT_VELOCITY
                                                   : ACCENT_VELOCITY;

          } else if (trellis.isPressed(KEY_VOICE_SELECT_ALL)) {
            for (int voice = 0; voice < VOICES; voice++) {
              seq.voices[voice].pattern_idx = index;
            }

          } else {
            bool voice_select_modifier_held = false;
            for (uint32_t voice = 0; voice < VOICES; voice++) {
              if (!trellis.isPressed(voice_index_to_key(voice))) {
                continue;
              }
              voice_select_modifier_held = true;
              seq.voices[voice].pattern_idx = index;
            }

            if (!voice_select_modifier_held) {
              create_undo_step();
              if (seq.voice->pattern()->steps[index].vel == 0) {
                seq.voice->pattern()->steps[index].vel = DEFAULT_VELOCITY;
              } else {
                seq.voice->pattern()->steps[index].vel = 0;
              }
            }
          }

        } else {
          // set_pixel(key, COLOR_PPOS);
        }
      }
    } else if (e.bit.EVENT == KEY_JUST_RELEASED) {
      debug_print("key_released", key);

      if (key == KEY_VOICE_SELECT_0) {
        set_pixel(key, COLOR_VOC0);

      } else if (key == KEY_VOICE_SELECT_1) {
        set_pixel(key, COLOR_VOC1);

      } else if (key == KEY_VOICE_SELECT_2) {
        set_pixel(key, COLOR_VOC2);

      } else if (key == KEY_VOICE_SELECT_3) {
        set_pixel(key, COLOR_VOC3);

      } else if (key == KEY_VOICE_SELECT_4) {
        set_pixel(key, COLOR_VOC4);

      } else if (key == KEY_VOICE_SELECT_5) {
        set_pixel(key, COLOR_VOC5);

      } else {
        // set_pixel(key, COLOR_OFF);
      }
    }
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
  for (int voice = 0; voice < VOICES; voice++) {
    seq.voices[voice].seek(next_step);
  }
  ppqn = (into_step + CLOCK_DIVISION - 1) % CLOCK_DIVISION;
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
  if (ppqn == (uint32_t)CLOCK_DIVISION - 1) {
    notes_off();
    midi_flush();
  } else if (ppqn >= (uint32_t)CLOCK_DIVISION) {
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
  notes_off();
  midi_flush();
}

// Song Position Pointer arrives while stopped, ahead of a Continue, and counts
// MIDI beats (sixteenths) from the start of the song.
void on_midi_song_position(uint32_t beats) {
  locate(beats * MIDI_CLOCKS_PER_BEAT);
}

// System Reset: stop, silence and rewind.
void on_midi_reset() {
  on_midi_stop();
  locate(0);
}

void handle_midi_in(midiEventPacket_t const &event) {
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
  uint32_t const now = millis();
  uint32_t const elapsed = now - last_step_time;
  uint32_t const step_interval = beat_interval / 4;
  ppqn = ((4 * 24 * elapsed) / beat_interval);
  if (elapsed >= step_interval) {
    run_step();
    ppqn = 0;
    last_step_time = now;
    global_pos++;
  } else if (elapsed >= step_interval - step_interval / CLOCK_DIVISION) {
    // one clock's worth ahead of the step, as in on_midi_clock()
    notes_off();
    midi_flush();
  };
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

// The UI is far more expensive than the clock path. show() has to expand all 32
// pixels into a 3KB DMA buffer, and if the previous NeoPixel frame is still
// going out (~1ms for 32 pixels on this board, which DMAs to a non-SERCOM pin)
// it blocks on that plus the 300us latch, while the keypad scan spends ~200us
// in per-column settling delays. Running either one per iteration made the loop
// period as long as a clock tick, so steps landed on loop boundaries instead of
// on their tick. Both now run on a frame timer while service_clock() gets
// called every iteration, and show_pixels() drops the frames that would repaint
// an unchanged grid, which is most of them between two steps.
uint32_t static const UI_FRAME_INTERVAL = 8; // ms, ~125Hz
uint32_t last_ui_frame = 0;

void loop() {
  service_clock();

  uint32_t now = millis();
  if (now - last_ui_frame < UI_FRAME_INTERVAL) {
    return;
  }
  last_ui_frame = now;

  trellis.tick();
  handle_keys();

  // Key handling can be slow (it writes to Serial), so pick up anything that
  // arrived during it before blocking on show().
  service_clock();

  render_pixels();
  show_pixels();
}
