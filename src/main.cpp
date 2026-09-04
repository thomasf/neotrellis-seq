#include <Adafruit_ADXL343.h>
#include <Adafruit_NeoTrellisM4.h>
#include <Adafruit_Sensor.h>
#include <Arduino.h>
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
  trellis.enableUSBMIDI(true);
  trellis.setUSBMIDIchannel(MIDI_CHANNEL);

  /* if(!accel.begin()) { */
  /*   Serial.println("No accelerometer found"); */
  /*   while(1); */
  /* } */

  for (int i = 0; i < VOICES; i++) {
    set_pixel(voice_index_to_key(i), voice_index_to_color(i));
  }

  set_pixel(KEY_PATTERN_LEN, COLOR_PMOD);
  set_pixel(KEY_PATTERN_POS, COLOR_PMOD);
  set_pixel(KEY_ROTATE, COLOR_PMOD);

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

bool voice_select_modifier_held = false;

// turn of any running notes
void notes_off() {
  for (int voice = 0; voice < VOICES; voice++) {
    if (seq.voices[voice].is_playing) {
      trellis.noteOff(FIRST_MIDI_NOTE + voice, MIDI_NOTE_OFF_VELOCITY);
      seq.voices[voice].is_playing = false;
    }
  }
}

bool is_voice_select_hl_period = false;
// run_step sends midi for the next/prev step
void run_step(bool next) {
  notes_off();
  Step current_step = Step();
  for (int voice = 0; voice < VOICES; voice++) {
    if (next) {
      current_step = seq.voices[voice].advance();
    } else {
      current_step = seq.voices[voice].step();
    }
    if (current_step.vel > 0) {
      trellis.noteOn(FIRST_MIDI_NOTE + voice, current_step.vel);
      set_pixel(voice_index_to_key(voice), COLOR_PPOS);
      seq.voices[voice].is_playing = true;
      is_voice_select_hl_period = true;
    }
  }
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
  voice_select_modifier_held = false;

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
            if (index == 0) {
              seq.voices[voice].pos = seq.voices[voice].pattern()->length - 1;
            } else {
              // TODO: need to decide when global time advances. Right now play
              // head is moved to the previous step as a work around.
              seq.voices[voice].pos = index - 1;
            }
          }
        } else {
          if (index == 0) {
            seq.voice->pos = seq.voice->pattern()->length - 1;
          } else {
            // TODO: need to decide when global time advances. Right now play
            // head is moved to the previous step as a work around.
            seq.voice->pos = index - 1;
          }
        };

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

          if (trellis.isPressed(KEY_VOICE_SELECT_ALL)) {
            voice_select_modifier_held = true;
            for (int voice = 0; voice < VOICES; voice++) {
              seq.voices[voice].pattern_idx = index;
            }
            reset_undo();
          } else {

            if (trellis.isPressed(KEY_VOICE_SELECT_0)) {
              voice_select_modifier_held = true;
              if (seq.voices[0].pattern_idx != index) {
                seq.voices[0].pattern_idx = index;
                reset_undo();
              }
            }

            if (trellis.isPressed(KEY_VOICE_SELECT_1)) {
              voice_select_modifier_held = true;
              if (seq.voices[1].pattern_idx != index) {
                seq.voices[1].pattern_idx = index;
                reset_undo();
              }
            }

            if (trellis.isPressed(KEY_VOICE_SELECT_2)) {
              voice_select_modifier_held = true;
              if (seq.voices[2].pattern_idx != index) {
                seq.voices[2].pattern_idx = index;
                reset_undo();
              }
            }

            if (trellis.isPressed(KEY_VOICE_SELECT_3)) {
              voice_select_modifier_held = true;
              if (seq.voices[3].pattern_idx != index) {
                seq.voices[3].pattern_idx = index;
                reset_undo();
              }
            }

            if (trellis.isPressed(KEY_VOICE_SELECT_4)) {
              voice_select_modifier_held = true;
              if (seq.voices[4].pattern_idx != index) {
                seq.voices[4].pattern_idx = index;
                reset_undo();
              }
            }

            if (trellis.isPressed(KEY_VOICE_SELECT_5)) {
              voice_select_modifier_held = true;
              if (seq.voices[5].pattern_idx != index) {
                seq.voices[5].pattern_idx = index;
                reset_undo();
              }
            }
          }

          if (trellis.isPressed(KEY_ROTATE)) {
            create_undo_step();
            if (index == 0) {
              std::reverse(seq.voice->pattern()->steps.begin(),
                           seq.voice->pattern()->steps.end());
            } else {
              rotate_array_elements(seq.voice->pattern()->steps, 16 - index);
            }
          } else if (!voice_select_modifier_held) {
            create_undo_step();
            if (seq.voice->pattern()->steps[index].vel == 0) {
              seq.voice->pattern()->steps[index].vel = 100;
            } else {
              seq.voice->pattern()->steps[index].vel = 0;
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

// service_clock consumes the pending clock messages and runs the steps they
// call for. This is the timing critical path and wants to be called as often
// as possible: clock ticks are only 2.1ms apart at 120 BPM.
void service_clock() {
#ifdef INTERNAL_CLOCK
  uint32_t now = millis();
  ppqn = ((4 * 24 * (now - last_step_time)) / beat_interval);
  if ((now - last_step_time) >= (beat_interval / 4)) {
    run_step(true);
    ppqn = 0;
    last_step_time = now;
    global_pos++;
  };
#else

  midiEventPacket_t event;
  do {
    event = MidiUSB.read();
    if (event.byte1 == _MIDI_MSG_CLOCK) {
      ++ppqn;
      if (ppqn == CLOCK_DIVISION) {
        global_pos++;
        run_step(true);
        MidiUSB.flush();
        ppqn = 0;
      };
    } else if (event.byte1 == _MIDI_MSG_CONT) {
      ppqn = 0;
    } else if (event.byte1 == _MIDI_MSG_START) {
      global_pos = 0;
      for (int voice = 0; voice < VOICES; voice++) {
        seq.voices[voice].pos = 0;
      }
      ppqn = 0;
      run_step(false);
      MidiUSB.flush();
    } else if (event.byte1 == _MIDI_MSG_STOP) {
      notes_off();
      MidiUSB.flush();
    }
  } while (event.header != 0);
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
