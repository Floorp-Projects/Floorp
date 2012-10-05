#!/bin/bash

# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Sets up environment for building Chromium on Android.  It can either be
# compiled with the Android tree or using the Android SDK/NDK. To build with
# NDK/SDK: ". build/android/envsetup.sh --sdk".  Environment variable
# ANDROID_SDK_BUILD=1 will then be defined and used in the rest of the setup to
# specifiy build type.

# NOTE(yfriedman): This looks unnecessary but downstream the default value
# should be 0 until all builds switch to SDK/NDK.
export ANDROID_SDK_BUILD=1
# Loop over args in case we add more arguments in the future.
while [ "$1" != "" ]; do
  case $1 in
    -s | --sdk  ) export ANDROID_SDK_BUILD=1 ; shift ;;
    *  )          shift ; break ;;
  esac
done

if [[ "${ANDROID_SDK_BUILD}" -eq 1 ]]; then
  echo "Using SDK build"
fi

host_os=$(uname -s | sed -e 's/Linux/linux/;s/Darwin/mac/')

case "${host_os}" in
  "linux")
    toolchain_dir="linux-x86_64"
    ;;
  "mac")
    toolchain_dir="darwin-x86"
    ;;
  *)
    echo "Host platform ${host_os} is not supported" >& 2
    return 1
esac

CURRENT_DIR="$(readlink -f "$(dirname $BASH_SOURCE)/../../")"
if [ -z "${CHROME_SRC}" ]; then
  # If $CHROME_SRC was not set, assume current directory is CHROME_SRC.
  export CHROME_SRC="${CURRENT_DIR}"
fi

if [ "${CURRENT_DIR/"${CHROME_SRC}"/}" == "${CURRENT_DIR}" ]; then
  # If current directory is not in $CHROME_SRC, it might be set for other
  # source tree. If $CHROME_SRC was set correctly and we are in the correct
  # directory, "${CURRENT_DIR/"${CHROME_SRC}"/}" will be "".
  # Otherwise, it will equal to "${CURRENT_DIR}"
  echo "Warning: Current directory is out of CHROME_SRC, it may not be \
the one you want."
  echo "${CHROME_SRC}"
fi

# Android sdk platform version to use
export ANDROID_SDK_VERSION=16

# Source functions script.  The file is in the same directory as this script.
. "$(dirname $BASH_SOURCE)"/envsetup_functions.sh

if [ "${ANDROID_SDK_BUILD}" -eq 1 ]; then
  sdk_build_init
# Sets up environment for building Chromium for Android with source. Expects
# android environment setup and lunch.
elif [ -z "$ANDROID_BUILD_TOP" -o -z "$ANDROID_TOOLCHAIN" -o \
  -z "$ANDROID_PRODUCT_OUT" ]; then
  echo "Android build environment variables must be set."
  echo "Please cd to the root of your Android tree and do: "
  echo "  . build/envsetup.sh"
  echo "  lunch"
  echo "Then try this again."
  echo "Or did you mean NDK/SDK build. Run envsetup.sh with --sdk argument."
  return 1
else
  non_sdk_build_init
fi

# Workaround for valgrind build
if [ -n "$CHROME_ANDROID_VALGRIND_BUILD" ]; then
# arm_thumb=0 is a workaround for https://bugs.kde.org/show_bug.cgi?id=270709
  DEFINES+=" arm_thumb=0 release_extra_cflags='-fno-inline\
 -fno-omit-frame-pointer -fno-builtin' release_valgrind_build=1\
 release_optimize=1"
fi

# Source a bunch of helper functions
. ${CHROME_SRC}/build/android/adb_device_functions.sh

ANDROID_GOMA_WRAPPER=""
if [[ -d $GOMA_DIR ]]; then
  ANDROID_GOMA_WRAPPER="$GOMA_DIR/gomacc"
fi
export ANDROID_GOMA_WRAPPER

export CC_target="${ANDROID_GOMA_WRAPPER} $(echo -n ${ANDROID_TOOLCHAIN}/*-gcc)"
export CXX_target="${ANDROID_GOMA_WRAPPER} \
  $(echo -n ${ANDROID_TOOLCHAIN}/*-g++)"
export LINK_target=$(echo -n ${ANDROID_TOOLCHAIN}/*-gcc)
export AR_target=$(echo -n ${ANDROID_TOOLCHAIN}/*-ar)

# Performs a gyp_chromium run to convert gyp->Makefile for android code.
android_gyp() {
  echo "GYP_GENERATORS set to '$GYP_GENERATORS'"
  "${CHROME_SRC}/build/gyp_chromium" --depth="${CHROME_SRC}" --check "$@"
}

# FLOCK needs to be null on system that has no flock
which flock > /dev/null || export FLOCK=
