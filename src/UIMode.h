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
  static constexpr size_t MAX_STACK = 8;
  UIMode *stack_[MAX_STACK]{};
  size_t stack_depth_ = 0;

public:
  void push_mode(UIMode *new_mode) {
    if (!new_mode || current_mode_ == new_mode)
      return;
    if (stack_depth_ < MAX_STACK && current_mode_) {
      stack_[stack_depth_++] = current_mode_;
    }
    if (current_mode_) {
      current_mode_->on_exit();
    }
    current_mode_ = new_mode;
    current_mode_->on_enter();
  }

  void pop_mode() {
    if (stack_depth_ > 0) {
      UIMode *target = stack_[--stack_depth_];
      if (current_mode_) {
        current_mode_->on_exit();
      }
      current_mode_ = target;
      if (current_mode_) {
        current_mode_->on_enter();
      }
      return;
    }
  }

  void switch_mode(UIMode *new_mode) {
    if (current_mode_ == new_mode)
      return;
    stack_depth_ = 0;
    if (current_mode_) {
      current_mode_->on_exit();
    }
    current_mode_ = new_mode;
    if (current_mode_) {
      current_mode_->on_enter();
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
