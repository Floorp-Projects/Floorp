#!/bin/bash

# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Sets up environment for building Chromium on Android.  It can either be
# compiled with the Android tree or using the Android SDK/NDK. To build with
# NDK/SDK: ". build/android/envsetup.sh --sdk".  Environment variable
# ANDROID_SDK_BUILD=1 will then be defined and used in the rest of the setup to
# specifiy build type.

# When building WebView as part of Android we can't use the SDK. Other builds
# default to using the SDK.
# NOTE(yfriedman): This looks unnecessary but downstream the default value
# should be 0 until all builds switch to SDK/NDK.
if [[ "${CHROME_ANDROID_BUILD_WEBVIEW}" -eq 1 ]]; then
  export ANDROID_SDK_BUILD=0
else
  export ANDROID_SDK_BUILD=1
fi
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
if [[ -z "${CHROME_SRC}" ]]; then
  # If $CHROME_SRC was not set, assume current directory is CHROME_SRC.
  export CHROME_SRC="${CURRENT_DIR}"
fi

if [[ "${CURRENT_DIR/"${CHROME_SRC}"/}" == "${CURRENT_DIR}" ]]; then
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

if [[ "${ANDROID_SDK_BUILD}" -eq 1 ]]; then
  sdk_build_init
# Sets up environment for building Chromium for Android with source. Expects
# android environment setup and lunch.
elif [[ -z "$ANDROID_BUILD_TOP" || \
        -z "$ANDROID_TOOLCHAIN" || \
        -z "$ANDROID_PRODUCT_OUT" ]]; then
  echo "Android build environment variables must be set."
  echo "Please cd to the root of your Android tree and do: "
  echo "  . build/envsetup.sh"
  echo "  lunch"
  echo "Then try this again."
  echo "Or did you mean NDK/SDK build. Run envsetup.sh with --sdk argument."
  return 1
elif [[ -n "$CHROME_ANDROID_BUILD_WEBVIEW" ]]; then
  webview_build_init
else
  non_sdk_build_init
fi

# Workaround for valgrind build
if [[ -n "$CHROME_ANDROID_VALGRIND_BUILD" ]]; then
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

# Declare Android are cross compile.
export GYP_CROSSCOMPILE=1

export CXX_target="${ANDROID_GOMA_WRAPPER} \
    $(echo -n ${ANDROID_TOOLCHAIN}/*-g++)"

# Performs a gyp_chromium run to convert gyp->Makefile for android code.
android_gyp() {
  echo "GYP_GENERATORS set to '$GYP_GENERATORS'"
  # http://crbug.com/143889.
  # In case we are doing a Clang build, we have to unset CC_target and
  # CXX_target. Otherwise GYP ends up generating a gcc build (although we set
  # 'clang' to 1). This behavior was introduced by
  # 54d2f6fe6d8a7b9d9786bd1f8540df6b4f46b83f in GYP.
  (
    # Fork to avoid side effects on the user's environment variables.
    if echo "$GYP_DEFINES" | grep -qE '(clang|asan)'; then
      if echo "$CXX_target" | grep -q g++; then
        unset CXX_target
      fi
    fi
    "${CHROME_SRC}/build/gyp_chromium" --depth="${CHROME_SRC}" --check "$@"
  )
}

# FLOCK needs to be null on system that has no flock
which flock > /dev/null || export FLOCK=
