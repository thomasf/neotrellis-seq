#ifndef SEQUENCER_MODE_H
#define SEQUENCER_MODE_H

#include "UIMode.h"

class SequencerMode : public UIMode {
public:
  void on_enter() override;
  void on_exit() override;
  void on_key(const KeyContext &ctx) override;
  void render_leds() override;

private:
  void handle_release(const KeyContext &ctx);
  void handle_step(const KeyContext &ctx);
  void handle_voice(const KeyContext &ctx);

  bool mid1_modified_ = false;
  bool mid2_modified_ = false;
  uint32_t mid1_press_time_ = 0;
  uint32_t mid2_press_time_ = 0;
};

extern SequencerMode sequencer_mode;

#endif // SEQUENCER_MODE_H
