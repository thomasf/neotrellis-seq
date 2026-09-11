#ifndef FN_MODE_H
#define FN_MODE_H

#include "UIMode.h"

// Fn1Mode (FnMode): momentary mode active while holding the FN1 key.
// Handles single-voice transforms, pattern preset loading, pattern swaps, redo, and paste all.
class Fn1Mode : public UIMode {
public:
  void on_enter() override;
  void on_exit() override;
  void on_key(const KeyContext &ctx) override;
  void render_leds() override;

private:
  void handle_step(const KeyContext &ctx);
  void handle_voice(const KeyContext &ctx);
};

// Fn1AllMode (FnAllMode): momentary mode active while holding both FN1 and ALL keys.
// Handles all-voice transforms, interlocking fill, declutter, life, sync lengths,
// kit presets, polymeter, and dropout.
class Fn1AllMode : public UIMode {
public:
  void on_enter() override;
  void on_exit() override;
  void on_key(const KeyContext &ctx) override;
  void render_leds() override;

private:
  void handle_step(const KeyContext &ctx);
  void handle_voice(const KeyContext &ctx);
};

// Fn2Mode: momentary mode active while holding the FN2 key.
// Handles pattern length adjustments (FN2 + POS + STEP n).
class Fn2Mode : public UIMode {
public:
  void on_enter() override;
  void on_exit() override;
  void on_key(const KeyContext &ctx) override;
  void render_leds() override;

private:
  void handle_step(const KeyContext &ctx);
  void handle_voice(const KeyContext &ctx);
};

// Fn2AllMode: momentary mode active while holding both FN2 and ALL keys.
// Handles all-voice pattern length adjustments (FN2 + ALL + POS + STEP n).
class Fn2AllMode : public UIMode {
public:
  void on_enter() override;
  void on_exit() override;
  void on_key(const KeyContext &ctx) override;
  void render_leds() override;

private:
  void handle_step(const KeyContext &ctx);
  void handle_voice(const KeyContext &ctx);
};

using FnMode = Fn1Mode;
using FnAllMode = Fn1AllMode;

extern Fn1Mode fn1_mode;
extern Fn1Mode &fn_mode;
extern Fn1AllMode fn1_all_mode;
extern Fn1AllMode &fn_all_mode;
extern Fn2Mode fn2_mode;
extern Fn2AllMode fn2_all_mode;

#endif // FN_MODE_H
