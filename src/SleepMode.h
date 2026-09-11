#ifndef SLEEP_MODE_H
#define SLEEP_MODE_H

#include "UIMode.h"

// SleepMode: locks all functionality, stops sequencer playback, turns off all
// LEDs except the ALL button which remains dimly lit until pressed again to wake up.
class SleepMode : public UIMode {
public:
  void on_enter() override;
  void on_exit() override;
  void on_key(const KeyContext &ctx) override;
  void render_leds() override;
};

extern SleepMode sleep_mode;

#endif // SLEEP_MODE_H
