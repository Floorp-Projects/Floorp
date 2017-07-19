#!/bin/bash -vex

set -x -e

: WORKSPACE ${WORKSPACE:=/workspace}

set -v

# Populate /home/worker/.mozbuild/android-sdk-linux.
python2.7 $WORKSPACE/build/src/python/mozboot/mozboot/android.py --artifact-mode --no-interactive
