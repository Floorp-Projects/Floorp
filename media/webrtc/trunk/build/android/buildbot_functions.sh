#!/bin/bash
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Bash functions used by buildbot annotator scripts for the android
# build of chromium.  Executing this script should not perform actions
# other than setting variables and defining of functions.

# Number of jobs on the compile line; e.g.  make -j"${JOBS}"
JOBS="${JOBS:-4}"

# Clobber build?  Overridden by bots with BUILDBOT_CLOBBER.
NEED_CLOBBER="${NEED_CLOBBER:-0}"

# Function to force-green a bot.
function bb_force_bot_green_and_exit {
  echo "@@@BUILD_STEP Bot forced green.@@@"
  exit 0
}

# Basic setup for all bots to run after a source tree checkout.
# $1: source root.
function bb_baseline_setup {
  echo "@@@BUILD_STEP cd into source root@@@"
  SRC_ROOT="$1"
  if [ ! -d "${SRC_ROOT}" ] ; then
    echo "Please specify a valid source root directory as an arg"
    echo '@@@STEP_FAILURE@@@'
    return 1
  fi
  cd $SRC_ROOT

  if [ ! -f build/android/envsetup.sh ] ; then
    echo "No envsetup.sh"
    echo "@@@STEP_FAILURE@@@"
    return 1
  fi

  echo "@@@BUILD_STEP Basic setup@@@"
  export ANDROID_SDK_ROOT=/usr/local/google/android-sdk-linux
  export ANDROID_NDK_ROOT=/usr/local/google/android-ndk-r7
  for mandatory_directory in "${ANDROID_SDK_ROOT}" "${ANDROID_NDK_ROOT}" ; do
    if [[ ! -d "${mandatory_directory}" ]]; then
      echo "Directory ${mandatory_directory} does not exist."
      echo "Build cannot continue."
      echo "@@@STEP_FAILURE@@@"
      return 1
    fi
  done

  if [ ! "$BUILDBOT_CLOBBER" = "" ]; then
    NEED_CLOBBER=1
  fi

  echo "@@@BUILD_STEP Configure with envsetup.sh@@@"
  . build/android/envsetup.sh

  if [ "$NEED_CLOBBER" -eq 1 ]; then
    echo "@@@BUILD_STEP Clobber@@@"
    rm -rf "${SRC_ROOT}"/out
    if [ -e "${SRC_ROOT}"/out ] ; then
      echo "Clobber appeared to fail?  ${SRC_ROOT}/out still exists."
      echo "@@@STEP_WARNINGS@@@"
    fi
  fi

  echo "@@@BUILD_STEP android_gyp@@@"
  android_gyp
}


# Compile step
function bb_compile {
  echo "@@@BUILD_STEP Compile@@@"
  make -j${JOBS}
}

# Experimental compile step; does not turn the tree red if it fails.
function bb_compile_experimental {
  # Linking DumpRenderTree appears to hang forever?
  # EXPERIMENTAL_TARGETS="DumpRenderTree webkit_unit_tests"
  EXPERIMENTAL_TARGETS="webkit_unit_tests"
  for target in ${EXPERIMENTAL_TARGETS} ; do
    echo "@@@BUILD_STEP Experimental Compile $target @@@"
    set +e
    make -j4 "${target}"
    if [ $? -ne 0 ] ; then
      echo "@@@STEP_WARNINGS@@@"
    fi
    set -e
  done
}

# Run tests on an emulator.
function bb_run_tests_emulator {
  echo "@@@BUILD_STEP Run Tests on an Emulator@@@"
  build/android/run_tests.py -e --xvfb --verbose
}

# Run tests on an actual device.  (Better have one plugged in!)
function bb_run_tests {
  echo "@@@BUILD_STEP Run Tests on actual hardware@@@"
  build/android/run_tests.py --xvfb --verbose
}
