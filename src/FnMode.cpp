#include "FnMode.h"
#include "PatternPresets.h"
#include "SequencerMode.h"
#include "colors.h"
#include "config.h"
#include "constants.h"
#include "main.h"

FnMode fn_mode;
FnAllMode fn_all_mode;

// =============================================================================
// FnMode: Single-Voice Transforms & Presets (Hold FN)
// =============================================================================

static const KeyBinding FN_BINDINGS[] = {
    {KEY_PATTERN_POS, 0, 0, rewind_transport},
    {KEY_UNDO, 0, 0, redo},
    {KEY_PASTE, 0, 0, paste_all_slots},
};
static constexpr size_t FN_BINDING_COUNT = sizeof(FN_BINDINGS) / sizeof(FN_BINDINGS[0]);

void FnMode::on_enter() {}

void FnMode::on_exit() {}

void FnMode::handle_step(const KeyContext &ctx) {
  uint32_t const index = ctx.step_index();
  if (ctx.has(Mod::ACCENT)) {
    transform_pattern(apply_accent_transform, index);
  } else {
    transform_pattern(apply_transform, index);
  }
}

void FnMode::handle_voice(const KeyContext &ctx) {
  uint32_t const voice = ctx.voice_index();
  if (voice != seq.voice_idx) {
    swap_pattern(voice);
  }
}

void FnMode::on_key(const KeyContext &ctx) {
  if (!ctx.pressed) {
    if (ctx.key == KEY_FN) {
      mode_manager.pop_mode();
    }
    return;
  }

  // ALL pressed while in FnMode -> transition to FnAllMode
  if (ctx.key == KEY_VOICE_SELECT_ALL) {
    mode_manager.push_mode(&fn_all_mode);
    return;
  }

  if (dispatch_binding(FN_BINDINGS, FN_BINDING_COUNT, ctx)) {
    return;
  }

  if (ctx.is_step()) {
    handle_step(ctx);
    return;
  }

  if (ctx.is_voice()) {
    handle_voice(ctx);
    return;
  }
}

void FnMode::render_leds() {
  // Render step grid and voices identically to SequencerMode
  render_pixels();

  // Modifiers & action pads
  set_pixel(KEY_FN, COLOR_PPOS);               // Active held modifier
  set_pixel(KEY_VOICE_SELECT_ALL, COLOR_PMOD); // Press for ALL mode
  set_pixel(KEY_PATTERN_POS, COLOR_PMOD);      // Rewind
  set_pixel(KEY_UNDO, COLOR_PACT);             // Redo
  set_pixel(KEY_PASTE, COLOR_PACT);            // Paste All Slots
  set_pixel(KEY_ACCENT, (held_keys_mask & (1UL << KEY_ACCENT)) ? COLOR_PPOS : COLOR_PMOD);

  // Inactive buttons in FnMode
  set_pixel(KEY_COPY, COLOR_OFF);
  set_pixel(KEY_CLEAR, COLOR_OFF);
  set_pixel(KEY_MENU, COLOR_OFF);
  set_pixel(KEY_PATTERN_LEN, COLOR_OFF);
}

// =============================================================================
// FnAllMode: Multi-Voice Transforms & Kits (Hold FN + ALL)
// =============================================================================

static const KeyBinding FN_ALL_BINDINGS[] = {
    {KEY_PATTERN_LEN, 0, 0, polymeter_patterns}, {KEY_CLEAR, 0, 0, dropout_patterns},
    {KEY_PATTERN_POS, 0, 0, rewind_transport},   {KEY_UNDO, 0, 0, redo},
    {KEY_PASTE, 0, 0, paste_all_slots},
};
static constexpr size_t FN_ALL_BINDING_COUNT = sizeof(FN_ALL_BINDINGS) / sizeof(FN_ALL_BINDINGS[0]);

void FnAllMode::on_enter() {}

void FnAllMode::on_exit() {}

void FnAllMode::handle_step(const KeyContext &ctx) {
  uint32_t const index = ctx.step_index();
  if (ctx.has(Mod::ACCENT)) {
    load_kit_preset(index);
  } else {
    if (!transform_board(index)) {
      transform_all_patterns(apply_transform, index);
    }
  }
}

void FnAllMode::handle_voice(const KeyContext &ctx) {
  uint32_t const voice = ctx.voice_index();
  if (voice != seq.voice_idx) {
    swap_pattern(voice);
  }
}

void FnAllMode::on_key(const KeyContext &ctx) {
  if (!ctx.pressed) {
    if (ctx.key == KEY_VOICE_SELECT_ALL) {
      if (ctx.has(Mod::FN)) {
        mode_manager.pop_mode(); // Return to FnMode
      } else {
        mode_manager.switch_mode(&sequencer_mode);
      }
      return;
    }
    if (ctx.key == KEY_FN) {
      mode_manager.switch_mode(&sequencer_mode);
      return;
    }
    return;
  }

  if (dispatch_binding(FN_ALL_BINDINGS, FN_ALL_BINDING_COUNT, ctx)) {
    return;
  }

  if (ctx.is_step()) {
    handle_step(ctx);
    return;
  }

  if (ctx.is_voice()) {
    handle_voice(ctx);
    return;
  }
}

void FnAllMode::render_leds() {
  // Render step grid and voices identically to SequencerMode
  render_pixels();

  // Voice protection indicator overlay on voice pads (if protected)
  for (uint32_t i = 0; i < VOICES; i++) {
    if (!is_voice_flashing(i) && seq.is_protected(i)) {
      set_pixel(voice_index_to_key(i), COLOR_TOOL);
    }
  }

  // Modifiers & action pads
  set_pixel(KEY_FN, COLOR_PPOS);               // Both modifiers held
  set_pixel(KEY_VOICE_SELECT_ALL, COLOR_PPOS); // Both modifiers held
  set_pixel(KEY_PATTERN_LEN, 0x9040D0);        // Polymeter / Odd meters
  set_pixel(KEY_CLEAR, COLOR_RED);             // Dropout
  set_pixel(KEY_PATTERN_POS, COLOR_PMOD);      // Rewind
  set_pixel(KEY_UNDO, COLOR_PACT);             // Redo
  set_pixel(KEY_PASTE, COLOR_PACT);            // Paste All Slots
  set_pixel(KEY_ACCENT, (held_keys_mask & (1UL << KEY_ACCENT)) ? COLOR_PPOS : COLOR_PMOD);

  set_pixel(KEY_COPY, COLOR_OFF);
  set_pixel(KEY_MENU, COLOR_OFF);
}
