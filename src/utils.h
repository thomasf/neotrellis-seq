#ifndef UTILS_H
#define UTILS_H
#include <algorithm>
#include <array>
#include <cstdint>
#include <delay.h>
#include <vector>

uint32_t index_of(const uint32_t a[], uint32_t size, uint32_t value);

template <typename T> int rotate_array_elements(T &v, int dir) {
  if (dir > 0) {
    std::rotate(v.rbegin(), v.rbegin() + dir, v.rend());
    return 0;
  } else if (dir < 0) {
    std::rotate(v.begin(), v.begin() + abs(dir), v.end());
    return 0;
  } else {
    return 1;
  }
}

#ifdef DEBUG
#define debug_print(msg, var)                                                  \
  Serial.print(msg);                                                           \
  Serial.print(": ");                                                          \
  Serial.println(var);
#else
#define debug_print(msg, var) ;
#endif

#endif
