# Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'targets': [
    {
      'target_name': 'remote_bitrate_estimator',
      'type': 'static_library',
      'dependencies': [
        # system_wrappers
        '<(webrtc_root)/system_wrappers/source/system_wrappers.gyp:system_wrappers',
      ],
      'include_dirs': [
        'include',
        '../rtp_rtcp/interface',
        '../interface',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          'include',
        ],
      },
      'sources': [
        # interface
        'include/bwe_defines.h',
        'include/remote_bitrate_estimator.h',
        'include/rtp_to_ntp.h',

        # source
        'bitrate_estimator.cc',
        'bitrate_estimator.h',
        'overuse_detector.cc',
        'overuse_detector.h',
        'remote_bitrate_estimator_multi_stream.cc',
        'remote_bitrate_estimator_single_stream.cc',
        'remote_rate_control.cc',
        'remote_rate_control.h',
        'rtp_to_ntp.cc',
      ], # source
    },
  ], # targets
}
