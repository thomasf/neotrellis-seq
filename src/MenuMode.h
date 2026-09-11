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
  uint32_t voice_press_time_ = 0;
  uint32_t pending_voice_ = VOICES;

  bool all_pressed_ = false;
  bool all_stop_triggered_ = false;
  bool all_sleep_triggered_ = false;
  uint32_t all_press_time_ = 0;
};

extern MenuMode menu_mode;

#endif // MENU_MODE_H
