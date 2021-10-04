#include <Adafruit_ADXL343.h>
#include <Adafruit_NeoTrellisM4.h>
#include <Adafruit_Sensor.h>
#include <Arduino.h>
#include <SPI.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

#include "colors.h"
#include "config.h"
#include "delay.h"

template <typename T> int rotate_array_elements(T &v, int dir) {
  if (dir > 0) {
    std::rotate(v.rbegin(), v.rbegin() + dir, v.rend());
    return 0;
  } else if (dir < 0) {
    std::rotate(v.begin(), v.begin() + abs(dir), v.end());
    return 0;
  } else {
    return 1;
  }
}

Adafruit_ADXL343 accel = Adafruit_ADXL343(123, &Wire1);

int xCC = 1; // choose a CC number to control with x axis tilting of the board.
             // 1 is mod wheel, for example.

int last_xbend = 0;
int last_ybend = 0;

unsigned long start_time;
unsigned long last_step_time;
#define STEP_DURATION 120

Adafruit_NeoTrellisM4 trellis = Adafruit_NeoTrellisM4();

// Step represents a single sequencer step
class Step {
public:
  uint32_t v;
  Step() { v = 0; }
  Step(uint32_t value) { v = value; }
  Step(const Step &s) { v = s.v; }
};

// Pattern is a sequence of steps.
class Pattern {
public:
  uint32_t length;            // pattern length, up to 16 steps
  std::array<Step, 16> steps; // pattern data
  Pattern() { length = 16; }
  Pattern(const Pattern &p) {
    length = p.length;
    steps = p.steps;
  }
};

// Voice is a collection of patterns
class Voice {
public:
  std::array<Pattern, 16> patterns;
  bool is_playing;
  uint32_t pattern_idx;
  uint32_t pos;            // current position
  Step advance();          // advace to next step
  Step step();             // get current step value
  Step step(uint32_t idx); // get current step value for pos
  Pattern *pattern();
  void replace_pattern(const Pattern p);
  Voice() {
    pos = 0;
    pattern_idx = 0;
    is_playing = false;
  }
};
Step Voice::step(uint32_t idx) { return pattern()->steps[idx]; }
Step Voice::step() { return pattern()->steps[pos]; }
Step Voice::advance() {
  pos = (pos + 1) % pattern()->length;
  return pattern()->steps[pos];
}

void Voice::replace_pattern(const Pattern p) { patterns[pattern_idx] = p; }

// Sequencer is the main data type
class Sequencer {
public:
  std::array<Voice, VOICES> voices;
  Voice *voice;
  uint32_t voice_idx;
  Sequencer() {
    voice_idx = 0;
    voice = &voices[0];
  }
  void set_voice(uint32_t idx);
};

void Sequencer::set_voice(uint32_t idx) {
  voice_idx = idx;
  voice = &voices[idx];
};

Sequencer seq = Sequencer();

Pattern *Voice::pattern() { return &patterns[pattern_idx]; }

std::array<std::array<Step, 16>, 16> undo_buffer;

Pattern copy_buffer = Pattern();

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

// USB MIDI messages sent over the micro B USB port
#ifdef DEBUG
  Serial.println("Enabling MIDI on USB");
#endif
  trellis.enableUSBMIDI(true);
  trellis.setUSBMIDIchannel(MIDI_CHANNEL);

  /* if(!accel.begin()) { */
  /*   Serial.println("No accelerometer found"); */
  /*   while(1); */
  /* } */

  trellis.setPixelColor(KEY_VOICE_SELECT_0, trellis.gamma32(COLOR_VOC0));
  trellis.setPixelColor(KEY_VOICE_SELECT_1, trellis.gamma32(COLOR_VOC1));
  trellis.setPixelColor(KEY_VOICE_SELECT_2, trellis.gamma32(COLOR_VOC2));
  trellis.setPixelColor(KEY_VOICE_SELECT_3, trellis.gamma32(COLOR_VOC3));
  trellis.setPixelColor(KEY_VOICE_SELECT_4, trellis.gamma32(COLOR_VOC4));
  trellis.setPixelColor(KEY_VOICE_SELECT_5, trellis.gamma32(COLOR_VOC5));

  trellis.setPixelColor(KEY_PATTERN_LEN, trellis.gamma32(COLOR_PMOD));
  trellis.setPixelColor(KEY_PATTERN_POS, trellis.gamma32(COLOR_PMOD));
  trellis.setPixelColor(KEY_ROTATE, trellis.gamma32(COLOR_PMOD));

  trellis.setPixelColor(KEY_COPY, trellis.gamma32(COLOR_PACT));
  trellis.setPixelColor(KEY_PASTE, trellis.gamma32(COLOR_PACT));
  trellis.setPixelColor(KEY_CLEAR, trellis.gamma32(COLOR_PACT));
  // trellis.setPixelColor(KEY_UNDO, trellis.gamma32(COLOR_PACT));

  trellis.show();
  start_time = millis();
  last_step_time = start_time;
}

