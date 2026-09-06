#!/bin/sh

set -e

exec platformio test -e native "$@"
