#include "SleepMode.h"
#include "colors.h"
#include "config.h"
#include "constants.h"
#include "main.h"

SleepMode sleep_mode;

void SleepMode::on_enter() {
  stop_playback();
  fill_pixels(COLOR_OFF);
  set_pixel(KEY_VOICE_ALL, COLOR_DIM_ALL);
}

void SleepMode::on_exit() { fill_pixels(COLOR_OFF); }

void SleepMode::on_key(const KeyContext &ctx) {
  if (ctx.key == KEY_VOICE_ALL && ctx.pressed) {
    mode_manager.pop_mode();
  }
}

void SleepMode::render_leds() {
  fill_pixels(COLOR_OFF);
  set_pixel(KEY_VOICE_ALL, COLOR_DIM_ALL);
}
