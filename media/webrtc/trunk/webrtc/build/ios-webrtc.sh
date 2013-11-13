#!/bin/bash -e

# Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

# Work in trunk/.
cd "$(dirname $0)/../.."

export GYP_DEFINES="build_with_libjingle=1 build_with_chromium=0"
GYP_DEFINES="$GYP_DEFINES OS=ios target_arch=armv7 key_id=\"\""
export GYP_GENERATORS="ninja"
export GYP_CROSSCOMPILE=1

echo "@@@BUILD_STEP runhooks@@@"
gclient runhooks || { echo "@@@STEP_FAILURE@@@"; exit 2; }

echo "@@@BUILD_STEP compile@@@"
ninja -C out/Debug || { echo "@@@STEP_FAILURE@@@"; exit 2; }

exit 0
