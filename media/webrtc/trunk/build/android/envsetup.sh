#!/bin/bash

# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Sets up environment for building Chromium on Android. Only Android NDK,
# Revision 6b on Linux or Mac is offically supported.
#
# To run this script, the system environment ANDROID_NDK_ROOT must be set
# to Android NDK's root path.
#
# TODO(michaelbai): Develop a standard for NDK/SDK integration.
#
# If current path isn't the Chrome's src directory, CHROME_SRC must be set
# to the Chrome's src directory.

if [ ! -d "${ANDROID_NDK_ROOT}" ]; then
  echo "ANDROID_NDK_ROOT must be set to the path of Android NDK, Revision 6b." \
    >& 2
  echo "which could be installed by" >& 2
  echo "<chromium_tree>/src/build/install-build-deps-android.sh" >& 2
  return 1
fi

if [ ! -d "${ANDROID_SDK_ROOT}" ]; then
  echo "ANDROID_SDK_ROOT must be set to the path of Android SDK, Android 3.2." \
    >& 2
  echo "which could be installed by" >& 2
  echo "<chromium_tree>/src/build/install-build-deps-android.sh" >& 2
  return 1
fi

host_os=$(uname -s | sed -e 's/Linux/linux/;s/Darwin/mac/')

case "${host_os}" in
  "linux")
    toolchain_dir="linux-x86"
    ;;
  "mac")
    toolchain_dir="darwin-x86"
    ;;
  *)
    echo "Host platform ${host_os} is not supported" >& 2
    return 1
esac

export ANDROID_TOOLCHAIN="${ANDROID_NDK_ROOT}/toolchains/arm-linux-androideabi-4.4.3/prebuilt/${toolchain_dir}/bin/"

# Add Android SDK's platform-tools to system path.
export PATH="${PATH}:${ANDROID_SDK_ROOT}/platform-tools/"

if [ ! -d "${ANDROID_TOOLCHAIN}" ]; then
  echo "Can not find Android toolchain in ${ANDROID_TOOLCHAIN}." >& 2
  echo "The NDK version might be wrong." >& 2
  return 1
fi

if [ -z "${CHROME_SRC}" ]; then
  # if $CHROME_SRC was not set, assume current directory is CHROME_SRC.
  export CHROME_SRC=$(pwd)
fi

if [ ! -d "${CHROME_SRC}" ]; then
  echo "CHROME_SRC must be set to the path of Chrome source code." >& 2
  return 1
fi

make() {
  # TODO(michaelbai): how to use ccache in NDK.
  if [ -n "${USE_CCACHE}" ]; then
    if [ -e "${PREBUILT_CCACHE_PATH}" ]; then
      use_ccache_var="$PREBUILT_CCACHE_PATH "
    else
      use_ccache_var=""
    fi
  fi
  # Only cross-compile if the build is being done either from Chromium's src/
  # directory, or through WebKit, in which case the WEBKIT_ANDROID_BUILD
  # environment variable will be defined. WebKit uses a different directory.
  if [ -f "$PWD/build/android/envsetup.sh" ] ||
     [ -n "${WEBKIT_ANDROID_BUILD}" ]; then
    CC="${use_ccache_var}${CROSS_CC}" CXX="${use_ccache_var}${CROSS_CXX}" \
    LINK="${CROSS_LINK}" AR="${CROSS_AR}" RANLIB="${CROSS_RANLIB}" \
      command make $*
  else
    command make $*
  fi
}

# Performs a gyp_chromium run to convert gyp->Makefile for android code.
android_gyp() {
  "${CHROME_SRC}/build/gyp_chromium" --depth="${CHROME_SRC}"
}

firstword() {
  echo "${1}"
}

export CROSS_AR="$(firstword "${ANDROID_TOOLCHAIN}"/*-ar)"
export CROSS_CC="$(firstword "${ANDROID_TOOLCHAIN}"/*-gcc)"
export CROSS_CXX="$(firstword "${ANDROID_TOOLCHAIN}"/*-g++)"
export CROSS_LINK="$(firstword "${ANDROID_TOOLCHAIN}"/*-gcc)"
export CROSS_RANLIB="$(firstword "${ANDROID_TOOLCHAIN}"/*-ranlib)"
export OBJCOPY="$(firstword "${ANDROID_TOOLCHAIN}"/*-objcopy)"
export STRIP="$(firstword "${ANDROID_TOOLCHAIN}"/*-strip)"

# The set of GYP_DEFINES to pass to gyp. Use 'readlink -e' on directories
# to canonicalize them (remove double '/', remove trailing '/', etc).
DEFINES="OS=android"
DEFINES+=" android_build_type=0"  # Currently, Only '0' is supportted.
DEFINES+=" host_os=${host_os}"
DEFINES+=" linux_fpic=1"
DEFINES+=" release_optimize=s"
DEFINES+=" linux_use_tcmalloc=0"
DEFINES+=" disable_nacl=1"
DEFINES+=" remoting=0"
DEFINES+=" p2p_apis=0"
DEFINES+=" enable_touch_events=1"
DEFINES+=" build_ffmpegsumo=0"
# TODO(bulach): use "shared_libraries" once the transition from executable
# is over.
DEFINES+=" gtest_target_type=executable"
DEFINES+=" branding=Chromium"

# If the TARGET_PRODUCT wasn't set, use 'full' by default.
if [ -z "${TARGET_PRODUCT}" ]; then
  TARGET_PRODUCT="full"
fi

# The following defines will affect ARM code generation of both C/C++ compiler
# and V8 mksnapshot.
case "${TARGET_PRODUCT}" in
  "full")
    DEFINES+=" target_arch=arm"
    DEFINES+=" arm_neon=0 armv7=1 arm_thumb=1 arm_fpu=vfpv3-d16"
    ;;
  *x86*)
    DEFINES+=" target_arch=ia32 use_libffmpeg=0"
    ;;
  *)
    echo "TARGET_PRODUCT: ${TARGET_PRODUCT} is not supported." >& 2
    return 1
esac

export GYP_DEFINES="${DEFINES}"

# Use the "android" flavor of the Makefile generator for both Linux and OS X.
export GYP_GENERATORS="make-android"

# Use our All target as the default
export GYP_GENERATOR_FLAGS="${GYP_GENERATOR_FLAGS} default_target=All"

# We want to use our version of "all" targets.
export CHROMIUM_GYP_FILE="${CHROME_SRC}/build/all_android.gyp"
