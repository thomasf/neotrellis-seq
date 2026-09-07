#ifndef MENU_MODE_H
#define MENU_MODE_H

#include "UIMode.h"

class MenuMode : public UIMode {
public:
  void on_enter() override;
  void on_exit() override;
  void on_key(const KeyContext &ctx) override;
  void render_leds() override;

  uint32_t active_page() const { return current_page_; }
  void set_page(uint32_t page);

private:
  uint32_t current_page_ = 0;
  static constexpr uint32_t VOICE_HOLD_THRESHOLD_MS = 200;
  uint32_t voice_press_time_ = 0;
  uint32_t pending_voice_ = VOICES;
};

extern MenuMode menu_mode;

#endif // MENU_MODE_H
