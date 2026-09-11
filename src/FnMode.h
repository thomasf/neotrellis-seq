#ifndef FN_MODE_H
#define FN_MODE_H

#include "UIMode.h"

// FnMode: momentary mode active while holding the FN1 key.
// Handles single-voice transforms, pattern preset loading, pattern swaps, redo, and paste all.
class FnMode : public UIMode {
public:
  void on_enter() override;
  void on_exit() override;
  void on_key(const KeyContext &ctx) override;
  void render_leds() override;

private:
  void handle_step(const KeyContext &ctx);
  void handle_voice(const KeyContext &ctx);
};

// FnAllMode: momentary mode active while holding both FN1 and ALL keys.
// Handles all-voice transforms, interlocking fill, declutter, life, sync lengths,
// kit presets, polymeter, and dropout.
class FnAllMode : public UIMode {
public:
  void on_enter() override;
  void on_exit() override;
  void on_key(const KeyContext &ctx) override;
  void render_leds() override;

private:
  void handle_step(const KeyContext &ctx);
  void handle_voice(const KeyContext &ctx);
};

extern FnMode fn_mode;
extern FnAllMode fn_all_mode;

#endif // FN_MODE_H
