#include "VoiceMenuMode.h"
#include "MenuMode.h"
#include "SequencerMode.h"
#include "colors.h"
#include "config.h"
#include "constants.h"
#include "main.h"

VoiceMenuMode voice_menu_mode;

void VoiceMenuMode::on_enter() {
  for (uint32_t s = 0; s < 16; s++) {
    set_pixel(step_key[s], COLOR_OFF);
  }
}

void VoiceMenuMode::set_voice(uint32_t voice) {
  voice_ = (voice < VOICES) ? voice : 0;
  select_voice(voice_);
}

void VoiceMenuMode::on_exit() {
  select_voice(voice_);
  for (uint32_t s = 0; s < 16; s++) {
    set_pixel(step_key[s], COLOR_OFF);
  }
}

void VoiceMenuMode::on_key(const KeyContext &ctx) {
  // 1. Pressing MENU exits menu mode completely back to sequencer
  if (ctx.pressed && ctx.key == KEY_MENU) {
    mode_manager.switch_mode(&sequencer_mode);
    return;
  }

  // 2. If the held voice key is released, pop back to MenuMode
  if (!ctx.pressed && ctx.is_voice() && ctx.voice_index() == voice_) {
    mode_manager.pop_mode();
    return;
  }

  // 3. If another voice key is pressed, switch active editing voice
  if (ctx.pressed && ctx.is_voice()) {
    set_voice(ctx.voice_index());
    menu_mode.set_page(voice_);
    return;
  }

  // 4. Step keys: assign/swap MIDI note for this voice
  if (ctx.pressed && ctx.is_step()) {
    uint32_t const step_idx = ctx.step_index();
    uint8_t const new_note = step_to_drum_rack_note(step_idx);
    seq.set_or_swap_note(voice_, new_note);
    return;
  }
}

void VoiceMenuMode::render_leds() {
  // If no voice key is currently physically held, pop back to MenuMode
  if (get_held_voice() >= VOICES) {
    mode_manager.pop_mode();
    return;
  }

  // Keep MENU indicator active
  set_pixel(KEY_MENU, COLOR_PPOS);
  set_pixel(KEY_CLEAR, COLOR_OFF);

  // Voice pads (0..5) on the right:
  // Active voice button lights in white (COLOR_PPOS)
  for (uint32_t i = 0; i < VOICES; i++) {
    uint32_t key = voice_index_to_key(i);
    if (is_voice_flashing(i)) {
      set_pixel(key, COLOR_OFF);
    } else if (i == voice_) {
      set_pixel(key, COLOR_PPOS);
    } else {
      set_pixel(key, voice_index_to_color(i));
    }
  }

  // 16 drum rack pads on the 4x4 step grid:
  // Inverted rows matching standard 4x4 drum racks (Bottom row = 36..39, Top row = 48..51)
  for (uint32_t s = 0; s < 16; s++) {
    uint8_t const note = step_to_drum_rack_note(s);
    uint32_t owner_voice = VOICES;
    for (uint32_t v = 0; v < VOICES; v++) {
      if (seq.voices[v].midi_note == note) {
        owner_voice = v;
        break;
      }
    }

    if (owner_voice < VOICES) {
      if (is_voice_flashing(owner_voice)) {
        set_pixel(step_key[s], COLOR_OFF);
      } else if (owner_voice == voice_) {
        // The currently selected voice has the white voice selected color
        set_pixel(step_key[s], COLOR_PPOS);
      } else {
        set_pixel(step_key[s], voice_index_to_color(owner_voice));
      }
    } else {
      set_pixel(step_key[s], COLOR_OFF);
    }
  }
}
