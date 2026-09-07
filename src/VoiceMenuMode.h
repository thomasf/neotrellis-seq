#ifndef VOICE_MENU_MODE_H
#define VOICE_MENU_MODE_H

#include "UIMode.h"
#include <cstdint>

class VoiceMenuMode : public UIMode {
public:
  void on_enter() override;
  void on_exit() override;
  void on_key(const KeyContext &ctx) override;
  void render_leds() override;

  uint32_t active_voice() const { return voice_; }
  void set_voice(uint32_t voice);

private:
  uint32_t voice_ = 0;
};

extern VoiceMenuMode voice_menu_mode;

#endif // VOICE_MENU_MODE_H
