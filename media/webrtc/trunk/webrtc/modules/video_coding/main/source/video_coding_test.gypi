# Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'targets': [{
      'target_name': 'video_coding_integrationtests',
      'type': 'executable',
      'dependencies': [
         'rtp_rtcp',
         'video_codecs_test_framework',
         'video_processing',
         'webrtc_video_coding',
         'webrtc_utility',
         '<(DEPTH)/testing/gtest.gyp:gtest',
         '<(DEPTH)/third_party/google-gflags/google-gflags.gyp:google-gflags',
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
        '../test/quality_modes_test.h',
        '../test/receiver_tests.h',
        '../test/release_test.h',
        '../test/rtp_player.h',
        '../test/test_callbacks.h',
        '../test/test_util.h',
        '../test/video_source.h',

        # sources
        '../test/codec_database_test.cc',
        '../test/decode_from_storage_test.cc',
        '../test/generic_codec_test.cc',
        '../test/jitter_buffer_test.cc',
        '../test/media_opt_test.cc',
        '../test/mt_test_common.cc',
        '../test/mt_rx_tx_test.cc',
        '../test/normal_test.cc',
        '../test/quality_modes_test.cc',
        '../test/receiver_timing_tests.cc',
        '../test/rtp_player.cc',
        '../test/test_callbacks.cc',
        '../test/test_util.cc',
        '../test/tester_main.cc',
        '../test/video_rtp_play_mt.cc',
        '../test/video_rtp_play.cc',
        '../test/video_source.cc',
        '../../codecs/test/videoprocessor_integrationtest.cc',
      ], # sources
    },
    {
      'target_name': 'video_coding_unittests',
      'type': 'executable',
      'dependencies': [
        'video_codecs_test_framework',
        'webrtc_video_coding',
        '<(webrtc_root)/test/test.gyp:test_support_main',
        '<(DEPTH)/testing/gtest.gyp:gtest',
        '<(DEPTH)/testing/gmock.gyp:gmock',
        '<(webrtc_root)/system_wrappers/source/system_wrappers.gyp:system_wrappers',
      ],
      'include_dirs': [
        '../../../interface',
        '../../codecs/interface',
      ],
      'sources': [
        '../interface/mock/mock_vcm_callbacks.h',
        'decoding_state_unittest.cc',
        'jitter_buffer_unittest.cc',
        'session_info_unittest.cc',
        'video_coding_robustness_unittest.cc',
        'video_coding_impl_unittest.cc',
        'qm_select_unittest.cc',
        '../../codecs/test/packet_manipulator_unittest.cc',
        '../../codecs/test/stats_unittest.cc',
        '../../codecs/test/videoprocessor_unittest.cc',
      ],
    },
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
