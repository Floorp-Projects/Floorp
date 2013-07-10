#!/bin/bash

# Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

function build_project() {
  # make the target string
  if [[ -z "$2" ]]; then
    target_string=""
  else
    declare -a arg_target=("${!2}")

  for item in ${arg_target[*]}
    do
      temp_string="-target $item "
      target_string=$target_string$temp_string
    done
  fi

  # xcodebuild
  xcodebuild -project "$1" -sdk iphoneos \
  -configuration ${CONFIGURATION} \
  -CONFIGURATION_BUILD_DIR=${CONFIGURATION_BUILD_DIR} $target_string

  if [ "$?" != "0" ]; then
    echo "[Error] build $1 failed!" 1>&2
    echo "@@@STEP_FAILURE@@@"
    exit 1
  fi
}

# change the working directory to trunk
cd "$( dirname "${BASH_SOURCE[0]}" )/../.."

# build setting
CONFIGURATION_BUILD_DIR=./xcodebuild
CONFIGURATION=Debug
GYPDEF="OS=ios target_arch=arm armv7=1 arm_neon=1 enable_video=0 include_opus=1"

export GYP_DEFINES=$GYPDEF
echo "[Running gclient runhooks...]"
echo "@@@BUILD_STEP runhooks@@@"
gclient runhooks
if [ "$?" != "0" ]; then
  echo "[Error] gclient runhooks failed!" 1>&2
  echo "@@@STEP_FAILURE@@@"
  exit 2
fi
echo "[Projects updated]\n"

echo "@@@BUILD_STEP compile@@@"
echo "[Building XCode projects...]"
array_target_module=(
  "bitrate_controller" "media_file" "paced_sender" "remote_bitrate_estimator"
  "webrtc_utility" "rtp_rtcp" "CNG" "G711" "G722" "iLBC" "iSACFix" "PCM16B"
  "audio_coding_module" "NetEq" "audio_conference_mixer" "audio_device"
  "audio_processing" "iSAC" "isac_neon" "audio_processing_neon" "webrtc_opus"
)
array_target_opus=("opus")

build_project "webrtc/common_audio/common_audio.xcodeproj"
build_project "webrtc/modules/modules.xcodeproj" array_target_module[@]
build_project "webrtc/system_wrappers/source/system_wrappers.xcodeproj"
build_project "webrtc/voice_engine/voice_engine.xcodeproj"
build_project "third_party/opus/opus.xcodeproj" array_target_opus[@]
echo "[Building XCode projects is successful]\n"

exit 0