uint32_t globalPos = 0;

uint32_t current_voice = 0;
Step current_step = 0;

uint32_t seq_color_set = COLOR_VOC0_SET;
uint32_t seq_color_bg = COLOR_VOC0_UNSET;

const uint32_t step_key[16] = {
    // row 0
    KEY_SEQ_POS_0, KEY_SEQ_POS_1, KEY_SEQ_POS_2, KEY_SEQ_POS_3,
    // row 1
    KEY_SEQ_POS_4, KEY_SEQ_POS_5, KEY_SEQ_POS_6, KEY_SEQ_POS_7,
    // row 2
    KEY_SEQ_POS_8, KEY_SEQ_POS_9, KEY_SEQ_POS_10, KEY_SEQ_POS_11,
    // row 3
    KEY_SEQ_POS_12, KEY_SEQ_POS_13, KEY_SEQ_POS_14, KEY_SEQ_POS_15

};

uint32_t index_of(const uint32_t a[], uint32_t size, uint32_t value) {
  uint32_t index = 0;

  while (index < size && a[index] != value)
    ++index;

  return (index == size ? -1 : index);
}

bool voice_select_modifier_held = false;

#define is_numpad_key(key)                                                     \
  (key >= KEY_SEQ_POS_0 && key <= KEY_SEQ_POS_3) ||                            \
      (key >= KEY_SEQ_POS_4 && key <= KEY_SEQ_POS_7) ||                        \
      (key >= KEY_SEQ_POS_8 && key <= KEY_SEQ_POS_11) ||                       \
      (key >= KEY_SEQ_POS_12 && key <= KEY_SEQ_POS_15)

