#ifndef UI_MODE_H
#define UI_MODE_H

#include "Input.h"

class UIMode {
public:
  virtual ~UIMode() = default;
  virtual void on_enter() {}
  virtual void on_exit() {}
  virtual void on_key(const KeyContext &ctx) = 0;
  virtual void render_leds() = 0;
};

class ModeManager {
private:
  UIMode *current_mode_ = nullptr;
  UIMode *previous_mode_ = nullptr;

public:
  void switch_mode(UIMode *new_mode) {
    if (current_mode_ == new_mode)
      return;
    if (current_mode_) {
      current_mode_->on_exit();
    }
    previous_mode_ = current_mode_;
    current_mode_ = new_mode;
    if (current_mode_) {
      current_mode_->on_enter();
    }
  }

  void pop_mode() {
    if (previous_mode_) {
      UIMode *target = previous_mode_;
      previous_mode_ = nullptr;
      switch_mode(target);
    }
  }

  UIMode *current_mode() const { return current_mode_; }

  void handle_key(const KeyContext &ctx) {
    if (current_mode_) {
      current_mode_->on_key(ctx);
    }
  }

  void render() {
    if (current_mode_) {
      current_mode_->render_leds();
    }
  }
};

extern ModeManager mode_manager;

#endif // UI_MODE_H
