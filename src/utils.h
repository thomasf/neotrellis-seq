#ifndef UTILS_H
#define UTILS_H
#include <algorithm>
#include <array>
#include <cstdint>
#include <delay.h>
#include <vector>

uint32_t index_of(const uint32_t a[], uint32_t size, uint32_t value);

#ifdef DEBUG
#define debug_print(msg, var)                                                  \
  Serial.print(msg);                                                           \
  Serial.print(": ");                                                          \
  Serial.println(var);
#else
#define debug_print(msg, var) ;
#endif

#endif
