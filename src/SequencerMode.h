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
};

extern SequencerMode sequencer_mode;

#endif // SEQUENCER_MODE_H
