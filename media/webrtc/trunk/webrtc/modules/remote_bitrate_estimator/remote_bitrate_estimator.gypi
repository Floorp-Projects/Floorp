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
        'include/send_time_history.h',
        'aimd_rate_control.cc',
        'aimd_rate_control.h',
        'inter_arrival.cc',
        'inter_arrival.h',
        'overuse_detector.cc',
        'overuse_detector.h',
        'overuse_estimator.cc',
        'overuse_estimator.h',
        'rate_statistics.cc',
        'rate_statistics.h',
        'remote_bitrate_estimator_abs_send_time.cc',
        'remote_bitrate_estimator_abs_send_time.h',
        'remote_bitrate_estimator_single_stream.cc',
        'remote_bitrate_estimator_single_stream.h',
        'remote_estimator_proxy.cc',
        'remote_estimator_proxy.h',
        'send_time_history.cc',
        'transport_feedback_adapter.cc',
        'transport_feedback_adapter.h',
        'test/bwe_test_logging.cc',
        'test/bwe_test_logging.h',
      ], # source
      'conditions': [
        ['enable_bwe_test_logging==1', {
          'defines': [ 'BWE_TEST_LOGGING_COMPILE_TIME_ENABLE=1' ],
        }, {
          'defines': [ 'BWE_TEST_LOGGING_COMPILE_TIME_ENABLE=0' ],
          'sources!': [
            'remote_bitrate_estimator/test/bwe_test_logging.cc'
          ],
        }],
      ],
    },
  ], # targets
  'conditions': [
    ['include_tests==1', {
      'targets': [
        {
          'target_name': 'bwe_simulator',
          'type': 'static_library',
          'dependencies': [
            '<(DEPTH)/testing/gtest.gyp:gtest',
          ],
          'sources': [
            'test/bwe.cc',
            'test/bwe.h',
            'test/bwe_test.cc',
            'test/bwe_test.h',
            'test/bwe_test_baselinefile.cc',
            'test/bwe_test_baselinefile.h',
            'test/bwe_test_fileutils.cc',
            'test/bwe_test_fileutils.h',
            'test/bwe_test_framework.cc',
            'test/bwe_test_framework.h',
            'test/bwe_test_logging.cc',
            'test/bwe_test_logging.h',
            'test/metric_recorder.cc',
            'test/metric_recorder.h',
            'test/packet_receiver.cc',
            'test/packet_receiver.h',
            'test/packet_sender.cc',
            'test/packet_sender.h',
            'test/packet.h',
            'test/estimators/nada.cc',
            'test/estimators/nada.h',
            'test/estimators/remb.cc',
            'test/estimators/remb.h',
            'test/estimators/send_side.cc',
            'test/estimators/send_side.h',
            'test/estimators/tcp.cc',
            'test/estimators/tcp.h',
          ],
          'conditions': [
            ['enable_bwe_test_logging==1', {
              'defines': [ 'BWE_TEST_LOGGING_COMPILE_TIME_ENABLE=1' ],
            }, {
              'defines': [ 'BWE_TEST_LOGGING_COMPILE_TIME_ENABLE=0' ],
              'sources!': [
                'remote_bitrate_estimator/test/bwe_test_logging.cc'
              ],
            }],
          ],
        },
        {
          'target_name': 'bwe_tools_util',
          'type': 'static_library',
          'dependencies': [
            '<(DEPTH)/third_party/gflags/gflags.gyp:gflags',
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
