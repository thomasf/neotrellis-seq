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
    {KEY_MID_3, Mod::ACCENT, 0, clear_accents},
    {KEY_MID_3, 0, 0, clear_pattern},

    // Modal Menu
    {KEY_MENU, 0, 0, open_menu},
};

static constexpr size_t BINDING_COUNT = sizeof(SEQUENCER_BINDINGS) / sizeof(SEQUENCER_BINDINGS[0]);

void SequencerMode::on_enter() {
  for (uint32_t i = 0; i < VOICES; i++) {
    set_pixel(voice_index_to_key(i), voice_index_to_color(i));
  }

  set_pixel(KEY_MID_6, COLOR_PMOD);
  set_pixel(KEY_FN1, COLOR_PMOD);
  set_pixel(KEY_FN2, COLOR_PMOD);
  set_pixel(KEY_MID_5, COLOR_PMOD);

  uint32_t const cur_page = seq.voice->current_page;
  uint32_t const total_pages = seq.voice->page_count();
  set_pixel(KEY_MID_1, cur_page > 0 ? COLOR_PMOD : COLOR_OFF);
  set_pixel(KEY_MID_2, cur_page + 1 < total_pages ? COLOR_PMOD : COLOR_OFF);
  set_pixel(KEY_MENU, COLOR_PACT);
  set_pixel(KEY_MID_3, COLOR_PACT);
  set_pixel(KEY_MID_4, COLOR_OFF);

  set_pixel(KEY_VOICE_ALL, COLOR_PPOS);
}

void SequencerMode::on_exit() {}

void SequencerMode::handle_release(const KeyContext &ctx) {
  if (ctx.is_voice()) {
    set_pixel(ctx.key, voice_key_to_color(ctx.key));
  }
}

void SequencerMode::handle_step(const KeyContext &ctx) {
  uint32_t const index = ctx.step_index();
  bool const is_mid1_held = (ctx.held_mask & (1UL << KEY_MID_1)) != 0;
  bool const is_mid2_held = (ctx.held_mask & (1UL << KEY_MID_2)) != 0;
  bool const page_set_mode = is_mid1_held && is_mid2_held;

  if (page_set_mode) {
    if (index < MAX_PAGES) {
      create_undo_step();
      uint32_t const new_len = (index + 1) * STEPS_PER_PAGE;
      if (ctx.has(Mod::ALL)) {
        for (auto &voice : seq.voices) {
          if (!voice.is_protected) {
            voice.pattern()->length = new_len;
            if (voice.current_page >= voice.page_count()) {
              voice.current_page = voice.page_count() - 1;
            }
          }
        }
      } else {
        seq.voice->pattern()->length = new_len;
        if (seq.voice->current_page >= seq.voice->page_count()) {
          seq.voice->current_page = seq.voice->page_count() - 1;
        }
      }
      trigger_page_flash(index);
    }
    return;
  }

  uint32_t const actual_step = seq.voice->current_page * STEPS_PER_PAGE + index;

  if (ctx.has(Mod::LEN)) {
    create_undo_step();
    seq.voice->pattern()->length = actual_step + 1;
    if (seq.voice->current_page >= seq.voice->page_count()) {
      seq.voice->current_page = seq.voice->page_count() - 1;
    }
  } else if (ctx.has(Mod::POS)) {
    if (ctx.has(Mod::ALL)) {
      for (auto &voice : seq.voices) {
        voice.seek(actual_step);
      }
    } else {
      seq.voice->seek(actual_step);
    }
  } else if (ctx.has(Mod::ACCENT)) {
    create_undo_step();
    Step &step = seq.voice->pattern()->steps[actual_step];
    step.vel = step.vel >= ACCENT_VELOCITY ? DEFAULT_VELOCITY : ACCENT_VELOCITY;
  } else if (ctx.has(Mod::ALL)) {
    for (auto &voice : seq.voices) {
      if (!voice.is_protected) {
        voice.pattern_idx = index;
        if (voice.current_page >= voice.page_count()) {
          voice.current_page = voice.page_count() - 1;
        }
      }
    }
    trigger_step_flash(seq.voice->current_page, COLOR_RED);
  } else if (ctx.any_voice_held()) {
    for (uint32_t voice = 0; voice < VOICES; voice++) {
      if (ctx.held_mask & (1UL << voice_index_to_key(voice))) {
        seq.voices[voice].pattern_idx = index;
        if (seq.voices[voice].current_page >= seq.voices[voice].page_count()) {
          seq.voices[voice].current_page = seq.voices[voice].page_count() - 1;
        }
      }
    }
    trigger_step_flash(seq.voice->current_page, COLOR_RED);
  } else {
    create_undo_step();
    if (seq.voice->pattern()->steps[actual_step].vel == 0) {
      seq.voice->pattern()->steps[actual_step].vel = DEFAULT_VELOCITY;
    } else {
      seq.voice->pattern()->steps[actual_step].vel = 0;
    }
  }
}

