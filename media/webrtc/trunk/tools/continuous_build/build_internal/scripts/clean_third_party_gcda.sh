#!/bin/sh
#  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
#  Use of this source code is governed by a BSD-style license
#  that can be found in the LICENSE file in the root of the source
#  tree. An additional intellectual property rights grant can be found
#  in the file PATENTS.  All contributing project authors may
#  be found in the AUTHORS file in the root of the source tree.
#

# This script removes all .gcda files from third_party in order to work around
# a bug in LCOV (this should also increase the bot speed).
find . -name "*.gcda" -path "*/third_party/*" | xargs rm -f
