#!/bin/sh

set -e

echo "build..."
go run ./cmd/neotrellis-seq-generator
platformio run -s -e dev -t upload

echo "watch..."
sleep 2
exec platformio device monitor -b 115200 --raw --quiet
