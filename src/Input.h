#ifndef INPUT_H
#define INPUT_H

#include "config.h"
#include "constants.h"
#include "utils.h"
#include <Arduino.h>
#include <cstddef>
#include <cstdint>

namespace Mod {
static constexpr uint32_t NONE = 0;
static constexpr uint32_t TRANSFORM = (1UL << KEY_TRANSFORM);
static constexpr uint32_t ALL = (1UL << KEY_VOICE_SELECT_ALL);
static constexpr uint32_t ACCENT = (1UL << KEY_ACCENT);
static constexpr uint32_t LEN = (1UL << KEY_PATTERN_LEN);
static constexpr uint32_t POS = (1UL << KEY_PATTERN_POS);
static constexpr uint32_t MENU = (1UL << KEY_MENU);

static constexpr uint32_t VOICE_0 = (1UL << KEY_VOICE_SELECT_0);
static constexpr uint32_t VOICE_1 = (1UL << KEY_VOICE_SELECT_1);
static constexpr uint32_t VOICE_2 = (1UL << KEY_VOICE_SELECT_2);
static constexpr uint32_t VOICE_3 = (1UL << KEY_VOICE_SELECT_3);
static constexpr uint32_t VOICE_4 = (1UL << KEY_VOICE_SELECT_4);
static constexpr uint32_t VOICE_5 = (1UL << KEY_VOICE_SELECT_5);

static constexpr uint32_t ALL_VOICES =
    VOICE_0 | VOICE_1 | VOICE_2 | VOICE_3 | VOICE_4 | VOICE_5;
} // namespace Mod

struct KeyContext {
  uint8_t key;        // Key index (0..31)
  bool pressed;       // true for press, false for release
  uint32_t held_mask; // Mask of *other* keys held when this event occurred

  bool has(uint32_t mods) const { return (held_mask & mods) == mods; }
  bool only(uint32_t mods) const { return held_mask == mods; }
  bool any_voice_held() const { return (held_mask & Mod::ALL_VOICES) != 0; }

  bool is_step() const { return is_numpad_key(key); }
  uint32_t step_index() const { return index_of(step_key, 16, key); }

  bool is_voice() const { return voice_key_to_index(key) < VOICES; }
  uint32_t voice_index() const { return voice_key_to_index(key); }

  uint32_t held_voice_index() const {
    for (uint32_t v = 0; v < VOICES; v++) {
      if (held_mask & (1UL << voice_index_to_key(v))) {
        return v;
      }
    }
    return VOICES;
  }
};

using ActionHandler = void (*)();

struct KeyBinding {
  uint8_t trigger_key;     // Key that triggers this action
  uint32_t required_mods;  // Modifiers that MUST be held
  uint32_t forbidden_mods; // Modifiers that MUST NOT be held
  ActionHandler handler;   // Function called when matched
};

inline bool dispatch_binding(const KeyBinding *table, size_t count,
                             const KeyContext &ctx) {
  if (!ctx.pressed)
    return false;

  for (size_t i = 0; i < count; ++i) {
    const auto &b = table[i];
    if (b.trigger_key == ctx.key &&
        (ctx.held_mask & b.required_mods) == b.required_mods &&
        (ctx.held_mask & b.forbidden_mods) == 0) {
      b.handler();
      return true;
    }
  }
  return false;
}

#endif // INPUT_H
