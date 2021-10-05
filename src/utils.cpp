#include "utils.h"

uint32_t index_of(const uint32_t a[], uint32_t size, uint32_t value) {
  uint32_t index = 0;

  while (index < size && a[index] != value)
    ++index;

  return (index == size ? -1 : index);
}
