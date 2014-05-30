# Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'includes': [
    '../build/common.gypi',
    './video_engine_core.gypi',
  ],

  'conditions': [
    ['include_tests==1', {
      'includes': [
        'test/libvietest/libvietest.gypi',
        'test/auto_test/vie_auto_test.gypi',
      ],
    }],
  ],
}

