#!/bin/bash

set -e

# using https://github.com/watchexec/watchexec to monitor changes in source
# code to recompile and upload.
watchexec \
  -e .cpp,.ini,.go,.h \
  -r \
  ./run.sh
