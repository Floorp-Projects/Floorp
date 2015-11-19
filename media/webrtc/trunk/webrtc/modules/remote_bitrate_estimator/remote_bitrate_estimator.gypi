# Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
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
      'target_name': 'remote_bitrate_estimator',
      'type': 'static_library',
      'dependencies': [
        '<(webrtc_root)/common.gyp:webrtc_common',
        '<(webrtc_root)/system_wrappers/system_wrappers.gyp:system_wrappers',
      ],
      'sources': [
        'include/bwe_defines.h',
        'include/remote_bitrate_estimator.h',
        'aimd_rate_control.cc',
        'aimd_rate_control.h',
        'inter_arrival.cc',
        'inter_arrival.h',
        'mimd_rate_control.cc',
        'mimd_rate_control.h',
        'overuse_detector.cc',
        'overuse_detector.h',
        'overuse_estimator.cc',
        'overuse_estimator.h',
        'rate_statistics.cc',
        'rate_statistics.h',
        'remote_bitrate_estimator_abs_send_time.cc',
        'remote_bitrate_estimator_single_stream.cc',
        'remote_rate_control.cc',
        'remote_rate_control.h',
        'test/bwe_test_logging.cc',
        'test/bwe_test_logging.h',
      ], # source
    },
  ], # targets
  'conditions': [
    ['include_tests==1', {
      'targets': [
        {
          'target_name': 'bwe_tools_util',
          'type': 'static_library',
          'dependencies': [
            '<(webrtc_root)/system_wrappers/system_wrappers.gyp:system_wrappers',
            'rtp_rtcp',
          ],
          'sources': [
            'tools/bwe_rtp.cc',
            'tools/bwe_rtp.h',
          ],
        },
        {
          'target_name': 'bwe_rtp_to_text',
          'type': 'executable',
          'includes': [
            '../rtp_rtcp/rtp_rtcp.gypi',
          ],
          'dependencies': [
            '<(webrtc_root)/system_wrappers/system_wrappers.gyp:system_wrappers',
            '<(webrtc_root)/system_wrappers/system_wrappers.gyp:system_wrappers_default',
            '<(webrtc_root)/test/test.gyp:rtp_test_utils',
            'bwe_tools_util',
            'rtp_rtcp',
          ],
          'direct_dependent_settings': {
            'include_dirs': [
              'include',
            ],
          },
          'sources': [
            'tools/rtp_to_text.cc',
          ], # source
        },
        {
          'target_name': 'bwe_rtp_play',
          'type': 'executable',
          'includes': [
            '../rtp_rtcp/rtp_rtcp.gypi',
          ],
          'dependencies': [
            '<(webrtc_root)/system_wrappers/system_wrappers.gyp:system_wrappers',
            '<(webrtc_root)/system_wrappers/system_wrappers.gyp:system_wrappers_default',
            '<(webrtc_root)/test/test.gyp:rtp_test_utils',
            'bwe_tools_util',
            'rtp_rtcp',
          ],
          'direct_dependent_settings': {
            'include_dirs': [
              'include',
            ],
          },
          'sources': [
            'tools/bwe_rtp_play.cc',
          ], # source
        },
      ],
    }],  # include_tests==1
  ],
}
