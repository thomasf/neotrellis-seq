#include "MenuMode.h"
#include "SleepMode.h"
#include "UIMode.h"
#include "VoiceMenuMode.h"
#include "colors.h"
#include "config.h"
#include "constants.h"
#include "main.h"

MenuMode menu_mode;

static void exit_menu() { mode_manager.pop_mode(); }

static const KeyBinding MENU_BINDINGS[] = {
    {KEY_MENU, 0, 0, exit_menu},
};

static constexpr size_t MENU_BINDING_COUNT = sizeof(MENU_BINDINGS) / sizeof(MENU_BINDINGS[0]);

void MenuMode::on_enter() {
  current_page_ = (seq.voice_idx < VOICES) ? seq.voice_idx : 0;
  pending_voice_ = VOICES;
  voice_press_time_ = 0;
  all_pressed_ = false;
  all_stop_triggered_ = false;
  all_sleep_triggered_ = false;
  all_press_time_ = 0;

  // Dim all pads initially
  fill_pixels(COLOR_OFF);

  // Light up MENU (Exit)
  set_pixel(KEY_MENU, COLOR_PPOS);

  // Light up the category / page selector buttons (voice keys 0..5)
  for (uint32_t i = 0; i < VOICES; i++) {
    set_pixel(voice_index_to_key(i), voice_index_to_color(i));
  }
}

void MenuMode::set_page(uint32_t page) {
  current_page_ = (page < VOICES) ? page : 0;
  select_voice(current_page_);
}

void MenuMode::on_exit() {
  select_voice(current_page_);
  pending_voice_ = VOICES;
  all_pressed_ = false;
  all_stop_triggered_ = false;
  all_sleep_triggered_ = false;
  // Turning off menu display before returning to previous mode
  fill_pixels(COLOR_OFF);
}

void MenuMode::on_key(const KeyContext &ctx) {
  // 1. Declarative menu actions (Exit/Back)
  if (dispatch_binding(MENU_BINDINGS, MENU_BINDING_COUNT, ctx)) {
    pending_voice_ = VOICES;
    return;
  }

  // 2. Handle ALL button: short tap while stopped starts playback;
  // holding for >= 1s stops playback; holding for >= 2s enters sleep mode.
  if (ctx.key == KEY_VOICE_ALL) {
    if (ctx.pressed) {
      all_pressed_ = true;
      all_stop_triggered_ = false;
      all_sleep_triggered_ = false;
      all_press_time_ = millis();
    } else {
      if (all_pressed_) {
        all_pressed_ = false;
        if (!all_stop_triggered_ && !all_sleep_triggered_) {
          // If stopped and tapped (< HOLD_MEDIUM_MS), start playback.
          // If already running, short tap is ignored so it won't stop by mistake.
          if (!clock_running && (millis() - all_press_time_ < HOLD_MEDIUM_MS)) {
            start_playback();
          }
        }
      }
    }
    return;
  }

  // 3. Fast combo: if a step key is pressed while a voice key is held, immediately activate
  // VoiceMenuMode
  if (ctx.pressed && ctx.is_step() && (ctx.any_voice_held() || pending_voice_ < VOICES)) {
    uint32_t voice = ctx.held_voice_index();
    if (voice >= VOICES && pending_voice_ < VOICES) {
      voice = pending_voice_;
    }
    if (voice < VOICES) {
      pending_voice_ = VOICES;
      set_page(voice);
      voice_menu_mode.set_voice(voice);
      mode_manager.push_mode(&voice_menu_mode);
      uint32_t const step_idx = ctx.step_index();
      uint8_t const new_note = step_to_drum_rack_note(step_idx);
      seq.set_or_swap_note(voice, new_note);
      return;
    }
  }

  // 4. Select menu page using voice pads (0..5), with hold timer to prevent flicker
  if (ctx.is_voice()) {
    if (ctx.pressed) {
      set_page(ctx.voice_index());
      pending_voice_ = ctx.voice_index();
      voice_press_time_ = millis();
    } else {
      if (pending_voice_ == ctx.voice_index()) {
        pending_voice_ = VOICES;
      }
    }
    return;
  }

  // 5. Step keys: handle page-specific options
  if (ctx.pressed && ctx.is_step()) {
    uint32_t option_index = ctx.step_index();
    if (option_index == 0) {
      seq.toggle_protected(current_page_);
      return;
    }
    if (option_index >= PATH_MODIFIER_FIRST_STEP &&
        option_index < PATH_MODIFIER_FIRST_STEP + NUM_ACTIVE_PATH_MODIFIERS) {
      uint32_t const mod_idx = option_index - PATH_MODIFIER_FIRST_STEP;
      seq.toggle_path_modifier(current_page_, ACTIVE_PATH_MODIFIERS[mod_idx]);
      return;
    }
    return;
  }
}

