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
  ],
  'targets': [
    {
      # The metrics code must be kept in its own GYP file in order to
      # avoid a circular dependency error due to the dependency on libyuv.
      # If the code would be put in test.gyp a circular dependency error during
      # GYP generation would occur, because the libyuv.gypi unittest target
      # depends on test_support_main. See issue #160 for more info.
      'target_name': 'metrics',
      'type': 'static_library',
      'dependencies': [
        '<(webrtc_root)/common_video/common_video.gyp:common_video',
        '<(webrtc_root)/system_wrappers/system_wrappers.gyp:system_wrappers',
      ],
      'sources': [
        'testsupport/metrics/video_metrics.h',
        'testsupport/metrics/video_metrics.cc',
      ],
    },
  ], # targets
}
