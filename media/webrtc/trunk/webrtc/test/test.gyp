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
        '<(webrtc_root)/system_wrappers/source/system_wrappers.gyp:system_wrappers',
      ],
      'sources': [
        'channel_transport/channel_transport.cc',
        'channel_transport/include/channel_transport.h',
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
    },
    {
      'target_name': 'test_support',
      'type': 'static_library',
      'include_dirs': [
        # TODO(kjellander): Remove this by making all includes use full paths.
        '.',
      ],
      'dependencies': [
        '<(DEPTH)/testing/gtest.gyp:gtest',
        '<(DEPTH)/testing/gmock.gyp:gmock',
        '<(webrtc_root)/system_wrappers/source/system_wrappers.gyp:system_wrappers',
      ],
      'all_dependent_settings': {
        'include_dirs': [
          # TODO(kjellander): Remove this by making all includes use full paths.
          '.',
        ],
      },
      'sources': [
        'test_suite.cc',
        'test_suite.h',
        'testsupport/fileutils.cc',
        'testsupport/fileutils.h',
        'testsupport/frame_reader.cc',
        'testsupport/frame_reader.h',
        'testsupport/frame_writer.cc',
        'testsupport/frame_writer.h',
        'testsupport/gtest_prod_util.h',
        'testsupport/gtest_disable.h',
        'testsupport/mock/mock_frame_reader.h',
        'testsupport/mock/mock_frame_writer.h',
        'testsupport/packet_reader.cc',
        'testsupport/packet_reader.h',
        'testsupport/perf_test.cc',
        'testsupport/perf_test.h',
        'testsupport/trace_to_stderr.cc',
        'testsupport/trace_to_stderr.h',
      ],
    },
    {
      # Depend on this target when you want to have test_support but also the
      # main method needed for gtest to execute!
      'target_name': 'test_support_main',
      'type': 'static_library',
      'dependencies': [
        'test_support',
      ],
      'sources': [
        'run_all_unittests.cc',
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
      'type': 'executable',
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
    },
    {
      'target_name': 'buildbot_tests_scripts',
      'type': 'none',
      'copies': [
        {
          'destination': '<(PRODUCT_DIR)',
          'files': [
            'buildbot_tests.py',
            '<(DEPTH)/tools/e2e_quality/audio/run_audio_test.py',
          ],
        },
        {
          'destination': '<(PRODUCT_DIR)/perf',
          'files': [
            '<(DEPTH)/tools/perf/__init__.py',
            '<(DEPTH)/tools/perf/perf_utils.py',
          ],
        },
      ],
    },  # target buildbot_tests_scripts
  ],
}
