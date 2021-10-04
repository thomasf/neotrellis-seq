#!/bin/sh

set -e

echo "build..."
go run gen/main.go
platformio run -s -e dev -t upload

echo "watch..."
sleep 2
platformio device monitor -b 115200 --raw --quiet