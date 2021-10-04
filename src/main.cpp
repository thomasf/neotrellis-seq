#include <Adafruit_ADXL343.h>
#include <Adafruit_NeoTrellisM4.h>
#include <Adafruit_Sensor.h>
#include <Arduino.h>
#include <SPI.h>
#include <array>
#include <cstdint>

#include "colors.h"
#include "config.h"
#include "delay.h"

Adafruit_ADXL343 accel = Adafruit_ADXL343(123, &Wire1);

int xCC = 1; // choose a CC number to control with x axis tilting of the board.
             // 1 is mod wheel, for example.

int last_xbend = 0;
int last_ybend = 0;

unsigned long start_time;
unsigned long last_step_time;
#define STEP_DURATION 120

Adafruit_NeoTrellisM4 trellis = Adafruit_NeoTrellisM4();

// TODO: Step represents a single sequencer step
class Step {
  uint32_t value;
};

// Pattern is a sequence of steps.
class Pattern {

  // NOTE: pattern undo data here uses about 50% of total RAM, step values
  // needs to beats 32 bit types because they will contain more than just note
  // velocity later. If memory becomes an issue we don't have to store undo
  // locally for each pattern.

public:
  uint32_t length;                // pattern length, up to 16 steps
  uint32_t pos;                   // current position
  std::array<uint32_t, 16> steps; // pattern data
  std::array<std::array<uint32_t, 16>, 16> undo; // pattern undo data
  uint32_t advance();                            // advace to next step
  uint32_t get();                                // get current step value
  uint32_t get(uint32_t idx); // get current step value for pos
  Pattern() {
    pos = 0;
    length = 16;
    for (int i = 0; i < 16; i++) {
      steps[i] = 0;
      for (int j = 0; j < 16; j++) {
        undo[i][j] = 0;
      }
    }
  }
  Pattern(const Pattern &p) {
    pos = p.pos;
    length = p.length;
    steps = p.steps;
    for (int i = 0; i < 16; i++) {
      for (int j = 0; j < 16; j++) {
        undo[i][j] = 0;
      }
    }
  }
};

uint32_t Pattern::advance() {
  pos = (pos + 1) % length;
  return steps[pos];
}

uint32_t Pattern::get() { return steps[pos]; }

uint32_t Pattern::get(uint32_t idx) { return steps[idx]; }

// TODO: Voice is a collection of patterns
class Voice {
public:
  Pattern patterns[16];
};

// std::array<Voice, VOICES> voices;

Pattern copy_buffer;
Pattern patterns[VOICES][16]; // 16 patterns for each voice
uint32_t current_patterns[VOICES];

void setup() {
  Serial.begin(115200);
  for (uint32_t i = 0; i < VOICES; i++) {
    current_patterns[i] = 0;
  }
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

  trellis.show();
  start_time = millis();
  last_step_time = start_time;
}

uint32_t globalPos = 0;

