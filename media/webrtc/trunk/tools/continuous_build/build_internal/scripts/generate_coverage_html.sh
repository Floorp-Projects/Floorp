#!/bin/sh
#  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
#  Use of this source code is governed by a BSD-style license
#  that can be found in the LICENSE file in the root of the source
#  tree. An additional intellectual property rights grant can be found
#  in the file PATENTS.  All contributing project authors may
#  be found in the AUTHORS file in the root of the source tree.
#

# Generates a LCOV error report and makes the results readable to all.

genhtml $1 --output-directory $2
if [ "$?" -ne "0" ]; then
  exit 1
fi

chmod -R 777 $2
