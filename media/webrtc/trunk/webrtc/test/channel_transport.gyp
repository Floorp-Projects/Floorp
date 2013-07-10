# Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
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
      'target_name': 'channel_transport',
      'type': 'static_library',
      'dependencies': [
        '<(DEPTH)/testing/gtest.gyp:gtest',
        '<(webrtc_root)/system_wrappers/source/system_wrappers.gyp:system_wrappers',
      ],
      'sources': [
        # PLATFORM INDEPENDENT SOURCE FILES
        'channel_transport/channel_transport.cc',
        'channel_transport/include/channel_transport.h',
        'channel_transport/udp_transport.h',
        'channel_transport/udp_transport_impl.cc',
        'channel_transport/udp_socket_wrapper.cc',
        'channel_transport/udp_socket_manager_wrapper.cc',
        'channel_transport/udp_transport_impl.h',
        'channel_transport/udp_socket_wrapper.h',
        'channel_transport/udp_socket_manager_wrapper.h',
        # PLATFORM SPECIFIC SOURCE FILES - Will be filtered below
        # Posix (Linux/Mac)
        'channel_transport/udp_socket_posix.cc',
        'channel_transport/udp_socket_posix.h',
        'channel_transport/udp_socket_manager_posix.cc',
        'channel_transport/udp_socket_manager_posix.h',
        # win
        'channel_transport/udp_socket2_manager_win.cc',
        'channel_transport/udp_socket2_manager_win.h',
        'channel_transport/udp_socket2_win.cc',
        'channel_transport/udp_socket2_win.h',
        'channel_transport/traffic_control_win.cc',
        'channel_transport/traffic_control_win.h',
      ], # source
    },
    {
      'target_name': 'channel_transport_unittests',
      'type': 'executable',
      'dependencies': [
        'channel_transport',
        '<(DEPTH)/testing/gtest.gyp:gtest',
        '<(DEPTH)/testing/gmock.gyp:gmock',
        '<(webrtc_root)/test/test.gyp:test_support_main',
      ],
      'sources': [
        'channel_transport/udp_transport_unittest.cc',
        'channel_transport/udp_socket_manager_unittest.cc',
        'channel_transport/udp_socket_wrapper_unittest.cc',
      ],
      # Disable warnings to enable Win64 build, issue 1323.
      'msvs_disabled_warnings': [
        4267,  # size_t to int truncation.
      ],
    }, # channel_transport_unittests
  ], # targets
}
