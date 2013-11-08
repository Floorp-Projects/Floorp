# Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'targets': [
    {
      'target_name': 'video_coding_test',
      'type': 'executable',
      'dependencies': [
         'rtp_rtcp',
         'video_processing',
         'webrtc_video_coding',
         'webrtc_utility',
         '<(DEPTH)/testing/gtest.gyp:gtest',
         '<(DEPTH)/third_party/gflags/gflags.gyp:gflags',
         '<(webrtc_root)/test/test.gyp:test_support',
         '<(webrtc_root)/test/metrics.gyp:metrics',
         '<(webrtc_root)/common_video/common_video.gyp:common_video',
      ],
      'include_dirs': [
         '../../../interface',
         '../../codecs/vp8/include',
         '../../../../system_wrappers/interface',
          '../../../../common_video/interface',
         '../source',
      ],
      'sources': [
        # headers
        '../test/codec_database_test.h',
        '../test/generic_codec_test.h',
        '../test/jitter_estimate_test.h',
        '../test/media_opt_test.h',
        '../test/mt_test_common.h',
        '../test/normal_test.h',
        '../test/pcap_file_reader.h',
        '../test/quality_modes_test.h',
        '../test/receiver_tests.h',
        '../test/release_test.h',
        '../test/rtp_file_reader.h',
        '../test/rtp_player.h',
        '../test/test_callbacks.h',
        '../test/test_util.h',
        '../test/vcm_payload_sink_factory.h',
        '../test/video_source.h',

        # sources
        '../test/codec_database_test.cc',
        '../test/generic_codec_test.cc',
        '../test/media_opt_test.cc',
        '../test/mt_test_common.cc',
        '../test/mt_rx_tx_test.cc',
        '../test/normal_test.cc',
        '../test/pcap_file_reader.cc',
        '../test/quality_modes_test.cc',
        '../test/rtp_file_reader.cc',
        '../test/rtp_player.cc',
        '../test/test_callbacks.cc',
        '../test/test_util.cc',
        '../test/tester_main.cc',
        '../test/vcm_payload_sink_factory.cc',
        '../test/video_rtp_play_mt.cc',
        '../test/video_rtp_play.cc',
        '../test/video_source.cc',
      ], # sources
    },
  ],
}
