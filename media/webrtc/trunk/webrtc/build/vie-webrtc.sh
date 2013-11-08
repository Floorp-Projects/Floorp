#!/bin/sh

# Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.
set -e

# TODO(sjlee): remove this whole script file.
# (https://code.google.com/p/webrtc/issues/detail?id=2028)
function build_project() {
  # make the target string
  local target_string=""
  if [[ -n "$2" ]]; then
    target_string="-target $2"
  fi

  xcodebuild -project "$1" -sdk iphoneos -arch armv7 \
    -configuration ${CONFIGURATION} \
    -CONFIGURATION_BUILD_DIR=${CONFIGURATION_BUILD_DIR} $target_string
}

# change the working directory to trunk
cd "$( dirname "$0" )/../.."

# build setting
CONFIGURATION_BUILD_DIR=./xcodebuild
CONFIGURATION=Debug
export GYP_DEFINES="OS=ios target_arch=arm armv7=1 arm_neon=1"
# TODO(sjlee): remove this script.
# (https://webrtc-codereview.appspot.com/1874005)

# update gyp settings
echo '[Updating gyp settings...]'
gclient runhooks
./build/gyp_chromium --depth=. \
webrtc/modules/remote_bitrate_estimator/remote_bitrate_estimator_components.gyp
./build/gyp_chromium --depth=. \
webrtc/modules/video_coding/utility/video_coding_utility.gyp
./build/gyp_chromium --depth=. third_party/opus/opus.gyp
./build/gyp_chromium --depth=. third_party/libyuv/libyuv.gyp
./build/gyp_chromium --depth=. third_party/libjpeg/libjpeg.gyp

# build the xcode projects
echo '[Building xcode projects...]'

build_project "webrtc/modules/remote_bitrate_estimator/remote_bitrate_estimator_components.xcodeproj"
build_project "webrtc/modules/video_coding/utility/video_coding_utility.xcodeproj"
build_project "third_party/opus/opus.xcodeproj" "opus"
build_project "third_party/libjpeg/libjpeg.xcodeproj"
build_project "third_party/libyuv/libyuv.xcodeproj"

# build the libvpx
cd third_party/libvpx/source/libvpx

./configure --target=armv7-darwin-gcc --disable-vp9 \
  --libc=/Applications/Xcode.app/Contents/Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS6.1.sdk

make

cd -

cp third_party/libvpx/source/libvpx/libvpx.a \
  ${CONFIGURATION_BUILD_DIR}/${CONFIGURATION}-iphoneos

echo "[Building xcode projects is success...]\n"
