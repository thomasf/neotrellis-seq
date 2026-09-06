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
    {KEY_CLEAR, 0, 0, exit_menu},
};

static constexpr size_t MENU_BINDING_COUNT =
    sizeof(MENU_BINDINGS) / sizeof(MENU_BINDINGS[0]);

void MenuMode::on_enter() {
  // Dim all pads initially
  fill_pixels(COLOR_OFF);

  // Light up MENU and CLEAR (Back)
  set_pixel(KEY_MENU, COLOR_PPOS);
  set_pixel(KEY_CLEAR, COLOR_PACT);

  // Light up the category / page selector buttons (voice keys 0..5)
  for (int i = 0; i < VOICES; i++) {
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
    (void)option_index;
    // Extensible: dispatch option adjustment for current_page_
    return;
  }
}

void MenuMode::render_leds() {
  // Keep MENU indicator active
  set_pixel(KEY_MENU, COLOR_PPOS);
  set_pixel(KEY_CLEAR, COLOR_PACT);

  // Highlight the currently active menu page button
  for (uint32_t i = 0; i < VOICES; i++) {
    uint32_t key = voice_index_to_key(i);
    if (i == current_page_) {
      set_pixel(key, COLOR_PPOS);
    } else {
      set_pixel(key, voice_index_to_color(i));
    }
  }

  // Example menu display: draw options for current_page_ on the 16 step keys
  for (uint32_t i = 0; i < 16; i++) {
    // Can be customized per menu page
    set_pixel(step_key[i], (i == current_page_) ? COLOR_TOOL : COLOR_OFF);
  }
}
