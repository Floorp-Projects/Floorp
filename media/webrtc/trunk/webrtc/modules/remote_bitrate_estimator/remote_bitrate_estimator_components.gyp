# Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'includes': [
    '../../build/common.gypi',
  ],
  'targets': [
    {
      'target_name': 'rbe_components',
      'type': 'static_library',
      'include_dirs': [
        '<(webrtc_root)/modules/remote_bitrate_estimator',
      ],
      'sources': [
        'overuse_detector.cc',
        'overuse_detector.h',
        'remote_bitrate_estimator_single_stream.cc',
        'remote_rate_control.cc',
        'remote_rate_control.h',
      ],
    },
  ],
}
