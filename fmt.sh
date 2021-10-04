#!/bin/sh

set -e

gofmt -w .
clang-format -i src/*.cpp src/*.h