void SequencerMode::handle_voice(const KeyContext &ctx) {
  uint32_t voice = ctx.voice_index();
  select_voice(voice);
  trigger_step_flash(seq.voice->current_page, COLOR_RED);
}

void SequencerMode::on_key(const KeyContext &ctx) {
  if (ctx.key == KEY_FN1) {
    if (ctx.pressed) {
      if (ctx.has(Mod::POS)) {
        rewind_transport();
      }
      if (ctx.has(Mod::FN2)) {
        mode_manager.push_mode(&fn1_fn2_mode);
      } else if (ctx.has(Mod::ALL)) {
        mode_manager.push_mode(&fn1_all_mode);
      } else {
        mode_manager.push_mode(&fn1_mode);
      }
    }
    return;
  }

  if (ctx.key == KEY_FN2) {
    if (ctx.pressed) {
      if (ctx.has(Mod::FN1)) {
        mode_manager.push_mode(&fn1_fn2_mode);
      } else if (ctx.has(Mod::ALL)) {
        mode_manager.push_mode(&fn2_all_mode);
      } else {
        mode_manager.push_mode(&fn2_mode);
      }
    }
    return;
  }

  if (ctx.key == KEY_MID_1) {
    if (ctx.pressed) {
      if (ctx.has(1UL << KEY_MID_2)) {
        return; // Both MID_1 and MID_2 held: entering page-length set mode
      }
      if (seq.voice->current_page > 0) {
        seq.voice->current_page--;
      }
      trigger_page_flash(seq.voice->current_page);
    }
    return;
  }

  if (ctx.key == KEY_MID_2) {
    if (ctx.pressed) {
      if (ctx.has(1UL << KEY_MID_1)) {
        return; // Both MID_1 and MID_2 held: entering page-length set mode
      }
      uint32_t const max_pages = seq.voice->page_count();
      if (seq.voice->current_page + 1 < max_pages) {
        seq.voice->current_page++;
      }
      trigger_page_flash(seq.voice->current_page);
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

void SequencerMode::render_leds() {
  render_pixels();

  bool const is_mid1_held = (held_keys_mask & (1UL << KEY_MID_1)) != 0;
  bool const is_mid2_held = (held_keys_mask & (1UL << KEY_MID_2)) != 0;
  bool const page_set_mode = is_mid1_held && is_mid2_held;

  if (page_set_mode) {
    set_pixel(KEY_MID_1, COLOR_PPOS);
    set_pixel(KEY_MID_2, COLOR_PPOS);

    uint32_t const cur_pages = seq.voice->page_count();
    for (uint32_t p = 0; p < MAX_PAGES; p++) {
      set_pixel(step_key[p], (p < cur_pages) ? COLOR_TOOL : COLOR_OFF);
    }
    for (uint32_t s = MAX_PAGES; s < 16; s++) {
      set_pixel(step_key[s], COLOR_OFF);
    }
  } else {
    uint32_t const cur_page = seq.voice->current_page;
    uint32_t const total_pages = seq.voice->page_count();

    if (cur_page > 0) {
      set_pixel(KEY_MID_1, is_mid1_held ? COLOR_PPOS : COLOR_PMOD);
    } else {
      set_pixel(KEY_MID_1, is_mid1_held ? COLOR_PPOS : COLOR_OFF);
    }

    if (cur_page + 1 < total_pages) {
      set_pixel(KEY_MID_2, is_mid2_held ? COLOR_PPOS : COLOR_PMOD);
    } else {
      set_pixel(KEY_MID_2, is_mid2_held ? COLOR_PPOS : COLOR_OFF);
    }
  }

  if (is_step_flashing() && get_step_flash_step() < 16) {
    set_pixel(step_key[get_step_flash_step()], get_step_flash_color());
  }
}
