#!/bin/bash

# Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

# Set up some paths and re-direct the arguments to webrtc_tests.py

# This script is a copy of the chrome_tests.sh wrapper script with the following
# changes:
# - The locate_valgrind.sh of Chromium's Valgrind scripts dir is used to locate
#   the Valgrind framework install.
# - webrtc_tests.py is invoked instead of chrome_tests.py.
# - Chromium's Valgrind scripts directory is added to the PYTHONPATH to make it 
#   possible to execute the Python scripts properly.

export THISDIR=`dirname $0`
ARGV_COPY="$@"

# We need to set CHROME_VALGRIND iff using Memcheck or TSan-Valgrind:
#   tools/valgrind/chrome_tests.sh --tool memcheck
# or
#   tools/valgrind/chrome_tests.sh --tool=memcheck
# (same for "--tool=tsan")
tool="memcheck"  # Default to memcheck.
while (( "$#" ))
do
  if [[ "$1" == "--tool" ]]
  then
    tool="$2"
    shift
  elif [[ "$1" =~ --tool=(.*) ]]
  then
    tool="${BASH_REMATCH[1]}"
  fi
  shift
done

NEEDS_VALGRIND=0
NEEDS_DRMEMORY=0

case "$tool" in
  "memcheck")
    NEEDS_VALGRIND=1
    ;;
  "tsan" | "tsan_rv")
    NEEDS_VALGRIND=1
    ;;
  "drmemory" | "drmemory_light" | "drmemory_full")
    NEEDS_DRMEMORY=1
    ;;
esac

# For WebRTC, we'll use the locate_valgrind.sh script in Chromium's Valgrind 
# scripts dir to locate the Valgrind framework install
CHROME_VALGRIND_SCRIPTS=$THISDIR/../valgrind

if [ "$NEEDS_VALGRIND" == "1" ]
then
  CHROME_VALGRIND=`sh $CHROME_VALGRIND_SCRIPTS/locate_valgrind.sh`
  if [ "$CHROME_VALGRIND" = "" ]
  then
    # locate_valgrind.sh failed
    exit 1
  fi
  echo "Using valgrind binaries from ${CHROME_VALGRIND}"

  PATH="${CHROME_VALGRIND}/bin:$PATH"
  # We need to set these variables to override default lib paths hard-coded into
  # Valgrind binary.
  export VALGRIND_LIB="$CHROME_VALGRIND/lib/valgrind"
  export VALGRIND_LIB_INNER="$CHROME_VALGRIND/lib/valgrind"
fi

if [ "$NEEDS_DRMEMORY" == "1" ]
then
  if [ -z "$DRMEMORY_COMMAND" ]
  then
    DRMEMORY_PATH="$THISDIR/../../third_party/drmemory"
    DRMEMORY_SFX="$DRMEMORY_PATH/drmemory-windows-sfx.exe"
    if [ ! -f "$DRMEMORY_SFX" ]
    then
      echo "Can't find Dr. Memory executables."
      echo "See http://www.chromium.org/developers/how-tos/using-valgrind/dr-memory"
      echo "for the instructions on how to get them."
      exit 1
    fi

    chmod +x "$DRMEMORY_SFX"  # Cygwin won't run it without +x.
    "$DRMEMORY_SFX" -o"$DRMEMORY_PATH/unpacked" -y
    export DRMEMORY_COMMAND="$DRMEMORY_PATH/unpacked/bin/drmemory.exe"
  fi
fi

# Add Chrome's Valgrind scripts dir to the PYTHON_PATH since it contains 
# the scripts that are needed for this script to run
PYTHONPATH=$THISDIR/../python/google:$CHROME_VALGRIND_SCRIPTS python \
           "$THISDIR/webrtc_tests.py" $ARGV_COPY