void MenuMode::render_leds() {
  // Check ALL button hold timers:
  // 1. Hold >= HOLD_LONG_MS (2s) enters SleepMode
  // 2. Hold >= HOLD_MEDIUM_MS (1s) stops running playback
  if (all_pressed_) {
    uint32_t const elapsed = millis() - all_press_time_;
    if (!all_sleep_triggered_ && elapsed >= HOLD_LONG_MS) {
      all_sleep_triggered_ = true;
      all_pressed_ = false;
      mode_manager.push_mode(&sleep_mode);
      return;
    } else if (clock_running && !all_stop_triggered_ && elapsed >= HOLD_MEDIUM_MS) {
      all_stop_triggered_ = true;
      stop_playback();
    }
  }

  // Check if a voice key has been held down for >= HOLD_SHORT_MS
  if (pending_voice_ < VOICES) {
    if (millis() - voice_press_time_ >= HOLD_SHORT_MS) {
      uint32_t const v = pending_voice_;
      pending_voice_ = VOICES;
      set_page(v);
      voice_menu_mode.set_voice(v);
      mode_manager.push_mode(&voice_menu_mode);
      return;
    }
  }

  // Keep MENU indicator active
  set_pixel(KEY_MENU, COLOR_PPOS);
  set_pixel(KEY_MID_3, COLOR_OFF);

  // ALL button: indicates playback state (green = playing, red = stopped).
  // Lights white when actively pressed down. If 1s stop hold was triggered,
  // show red to give immediate feedback that playback has stopped.
  if (all_pressed_) {
    if (all_stop_triggered_) {
      set_pixel(KEY_VOICE_ALL, COLOR_RED);
    } else {
      set_pixel(KEY_VOICE_ALL, COLOR_PPOS);
    }
  } else {
    set_pixel(KEY_VOICE_ALL, clock_running ? COLOR_GREEN : COLOR_RED);
  }

  // Highlight the currently active menu page button
  // Flashes in sync with the music: inverted (turns LED off instead of white)
  for (uint32_t i = 0; i < VOICES; i++) {
    uint32_t key = voice_index_to_key(i);
    if (is_voice_flashing(i)) {
      set_pixel(key, COLOR_OFF);
    } else if (i == current_page_) {
      set_pixel(key, COLOR_PPOS);
    } else {
      set_pixel(key, voice_index_to_color(i));
    }
  }

  // Step 0 (top-left button): Voice Protect toggle
  bool const is_prot = seq.is_protected(current_page_);
  set_pixel(step_key[0], is_prot ? COLOR_GREEN : COLOR_RED);

  // Steps before path modifiers: unassigned
  for (uint32_t i = 1; i < PATH_MODIFIER_FIRST_STEP; i++) {
    set_pixel(step_key[i], COLOR_OFF);
  }

  // Active playback path modifiers
  for (uint32_t i = 0; i < NUM_ACTIVE_PATH_MODIFIERS && (PATH_MODIFIER_FIRST_STEP + i < 16); i++) {
    uint32_t const step_idx = PATH_MODIFIER_FIRST_STEP + i;
    set_pixel(step_key[step_idx], seq.has_path_modifier(current_page_, ACTIVE_PATH_MODIFIERS[i])
                                      ? COLOR_GREEN
                                      : COLOR_RED);
  }

  // Remaining steps after path modifiers: unassigned
  for (uint32_t i = PATH_MODIFIER_FIRST_STEP + NUM_ACTIVE_PATH_MODIFIERS; i < 16; i++) {
    set_pixel(step_key[i], COLOR_OFF);
  }
}
