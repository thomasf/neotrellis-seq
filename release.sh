#!/bin/sh

set -e

platformio run

go run contrib/uf2conv.go \
  -c \
  -f 0x55114460 \
  -b 0x4000 \
  -o neotrellis-seq.uf2 \
  .pio/build/default/firmware.bin

