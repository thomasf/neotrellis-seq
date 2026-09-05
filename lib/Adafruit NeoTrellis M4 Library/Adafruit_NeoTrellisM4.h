/*!
 * @file Adafruit_NeoTrellisM4.h
 */

#ifndef _ADAFRUIT_NEOTRELLIS_M4_H_
#define _ADAFRUIT_NEOTRELLIS_M4_H_

#include "MIDIUSB.h"
#include <Adafruit_Keypad.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_NeoPixel_ZeroDMA.h>
#include <Arduino.h>

#ifndef ADAFRUIT_TRELLIS_M4_EXPRESS
#error "This library is only for the Adafruit NeoTrellis M4!!"
#endif

/*!    @brief  NeoTrellis M4 helper library that handles DMA NeoPixel, Keypad
 * scanning, and some basic MIDI messages */
class Adafruit_NeoTrellisM4 : public Adafruit_NeoPixel_ZeroDMA {

public:
  Adafruit_NeoTrellisM4();
  void begin(void);
  void tick(void);

  int available(void);
  keypadEvent read(void);
  bool isPressed(uint8_t key) const;
  void clear(void);

  void autoUpdateNeoPixels(boolean flag);
  void setPixelColor(uint32_t pixel, uint32_t color);
  void fill(uint32_t color);

  /*!    @brief  Getter for how many keys are available on this NeoTrellis
         @returns number of keys  */
  uint8_t num_keys(void) { return _num_keys; }

  void enableUSBMIDI(boolean f);
  void enableUARTMIDI(boolean f);
  void setUARTMIDIchannel(uint8_t c);
  void setUSBMIDIchannel(uint8_t c);
  void noteOn(byte pitch, byte velocity);
  void noteOff(byte pitch, byte velocity);
  void pitchBend(int value);
  void controlChange(byte control, byte value);
  void sendMIDI(void);
  void programChange(byte channel, byte program);

private:
  int _num_keys, _rows, _cols;
  boolean _pending_midi, _auto_update, _midi_usb, _midi_uart;
  int _midi_channel_usb, _midi_channel_uart;

  uint32_t _keys_current = 0;
  uint32_t _keys_previous = 0;

  static const uint8_t EVENT_BUF_SIZE = 32;
  keypadEvent _event_buf[EVENT_BUF_SIZE];
  uint8_t _event_head = 0;
  uint8_t _event_tail = 0;
};

#endif
