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

void create_undo_step() {
  if (!undo_buffer.empty() && undo_buffer.back() == *seq.voice->pattern()) {
    return;
  }
  undo_buffer.push(*seq.voice->pattern());
};

void undo() {
  if (!undo_buffer.empty()) {
    seq.voice->replace_pattern(undo_buffer.back());
    undo_buffer.pop();
  }
};

void reset_undo() { undo_buffer.clear(); };

Pattern copy_buffer = Pattern(); // for copy/paste

uint32_t random_below(uint32_t n) { return random(n); }

// apply_transform applies the TRANSFORM + STEP action for the step key at
// `index` (0-15, row major) to pattern `p` and reports whether the key is
// assigned:
//
//   row 0: shift right by 1, 2, 3 or 4 steps
//   row 1: shift left by 1, 2, 3 or 4 steps
//   row 2: deterministic reshapes: reverse, invert, euclid, unassigned
//   row 3: random reshapes: shuffle, the rest unassigned
bool apply_transform(Pattern *const p, uint32_t index) {
  if (index < 4) {
    p->shift(index + 1);
  } else if (index < 8) {
    p->shift(-(int)(index - 3));
  } else if (index == 8) {
    p->reverse();
  } else if (index == 9) {
    p->invert();
  } else if (index == 10) {
    p->euclid();
  } else if (index == 12) {
    p->shuffle(random_below);
  } else {
    return false;
  }
  return true;
}

// seed_random reseeds the stock generator, which is deterministic from boot,
// from the time of the key press, which is as random as the player.
void seed_random() { randomSeed(micros()); }

// transform_pattern applies transform `index` to the selected pattern, with
// an undo step. An unassigned key leaves at most a redundant undo entry,
// which create_undo_step already collapses when nothing has changed.
void transform_pattern(uint32_t index) {
  create_undo_step();
  seed_random();
  apply_transform(seq.voice->pattern(), index);
}

// transform_all_patterns applies transform `index` to every voice's current
// pattern (TRANSFORM + ALL + STEP). The undo buffer only knows the selected
// voice's pattern, and restoring just that one would leave the kit half
// transformed, so it is cleared instead.
void transform_all_patterns(uint32_t index) {
  seed_random();
  bool changed = false;
  for (uint32_t voice = 0; voice < VOICES; voice++) {
    changed |= apply_transform(seq.voices[voice].pattern(), index);
  }
  if (changed) {
    reset_undo();
  }
}

// swap_pattern exchanges the selected voice's current pattern with voice
// `other`'s current pattern (TRANSFORM + VOICE). The undo buffer only knows
// the selected voice's pattern, so it is cleared just as when a different
// pattern is selected; pressing the same chord again swaps back.
void swap_pattern(uint32_t other) {
  if (other >= VOICES || other == seq.voice_idx) {
    return;
  }
  std::swap(*seq.voice->pattern(), *seq.voices[other].pattern());
  reset_undo();
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

uint32_t seq_color_set = COLOR_VOC0_SET;
uint32_t seq_color_bg = COLOR_VOC0_UNSET;

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
    if (seq.voices[voice].is_playing) {
      midi_note_off(FIRST_MIDI_NOTE + voice, MIDI_NOTE_OFF_VELOCITY);
      seq.voices[voice].is_playing = false;
    }
  }
}

bool is_voice_select_hl_period = false;
// run_step moves every voice to its next step (or the step it was seeked to)
// and sends its note ons in one USB transfer.
void run_step() {
  notes_off();
  for (int voice = 0; voice < VOICES; voice++) {
    Step const current_step = seq.voices[voice].advance();
    if (current_step.vel > 0) {
      midi_note_on(FIRST_MIDI_NOTE + voice, current_step.vel);
      set_pixel(voice_index_to_key(voice), COLOR_PPOS);
      seq.voices[voice].is_playing = true;
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

      if (trellis.isPressed(KEY_PATTERN_LEN)) {
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
        swap_pattern(voice_key_to_index(key));

      } else {

        if (key == KEY_VOICE_SELECT_0 && seq.voice_idx != 0) {
          reset_undo();
          seq.set_voice(0);
          seq_color_set = COLOR_VOC0_SET;
          seq_color_bg = COLOR_VOC0_UNSET;

        } else if (key == KEY_VOICE_SELECT_1 && seq.voice_idx != 1) {
          reset_undo();
          seq.set_voice(1);
          seq_color_set = COLOR_VOC1_SET;
          seq_color_bg = COLOR_VOC1_UNSET;

        } else if (key == KEY_VOICE_SELECT_2 && seq.voice_idx != 2) {
          reset_undo();
          seq.set_voice(2);
          seq_color_set = COLOR_VOC2_SET;
          seq_color_bg = COLOR_VOC2_UNSET;

        } else if (key == KEY_VOICE_SELECT_3 && seq.voice_idx != 3) {
          reset_undo();
          seq.set_voice(3);
          seq_color_set = COLOR_VOC3_SET;
          seq_color_bg = COLOR_VOC3_UNSET;

        } else if (key == KEY_VOICE_SELECT_4 && seq.voice_idx != 4) {
          reset_undo();
          seq.set_voice(4);
          seq_color_set = COLOR_VOC4_SET;
          seq_color_bg = COLOR_VOC4_UNSET;

        } else if (key == KEY_VOICE_SELECT_5 && seq.voice_idx != 5) {
          reset_undo();
          seq.set_voice(5);
          seq_color_set = COLOR_VOC5_SET;
          seq_color_bg = COLOR_VOC5_UNSET;

        } else if (key == KEY_UNDO) {
          undo();

        } else if (key == KEY_COPY) {
          copy_buffer = Pattern(*seq.voice->pattern());

        } else if (key == KEY_PASTE) {
          create_undo_step();
          seq.voice->replace_pattern(Pattern(copy_buffer));

        } else if (key == KEY_CLEAR) {
          create_undo_step();
          for (int i = 0; i < 16; i++) {
            seq.voice->pattern()->steps[i] = Step(0);
          }
        } else if (is_numpad_key(key)) {

          uint32_t index = index_of(step_key, 16, key);
          debug_print("index", index);

          if (trellis.isPressed(KEY_TRANSFORM)) {
            if (trellis.isPressed(KEY_VOICE_SELECT_ALL)) {
              transform_all_patterns(index);
            } else {
              transform_pattern(index);
            }

          } else if (trellis.isPressed(KEY_VOICE_SELECT_ALL)) {
            for (int voice = 0; voice < VOICES; voice++) {
              seq.voices[voice].pattern_idx = index;
            }
            reset_undo();

          } else {
            bool voice_select_modifier_held = false;
            for (uint32_t voice = 0; voice < VOICES; voice++) {
              if (!trellis.isPressed(voice_index_to_key(voice))) {
                continue;
              }
              voice_select_modifier_held = true;
              if (seq.voices[voice].pattern_idx != index) {
                seq.voices[voice].pattern_idx = index;
                reset_undo();
              }
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