void loop() {
  voice_select_modifier_held = false;
  trellis.tick();

  for (uint32_t i = 0; i < 16; i++) {

    if (i == seq.voice->pos) {
      trellis.setPixelColor(step_key[i], trellis.gamma32(COLOR_PPOS));
    } else if (seq.voice->pattern()->length <= i) {
      trellis.setPixelColor(step_key[i], trellis.gamma32(COLOR_OFF));
    } else if (seq.voice->step(i).v > 0) {
      trellis.setPixelColor(step_key[i], trellis.gamma32(seq_color_set));
    } else {
      trellis.setPixelColor(step_key[i], trellis.gamma32(seq_color_bg));
    }
  }

  while (trellis.available()) {
    keypadEvent e = trellis.read();
    int key = e.bit.KEY;
#ifdef DEBUG
    Serial.print("Keypad key: ");
    Serial.println(key);
#endif

    if (e.bit.EVENT == KEY_JUST_PRESSED) {
#ifdef DEBUG
      Serial.println(" pressed\n");
#endif

      if (trellis.isPressed(KEY_PATTERN_LEN)) {
#ifdef DEBUG
        Serial.println("pattern length");
#endif
        if (is_numpad_key(key)) {
          uint32_t index = index_of(step_key, 16, key);
#ifdef DEBUG
          Serial.print(" setting new length for pattern ");
          Serial.println(index);
#endif

          seq.voice->pattern()->length = index + 1;
          Serial.println(" le\n");
        };
      } else if (trellis.isPressed(KEY_PATTERN_POS)) {
#ifdef DEBUG
        Serial.println("pattern pos");
#endif
        if (is_numpad_key(key)) {
          uint32_t index = index_of(step_key, 16, key);
#ifdef DEBUG
          Serial.print(" setting new position for pattern ");
          Serial.println(index);
#endif
          if (index == 0) {
            seq.voice->pos = seq.voice->pattern()->length;
          } else {
            // TODO: need to decide when global time advances. Right now play
            // head is moved to the previous step as a work around.
            seq.voice->pos = index - 1;
          }
        };

      } else {

        if (key == KEY_VOICE_SELECT_0) {
          seq.set_voice(0);
          seq_color_set = COLOR_VOC0_SET;
          seq_color_bg = COLOR_VOC0_UNSET;

        } else if (key == KEY_VOICE_SELECT_1) {
          seq.set_voice(1);
          seq_color_set = COLOR_VOC1_SET;
          seq_color_bg = COLOR_VOC1_UNSET;

        } else if (key == KEY_VOICE_SELECT_2) {
          seq.set_voice(2);
          seq_color_set = COLOR_VOC2_SET;
          seq_color_bg = COLOR_VOC2_UNSET;

        } else if (key == KEY_VOICE_SELECT_3) {
          seq.set_voice(3);
          seq_color_set = COLOR_VOC3_SET;
          seq_color_bg = COLOR_VOC3_UNSET;

        } else if (key == KEY_VOICE_SELECT_4) {
          seq.set_voice(4);
          seq_color_set = COLOR_VOC4_SET;
          seq_color_bg = COLOR_VOC4_UNSET;

        } else if (key == KEY_VOICE_SELECT_5) {
          seq.set_voice(5);
          seq_color_set = COLOR_VOC5_SET;
          seq_color_bg = COLOR_VOC5_UNSET;

        } else if (key == KEY_COPY) {
          copy_buffer = Pattern(*seq.voice->pattern());

        } else if (key == KEY_PASTE) {
          seq.voice->replace_pattern(Pattern(copy_buffer));

        } else if (key == KEY_CLEAR) {
          for (int i = 0; i < 16; i++) {
            seq.voice->pattern()->steps[i] = Step(0);
          }
        } else if (is_numpad_key(key)) {

          uint32_t index = index_of(step_key, 16, key);
#ifdef DEBUG
          Serial.print("index: ");
          Serial.println(index);
#endif

          if (trellis.isPressed(KEY_VOICE_SELECT_0)) {
            voice_select_modifier_held = true;
            seq.voices[0].pattern_idx = index;
          }

          if (trellis.isPressed(KEY_VOICE_SELECT_1)) {
            voice_select_modifier_held = true;
            seq.voices[1].pattern_idx = index;
          }

          if (trellis.isPressed(KEY_VOICE_SELECT_2)) {
            voice_select_modifier_held = true;
            seq.voices[2].pattern_idx = index;
          }

          if (trellis.isPressed(KEY_VOICE_SELECT_3)) {
            voice_select_modifier_held = true;
            seq.voices[3].pattern_idx = index;
          }

          if (trellis.isPressed(KEY_VOICE_SELECT_4)) {
            voice_select_modifier_held = true;
            seq.voices[4].pattern_idx = index;
          }

          if (trellis.isPressed(KEY_VOICE_SELECT_5)) {
            voice_select_modifier_held = true;
            seq.voices[5].pattern_idx = index;
          }

          if (trellis.isPressed(KEY_ROTATE)) {
            if (index == 0) {
              std::reverse(seq.voice->pattern()->steps.begin(),
                           seq.voice->pattern()->steps.end());
            } else {
              rotate_array_elements(seq.voice->pattern()->steps, index);
            }
          } else if (!voice_select_modifier_held) {
            if (seq.voice->pattern()->steps[index].v == 0) {
              seq.voice->pattern()->steps[index].v = 100;
            } else {
              seq.voice->pattern()->steps[index].v = 0;
            }
          }

        } else {
          // trellis.setPixelColor(key, trellis.gamma32(COLOR_PPOS));
        }
      }
    } else if (e.bit.EVENT == KEY_JUST_RELEASED) {
#ifdef DEBUG
      Serial.println(" released\n");
#endif
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
  if (now - last_step_time > STEP_DURATION) {
    last_step_time = now;
    for (int voice = 0; voice < VOICES; voice++) {
      if (seq.voices[voice].is_playing) {
        trellis.noteOff(FIRST_MIDI_NOTE + voice, MIDI_NOTE_OFF_VELOCITY);
        seq.voices[voice].is_playing = false;
      }
    }
    trellis.sendMIDI();
    for (int voice = 0; voice < VOICES; voice++) {
      current_step = seq.voices[voice].advance();
      if (current_step.v > 0) {
        seq.voices[voice].is_playing = true;
        trellis.noteOn(FIRST_MIDI_NOTE + voice, current_step.v);
      }
    }
  }
  trellis.sendMIDI();
  delayMicroseconds(10);
  globalPos++;
}
