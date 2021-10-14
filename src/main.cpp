#include <Adafruit_ADXL343.h>
#include <Adafruit_NeoTrellisM4.h>
#include <Adafruit_Sensor.h>
#include <Arduino.h>
#include <SPI.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <delay.h>
#include <deque>
#include <vector>

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

Sequencer seq = Sequencer();
uint32_t current_voice = 0;

std::deque<Pattern> undo_buffer;

void create_undo_step() {
  if (undo_buffer.size() > 0 && undo_buffer.back() == *seq.voice->pattern()) {
    return;
  }
  undo_buffer.push_back(Pattern(*seq.voice->pattern()));
  if (undo_buffer.size() > UNDO_LENGTH) {
    undo_buffer.pop_front();
  }
};

void undo() {
  if (undo_buffer.size() > 0) {
    seq.voice->replace_pattern(undo_buffer.back());
    undo_buffer.pop_back();
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
  trellis.fill(trellis.gamma32(COLOR_OFF));
  trellis.enableUSBMIDI(true);
  trellis.setUSBMIDIchannel(MIDI_CHANNEL);

  /* if(!accel.begin()) { */
  /*   Serial.println("No accelerometer found"); */
  /*   while(1); */
  /* } */

  for (int i = 0; i < VOICES; i++) {
    trellis.setPixelColor(voice_index_to_key(i),
                          trellis.gamma32(voice_index_to_color(i)));
  }

  trellis.setPixelColor(KEY_PATTERN_LEN, trellis.gamma32(COLOR_PMOD));
  trellis.setPixelColor(KEY_PATTERN_POS, trellis.gamma32(COLOR_PMOD));
  trellis.setPixelColor(KEY_ROTATE, trellis.gamma32(COLOR_PMOD));

  trellis.setPixelColor(KEY_COPY, trellis.gamma32(COLOR_PACT));
  trellis.setPixelColor(KEY_PASTE, trellis.gamma32(COLOR_PACT));
  trellis.setPixelColor(KEY_CLEAR, trellis.gamma32(COLOR_PACT));
  trellis.setPixelColor(KEY_UNDO, trellis.gamma32(COLOR_PACT));

  trellis.setPixelColor(KEY_VOICE_SELECT_ALL, trellis.gamma32(COLOR_PPOS));

  trellis.show();
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
      trellis.setPixelColor(voice_index_to_key(voice),
                            trellis.gamma32(COLOR_PPOS));
      seq.voices[voice].is_playing = true;
      is_voice_select_hl_period = true;
    }
  }
}

void loop() {
  voice_select_modifier_held = false;
  trellis.tick();

  for (uint32_t i = 0; i < 16; i++) {

    if (i == seq.voice->pos) {
      trellis.setPixelColor(step_key[i], trellis.gamma32(COLOR_PPOS));
    } else if (seq.voice->pattern()->length <= i) {
      trellis.setPixelColor(step_key[i], trellis.gamma32(COLOR_OFF));
    } else if (seq.voice->step(i).vel > 0) {
      trellis.setPixelColor(step_key[i], trellis.gamma32(seq_color_set));
    } else {
      trellis.setPixelColor(step_key[i], trellis.gamma32(seq_color_bg));
    }
  }

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
          Serial.println(" le\n");
        };
      } else if (trellis.isPressed(KEY_PATTERN_POS)) {
        if (is_numpad_key(key)) {
          uint32_t index = index_of(step_key, 16, key);

          create_undo_step();
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
          // trellis.setPixelColor(key, trellis.gamma32(COLOR_PPOS));
        }
      }
    } else if (e.bit.EVENT == KEY_JUST_RELEASED) {
      debug_print("key_released", key);

      if (key == KEY_VOICE_SELECT_0) {
        trellis.setPixelColor(key, trellis.gamma32(COLOR_VOC0));

      } else if (key == KEY_VOICE_SELECT_1) {
        trellis.setPixelColor(key, trellis.gamma32(COLOR_VOC1));

      } else if (key == KEY_VOICE_SELECT_2) {
        trellis.setPixelColor(key, trellis.gamma32(COLOR_VOC2));

      } else if (key == KEY_VOICE_SELECT_3) {
        trellis.setPixelColor(key, trellis.gamma32(COLOR_VOC3));

      } else if (key == KEY_VOICE_SELECT_4) {
        trellis.setPixelColor(key, trellis.gamma32(COLOR_VOC4));

      } else if (key == KEY_VOICE_SELECT_5) {
        trellis.setPixelColor(key, trellis.gamma32(COLOR_VOC5));

      } else {
        // trellis.setPixelColor(key, trellis.gamma32(COLOR_OFF));
      }
    }
  }

  trellis.show();

  int now = millis();

#ifdef INTERNAL_CLOCK
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
  if (is_voice_select_hl_period && ppqn >= 2) {
    is_voice_select_hl_period = false;
    for (int i = 0; i < VOICES; i++) {
      trellis.setPixelColor(voice_index_to_key(i),
                            trellis.gamma32(voice_index_to_color(i)));
    }
  }
}
