#ifndef UTILS_H
#define UTILS_H

#ifdef ARDUINO
#include <Arduino.h>
#endif
#include <cstdint>

constexpr uint32_t INDEX_NOT_FOUND = UINT32_MAX;

uint32_t index_of(const uint32_t a[], uint32_t size, uint32_t value);

#ifdef DEBUG
#define debug_print(msg, var)                                                                      \
  do {                                                                                             \
    Serial.print(msg);                                                                             \
    Serial.print(": ");                                                                            \
    Serial.println(var);                                                                           \
  } while (0)
#else
#define debug_print(msg, var)                                                                      \
  do {                                                                                             \
  } while (0)
#endif

#endif // UTILS_H
