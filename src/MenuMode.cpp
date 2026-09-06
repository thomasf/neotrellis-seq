#include "MenuMode.h"
#include "UIMode.h"
#include "colors.h"
#include "config.h"
#include "constants.h"
#include "main.h"

MenuMode menu_mode;

static void exit_menu() { mode_manager.pop_mode(); }

static const KeyBinding MENU_BINDINGS[] = {
    {KEY_MENU, 0, 0, exit_menu},
};

static constexpr size_t MENU_BINDING_COUNT =
    sizeof(MENU_BINDINGS) / sizeof(MENU_BINDINGS[0]);

void MenuMode::on_enter() {
  current_page_ = (seq.voice_idx < VOICES) ? seq.voice_idx : 0;

  // Dim all pads initially
  fill_pixels(COLOR_OFF);

  // Light up MENU (Exit)
  set_pixel(KEY_MENU, COLOR_PPOS);

  // Light up the category / page selector buttons (voice keys 0..5)
  for (uint32_t i = 0; i < VOICES; i++) {
    set_pixel(voice_index_to_key(i), voice_index_to_color(i));
  }
}

void MenuMode::on_exit() {
  // Turning off menu display before returning to previous mode
  fill_pixels(COLOR_OFF);
}

void MenuMode::on_key(const KeyContext &ctx) {
  if (!ctx.pressed)
    return;

  // 1. Declarative menu actions (Exit/Back)
  if (dispatch_binding(MENU_BINDINGS, MENU_BINDING_COUNT, ctx)) {
    return;
  }

  // 2. Select menu page using voice pads (0..5)
  if (ctx.is_voice()) {
    current_page_ = ctx.voice_index();
    return;
  }

  // 3. Step keys: handle page-specific options
  if (ctx.is_step()) {
    uint32_t option_index = ctx.step_index();
    if (option_index == 0) {
      seq.toggle_protected(current_page_);
    } else if (option_index == 4) {
      seq.toggle_path_modifier(current_page_, PATH_PINGPONG);
    } else if (option_index == 5) {
      seq.toggle_path_modifier(current_page_, PATH_SPIRAL);
    } else if (option_index == 6) {
      seq.toggle_path_modifier(current_page_, PATH_VERTICAL);
    } else if (option_index == 7) {
      seq.toggle_path_modifier(current_page_, PATH_STRIDE);
    }
    return;
  }
}

void MenuMode::render_leds() {
  // Keep MENU indicator active
  set_pixel(KEY_MENU, COLOR_PPOS);
  set_pixel(KEY_CLEAR, COLOR_OFF);

  // Highlight the currently active menu page button
  for (uint32_t i = 0; i < VOICES; i++) {
    uint32_t key = voice_index_to_key(i);
    if (i == current_page_) {
      set_pixel(key, COLOR_PPOS);
    } else {
      set_pixel(key, voice_index_to_color(i));
    }
  }

  // Step 0 (top-left button): Voice Protect toggle
  bool const is_prot = seq.is_protected(current_page_);
  set_pixel(step_key[0], is_prot ? COLOR_GREEN : COLOR_RED);

  // Row 0 steps 1..3: unassigned
  for (uint32_t i = 1; i < 4; i++) {
    set_pixel(step_key[i], COLOR_OFF);
  }

  // Row 1 steps 4..7: Playback path modifiers
  set_pixel(step_key[4], seq.has_path_modifier(current_page_, PATH_PINGPONG)
                             ? COLOR_GREEN
                             : COLOR_RED);
  set_pixel(step_key[5], seq.has_path_modifier(current_page_, PATH_SPIRAL)
                             ? COLOR_GREEN
                             : COLOR_RED);
  set_pixel(step_key[6], seq.has_path_modifier(current_page_, PATH_VERTICAL)
                             ? COLOR_GREEN
                             : COLOR_RED);
  set_pixel(step_key[7], seq.has_path_modifier(current_page_, PATH_STRIDE)
                             ? COLOR_GREEN
                             : COLOR_RED);

  // Rows 2 and 3 (steps 8..15): unassigned
  for (uint32_t i = 8; i < 16; i++) {
    set_pixel(step_key[i], COLOR_OFF);
  }
}
