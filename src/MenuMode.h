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
  void set_page(uint32_t page) { current_page_ = (page < VOICES) ? page : 0; }

private:
  uint32_t current_page_ = 0;
};

extern MenuMode menu_mode;

#endif // MENU_MODE_H
