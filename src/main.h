#ifndef MAIN_H
#define MAIN_H

#include <Arduino.h>
#include <MIDIUSB.h>
#include <cstdint>

#include "Sequencer.h"

// -----------------------------------------------------------------------------
// Types
// -----------------------------------------------------------------------------

// Transform applies the action for step key `index` to a voice's current
// pattern and reports whether that key is assigned.
typedef bool (*Transform)(uint32_t voice, uint32_t index);

// -----------------------------------------------------------------------------
// Display & Pixels
// -----------------------------------------------------------------------------

void set_pixel(uint32_t key, uint32_t color);
void fill_pixels(uint32_t color);
void show_pixels();
void render_pixels();
void select_voice(uint32_t idx);

// -----------------------------------------------------------------------------
// Setup & Initialization
// -----------------------------------------------------------------------------

void setup_default_patterns();
void init_timer();
uint32_t random_below(uint32_t n);
void seed_random();

// -----------------------------------------------------------------------------
// Undo / Redo
// -----------------------------------------------------------------------------

void begin_edit();
void record_before(uint32_t voice, uint32_t pattern_idx, const Pattern &before);
void record_undo(uint32_t voice);
void create_undo_step();
void move_group(UndoBuffer &from, UndoBuffer &to);
void undo();
void redo();

// -----------------------------------------------------------------------------
// Pattern & Transport Actions
// -----------------------------------------------------------------------------

void copy_pattern();
void paste_single();
void paste_all_slots();
void clear_pattern();
void clear_accents();
void rewind_transport();
void open_menu();

// -----------------------------------------------------------------------------
// Global State Externs
// -----------------------------------------------------------------------------

extern Sequencer seq;
extern Pattern copy_buffer;
extern uint32_t seq_color_set;
extern uint32_t seq_color_bg;
extern uint32_t seq_color_accent;
extern bool is_voice_select_hl_period;
extern uint32_t ppqn;

// -----------------------------------------------------------------------------
// Transforms
// -----------------------------------------------------------------------------

bool apply_transform(uint32_t voice, uint32_t index);
bool apply_accent_transform(uint32_t voice, uint32_t index);
void transform_pattern(Transform transform, uint32_t index);
void transform_all_patterns(Transform transform, uint32_t index);
bool transform_board(uint32_t index);
void dropout_patterns();
void polymeter_patterns();
void swap_pattern(uint32_t other);
void toggle_note_offset(uint32_t voice);

// -----------------------------------------------------------------------------
// MIDI & Clock Engine
// -----------------------------------------------------------------------------

void midi_flush();
void midi_queue(uint8_t cin, uint8_t status, uint8_t data1, uint8_t data2);
void midi_note_on(uint8_t note, uint8_t velocity);
void midi_note_off(uint8_t note, uint8_t velocity);
void notes_off();
void run_step();
void locate(uint32_t clocks);
void on_midi_clock();
void on_midi_start();
void on_midi_continue();
void on_midi_stop();
void on_midi_song_position(uint32_t beats);
void on_midi_reset();
void handle_midi_in(midiEventPacket_t const &event);
void service_clock();

// -----------------------------------------------------------------------------
// Key Handling & Arduino Lifecycle
// -----------------------------------------------------------------------------

void handle_keys();
void setup();
void loop();

#endif // MAIN_H