uint32_t current_voice = 0;
uint32_t current_step_value = 0;

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
    if (i == patterns[current_voice][current_patterns[current_voice]].pos) {
      trellis.setPixelColor(step_key[i], trellis.gamma32(COLOR_PPOS));
    } else if (patterns[current_voice][current_patterns[current_voice]]
                   .length <= i) {
      trellis.setPixelColor(step_key[i], trellis.gamma32(COLOR_OFF));
    } else if (patterns[current_voice][current_patterns[current_voice]].get(i) >
               0) {
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
          patterns[current_voice][current_patterns[current_voice]].length =
              index + 1;
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
          if ((index - 1) < 0) {
            patterns[current_voice][current_patterns[current_voice]].pos =
                patterns[current_voice][current_patterns[current_voice]].length;
          } else {
            // TODO: need to decide when global time advances. Right now play
            // head is moved to the previous step as a work around.
            patterns[current_voice][current_patterns[current_voice]].pos =
                index - 1;
          }
        };

      } else {

        if (key == KEY_VOICE_SELECT_0) {
          current_voice = 0;
          seq_color_set = COLOR_VOC0_SET;
          seq_color_bg = COLOR_VOC0_UNSET;

        } else if (key == KEY_VOICE_SELECT_1) {
          current_voice = 1;
          seq_color_set = COLOR_VOC1_SET;
          seq_color_bg = COLOR_VOC1_UNSET;

        } else if (key == KEY_VOICE_SELECT_2) {
          current_voice = 2;
          seq_color_set = COLOR_VOC2_SET;
          seq_color_bg = COLOR_VOC2_UNSET;

        } else if (key == KEY_VOICE_SELECT_3) {
          current_voice = 3;
          seq_color_set = COLOR_VOC3_SET;
          seq_color_bg = COLOR_VOC3_UNSET;

        } else if (key == KEY_VOICE_SELECT_4) {
          current_voice = 4;
          seq_color_set = COLOR_VOC4_SET;
          seq_color_bg = COLOR_VOC4_UNSET;

        } else if (key == KEY_VOICE_SELECT_5) {
          current_voice = 5;
          seq_color_set = COLOR_VOC5_SET;
          seq_color_bg = COLOR_VOC5_UNSET;

        } else if (key == KEY_COPY) {
          copy_buffer =
              Pattern(patterns[current_voice][current_patterns[current_voice]]);

        } else if (key == KEY_PASTE) {
          copy_buffer.pos =
              patterns[current_voice][current_patterns[current_voice]].pos;
          patterns[current_voice][current_patterns[current_voice]] =
              Pattern(copy_buffer);

        } else if (key == KEY_CLEAR) {
          for (int i = 0; i < 16; i++) {
            patterns[current_voice][current_patterns[current_voice]].steps[i] =
                0;
          }

        } else if (is_numpad_key(key)) {

          uint32_t index = index_of(step_key, 16, key);
#ifdef DEBUG
          Serial.print("index: ");
          Serial.println(index);
          Serial.print("value: ");
          Serial.println(
              patterns[current_voice][current_patterns[current_voice]]
                  .steps[index]);
#endif

          if (trellis.isPressed(KEY_VOICE_SELECT_0)) {
            patterns[0][index].pos = patterns[0][current_patterns[0]].pos;
            voice_select_modifier_held = true;
            current_patterns[0] = index;
          }

          if (trellis.isPressed(KEY_VOICE_SELECT_1)) {
            patterns[1][index].pos = patterns[1][current_patterns[1]].pos;
            voice_select_modifier_held = true;
            current_patterns[1] = index;
          }

          if (trellis.isPressed(KEY_VOICE_SELECT_2)) {
            patterns[2][index].pos = patterns[2][current_patterns[2]].pos;
            voice_select_modifier_held = true;
            current_patterns[2] = index;
          }

          if (trellis.isPressed(KEY_VOICE_SELECT_3)) {
            patterns[3][index].pos = patterns[3][current_patterns[3]].pos;
            voice_select_modifier_held = true;
            current_patterns[3] = index;
          }

          if (trellis.isPressed(KEY_VOICE_SELECT_4)) {
            patterns[4][index].pos = patterns[4][current_patterns[4]].pos;
            voice_select_modifier_held = true;
            current_patterns[4] = index;
          }

          if (trellis.isPressed(KEY_VOICE_SELECT_5)) {
            patterns[5][index].pos = patterns[5][current_patterns[5]].pos;
            voice_select_modifier_held = true;
            current_patterns[5] = index;
          }

          if (!voice_select_modifier_held) {
            if (patterns[current_voice][current_patterns[current_voice]]
                    .steps[index] == 0) {
              patterns[current_voice][current_patterns[current_voice]]
                  .steps[index] = 100;

            } else {
              patterns[current_voice][current_patterns[current_voice]]
                  .steps[index] = 0;
            }
          }

        } else {
          trellis.setPixelColor(key, trellis.gamma32(COLOR_PPOS));
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
        trellis.setPixelColor(key, trellis.gamma32(COLOR_OFF));
      }
    }
  }
  trellis.show();

  int now = millis();
  if (now - last_step_time > STEP_DURATION) {
    last_step_time = now;

    for (int voice = 0; voice < VOICES; voice++) {

      // if (patterns[voice][currentPattern].steps[currentPos] %
      // patterns[voice][currentPattern].length >
      // 0) {
      // TODO: turn off only playing notes
      trellis.noteOff(FIRST_MIDI_NOTE + voice, 64);
      // }
    }
    trellis.sendMIDI();
    for (int voice = 0; voice < VOICES; voice++) {
      current_step_value = patterns[voice][current_patterns[voice]].advance();

      if (current_step_value > 0) {
        trellis.noteOn(FIRST_MIDI_NOTE + voice, current_step_value);
      }
    }
    trellis.sendMIDI();
  }
  delayMicroseconds(10);
  globalPos++;
}
