#include "SequencerMode.h"
#include "FnMode.h"
#include "MenuMode.h"
#include "colors.h"
#include "config.h"
#include "constants.h"
#include "main.h"

SequencerMode sequencer_mode;

static const KeyBinding SEQUENCER_BINDINGS[] = {
    // Edit actions
    {KEY_CLEAR, Mod::ACCENT, 0, clear_accents},
    {KEY_CLEAR, 0, 0, clear_pattern},
    {KEY_COPY, 0, 0, copy_pattern},
    {KEY_PASTE, 0, 0, paste_single},
    {KEY_UNDO, 0, 0, undo},

    // Modal Menu
    {KEY_MENU, 0, 0, open_menu},
};

static constexpr size_t BINDING_COUNT = sizeof(SEQUENCER_BINDINGS) / sizeof(SEQUENCER_BINDINGS[0]);

void SequencerMode::on_enter() {
  for (uint32_t i = 0; i < VOICES; i++) {
    set_pixel(voice_index_to_key(i), voice_index_to_color(i));
  }

  set_pixel(KEY_PATTERN_LEN, COLOR_PMOD);
  set_pixel(KEY_PATTERN_POS, COLOR_PMOD);
  set_pixel(KEY_FN, COLOR_PMOD);
  set_pixel(KEY_ACCENT, COLOR_PMOD);

  set_pixel(KEY_COPY, COLOR_PACT);
  set_pixel(KEY_PASTE, COLOR_PACT);
  set_pixel(KEY_MENU, COLOR_PACT);
  set_pixel(KEY_CLEAR, COLOR_PACT);
  set_pixel(KEY_UNDO, COLOR_PACT);

  set_pixel(KEY_VOICE_SELECT_ALL, COLOR_PPOS);
}

void SequencerMode::on_exit() {}

void SequencerMode::handle_release(const KeyContext &ctx) {
  if (ctx.is_voice()) {
    set_pixel(ctx.key, voice_key_to_color(ctx.key));
  }
}

void SequencerMode::handle_step(const KeyContext &ctx) {
  uint32_t index = ctx.step_index();

  if (ctx.has(Mod::LEN)) {
    create_undo_step();
    seq.voice->pattern()->length = index + 1;
  } else if (ctx.has(Mod::POS)) {
    if (ctx.has(Mod::ALL)) {
      for (auto &voice : seq.voices) {
        voice.seek(index);
      }
    } else {
      seq.voice->seek(index);
    }
  } else if (ctx.has(Mod::ACCENT)) {
    create_undo_step();
    Step &step = seq.voice->pattern()->steps[index];
    step.vel = step.vel >= ACCENT_VELOCITY ? DEFAULT_VELOCITY : ACCENT_VELOCITY;
  } else if (ctx.has(Mod::ALL)) {
    for (auto &voice : seq.voices) {
      if (!voice.is_protected) {
        voice.pattern_idx = index;
      }
    }
  } else if (ctx.any_voice_held()) {
    for (uint32_t voice = 0; voice < VOICES; voice++) {
      if (ctx.held_mask & (1UL << voice_index_to_key(voice))) {
        seq.voices[voice].pattern_idx = index;
      }
    }
  } else {
    create_undo_step();
    if (seq.voice->pattern()->steps[index].vel == 0) {
      seq.voice->pattern()->steps[index].vel = DEFAULT_VELOCITY;
    } else {
      seq.voice->pattern()->steps[index].vel = 0;
    }
  }
}

void SequencerMode::handle_voice(const KeyContext &ctx) {
  uint32_t voice = ctx.voice_index();
  select_voice(voice);
}

void SequencerMode::on_key(const KeyContext &ctx) {
  if (ctx.key == KEY_FN) {
    if (ctx.pressed) {
      if (ctx.has(Mod::POS)) {
        rewind_transport();
      }
      if (ctx.has(Mod::ALL)) {
        mode_manager.push_mode(&fn_all_mode);
      } else {
        mode_manager.push_mode(&fn_mode);
      }
    }
    return;
  }

  if (!ctx.pressed) {
    handle_release(ctx);
    return;
  }

  // 1. Try declarative bindings table
  if (dispatch_binding(SEQUENCER_BINDINGS, BINDING_COUNT, ctx)) {
    return;
  }

  // 2. Step keys
  if (ctx.is_step()) {
    handle_step(ctx);
    return;
  }

  // 3. Voice keys
  if (ctx.is_voice()) {
    handle_voice(ctx);
    return;
  }
}

void SequencerMode::render_leds() { render_pixels(); }
