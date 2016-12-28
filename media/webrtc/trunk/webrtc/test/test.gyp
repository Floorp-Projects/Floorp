# Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

# TODO(andrew): consider moving test_support to src/base/test.
{
  'includes': [
    '../build/common.gypi',
  ],
  'targets': [
    {
      'target_name': 'channel_transport',
      'type': 'static_library',
      'dependencies': [
        '<(DEPTH)/testing/gtest.gyp:gtest',
        '<(webrtc_root)/common.gyp:webrtc_common',
        '<(webrtc_root)/system_wrappers/system_wrappers.gyp:system_wrappers',
      ],
      'sources': [
        'channel_transport/channel_transport.cc',
        'channel_transport/channel_transport.h',
        'channel_transport/traffic_control_win.cc',
        'channel_transport/traffic_control_win.h',
        'channel_transport/udp_socket_manager_posix.cc',
        'channel_transport/udp_socket_manager_posix.h',
        'channel_transport/udp_socket_manager_wrapper.cc',
        'channel_transport/udp_socket_manager_wrapper.h',
        'channel_transport/udp_socket_posix.cc',
        'channel_transport/udp_socket_posix.h',
        'channel_transport/udp_socket_wrapper.cc',
        'channel_transport/udp_socket_wrapper.h',
        'channel_transport/udp_socket2_manager_win.cc',
        'channel_transport/udp_socket2_manager_win.h',
        'channel_transport/udp_socket2_win.cc',
        'channel_transport/udp_socket2_win.h',
        'channel_transport/udp_transport.h',
        'channel_transport/udp_transport_impl.cc',
        'channel_transport/udp_transport_impl.h',
      ],
      'conditions': [
        ['OS=="win" and clang==1', {
          'msvs_settings': {
            'VCCLCompilerTool': {
              'AdditionalOptions': [
                # Disable warnings failing when compiling with Clang on Windows.
                # https://bugs.chromium.org/p/webrtc/issues/detail?id=5366
                '-Wno-parentheses-equality',
                '-Wno-reorder',
                '-Wno-tautological-constant-out-of-range-compare',
                '-Wno-unused-private-field',
              ],
            },
          },
        }],
      ],  # conditions.
    },
    {
      'target_name': 'fake_video_frames',
      'type': 'static_library',
      'sources': [
        'fake_texture_frame.cc',
        'fake_texture_frame.h',
        'frame_generator.cc',
        'frame_generator.h',
      ],
      'dependencies': [
        '<(webrtc_root)/common_video/common_video.gyp:common_video',
      ],
    },
    {
      'target_name': 'rtp_test_utils',
      'type': 'static_library',
      'sources': [
        'rtcp_packet_parser.cc',
        'rtcp_packet_parser.h',
        'rtp_file_reader.cc',
        'rtp_file_reader.h',
        'rtp_file_writer.cc',
        'rtp_file_writer.h',
      ],
      'dependencies': [
        '<(DEPTH)/webrtc/common.gyp:webrtc_common',
        '<(DEPTH)/testing/gtest.gyp:gtest',
        '<(webrtc_root)/modules/modules.gyp:rtp_rtcp',
      ],
    },
    {
      'target_name': 'field_trial',
      'type': 'static_library',
      'sources': [
        'field_trial.cc',
        'field_trial.h',
      ],
      'dependencies': [
        '<(webrtc_root)/common.gyp:webrtc_common',
        '<(webrtc_root)/system_wrappers/system_wrappers.gyp:field_trial_default',
        '<(webrtc_root)/system_wrappers/system_wrappers.gyp:system_wrappers',
      ],
    },
    {
      'target_name': 'histogram',
      'type': 'static_library',
      'sources': [
        'histogram.cc',
        'histogram.h',
      ],
      'dependencies': [
        '<(webrtc_root)/common.gyp:webrtc_common',
        '<(webrtc_root)/system_wrappers/system_wrappers.gyp:system_wrappers',
      ],
    },
    {
      'target_name': 'test_main',
      'type': 'static_library',
      'sources': [
        'test_main.cc',
      ],
      'dependencies': [
        'field_trial',
        'histogram',
        'test_support',
        '<(DEPTH)/testing/gtest.gyp:gtest',
        '<(DEPTH)/third_party/gflags/gflags.gyp:gflags',
      ],
    },
    {
      'target_name': 'test_support',
      'type': 'static_library',
      'dependencies': [
        '<(DEPTH)/testing/gtest.gyp:gtest',
        '<(DEPTH)/testing/gmock.gyp:gmock',
        '<(webrtc_root)/common.gyp:gtest_prod',
        '<(webrtc_root)/system_wrappers/system_wrappers.gyp:system_wrappers',
      ],
      'sources': [
        'testsupport/fileutils.cc',
        'testsupport/fileutils.h',
        'testsupport/frame_reader.cc',
        'testsupport/frame_reader.h',
        'testsupport/frame_writer.cc',
        'testsupport/frame_writer.h',
        'testsupport/iosfileutils.mm',
        'testsupport/mock/mock_frame_reader.h',
        'testsupport/mock/mock_frame_writer.h',
        'testsupport/packet_reader.cc',
        'testsupport/packet_reader.h',
        'testsupport/perf_test.cc',
        'testsupport/perf_test.h',
        'testsupport/trace_to_stderr.cc',
        'testsupport/trace_to_stderr.h',
      ],
      'conditions': [
        ['OS=="ios"', {
          'xcode_settings': {
            'CLANG_ENABLE_OBJC_ARC': 'YES',
          },
        }],
        ['use_x11==1', {
          'dependencies': [
            '<(DEPTH)/tools/xdisplaycheck/xdisplaycheck.gyp:xdisplaycheck',
          ],
        }],
      ],
    },
    {
      # Depend on this target when you want to have test_support but also the
      # main method needed for gtest to execute!
      'target_name': 'test_support_main',
      'type': 'static_library',
      'dependencies': [
        'field_trial',
        'histogram',
        'test_support',
        '<(DEPTH)/testing/gmock.gyp:gmock',
        '<(DEPTH)/testing/gtest.gyp:gtest',
        '<(DEPTH)/third_party/gflags/gflags.gyp:gflags',
      ],
      'sources': [
        'run_all_unittests.cc',
        'test_suite.cc',
        'test_suite.h',
      ],
    },
    {
      # Depend on this target when you want to have test_support and a special
      # main for mac which will run your test on a worker thread and consume
      # events on the main thread. Useful if you want to access a webcam.
      # This main will provide all the scaffolding and objective-c black magic
      # for you. All you need to do is to implement a function in the
      # run_threaded_main_mac.h file (ImplementThisToRunYourTest).
      'target_name': 'test_support_main_threaded_mac',
      'type': 'static_library',
      'dependencies': [
        'test_support',
      ],
      'sources': [
        'testsupport/mac/run_threaded_main_mac.h',
        'testsupport/mac/run_threaded_main_mac.mm',
      ],
    },
    {
      'target_name': 'test_support_unittests',
      'type': '<(gtest_target_type)',
      'dependencies': [
        'channel_transport',
        'test_support_main',
        '<(DEPTH)/testing/gmock.gyp:gmock',
        '<(DEPTH)/testing/gtest.gyp:gtest',
      ],
      'sources': [
        'channel_transport/udp_transport_unittest.cc',
        'channel_transport/udp_socket_manager_unittest.cc',
        'channel_transport/udp_socket_wrapper_unittest.cc',
        'testsupport/always_passing_unittest.cc',
        'testsupport/unittest_utils.h',
        'testsupport/fileutils_unittest.cc',
        'testsupport/frame_reader_unittest.cc',
        'testsupport/frame_writer_unittest.cc',
        'testsupport/packet_reader_unittest.cc',
        'testsupport/perf_test_unittest.cc',
      ],
      # Disable warnings to enable Win64 build, issue 1323.
      'msvs_disabled_warnings': [
        4267,  # size_t to int truncation.
      ],
      'conditions': [
        ['OS=="android"', {
          'dependencies': [
            '<(DEPTH)/testing/android/native_test.gyp:native_test_native_code',
          ],
        }],
      ],
    },
  ],
  'conditions': [
    ['include_tests==1 and OS=="android"', {
      'targets': [
        {
          'target_name': 'test_support_unittests_apk_target',
          'type': 'none',
          'dependencies': [
            '<(apk_tests_path):test_support_unittests_apk',
          ],
        },
      ],
    }],
    ['test_isolation_mode != "noop"', {
      'targets': [
        {
          'target_name': 'test_support_unittests_run',
          'type': 'none',
          'dependencies': [
            'test_support_unittests',
          ],
          'includes': [
            '../build/isolate.gypi',
          ],
          'sources': [
            'test_support_unittests.isolate',
          ],
        },
      ],
    }],
  ],
}
