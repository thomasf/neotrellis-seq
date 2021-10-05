#!/bin/sh

set -e

platformio run

./contrib/uf2conv.py \
  -c \
  .pio/build/default/firmware.bin \
  -f 0x55114460 \
  -o neotrellis-seq.uf2 \
  -b 0x4000
