# Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'targets': [
    {
      'target_name': 'rtp_rtcp_unittests',
      'type': 'executable',
      'dependencies': [
        'rtp_rtcp',
        '<(DEPTH)/testing/gmock.gyp:gmock',
        '<(DEPTH)/testing/gtest.gyp:gtest',
        '<(webrtc_root)/test/test.gyp:test_support_main',
        '<(webrtc_root)/system_wrappers/source/system_wrappers.gyp:system_wrappers',
      ],
      'include_dirs': [
        '../../../',
      ],
      'sources': [
        '../test/testAPI/test_api.cc',
        '../test/testAPI/test_api.h',
        '../test/testAPI/test_api_audio.cc',
        '../test/testAPI/test_api_nack.cc',
        '../test/testAPI/test_api_rtcp.cc',
        '../test/testAPI/test_api_video.cc',
        'mock/mock_rtp_payload_strategy.h',
        'mock/mock_rtp_receiver_video.h',
        'fec_test_helper.cc',
        'fec_test_helper.h',
        'nack_rtx_unittest.cc',
        'producer_fec_unittest.cc',
        'receiver_fec_unittest.cc',
        'rtcp_format_remb_unittest.cc',
        'rtcp_sender_unittest.cc',
        'rtcp_receiver_unittest.cc',
        'rtp_fec_unittest.cc',
        'rtp_format_vp8_unittest.cc',
        'rtp_format_vp8_test_helper.cc',
        'rtp_format_vp8_test_helper.h',
        'rtp_packet_history_unittest.cc',
        'rtp_payload_registry_unittest.cc',
        'rtp_utility_unittest.cc',
        'rtp_header_extension_unittest.cc',
        'rtp_sender_unittest.cc',
        'vp8_partition_aggregator_unittest.cc',
      ],
      # Disable warnings to enable Win64 build, issue 1323.
      'msvs_disabled_warnings': [
        4267,  # size_t to int truncation.
      ],
    },
  ],
}
