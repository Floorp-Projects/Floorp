# Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'targets': [{
      'target_name': 'video_coding_test',
      'type': 'executable',
      'dependencies': [
         '<(webrtc_root)/../testing/gtest.gyp:gtest',
         '<(webrtc_root)/../test/test.gyp:test_support',
         '<(webrtc_root)/../test/metrics.gyp:metrics',
         'webrtc_video_coding',
         'rtp_rtcp',
         'webrtc_utility',
         'video_processing',
         '<(webrtc_root)/common_video/common_video.gyp:webrtc_libyuv',
      ],
      'include_dirs': [
         '../../../interface',
         '../../codecs/vp8/main/interface',
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
      ], # source
    },
    {
      'target_name': 'video_coding_unittests',
      'type': 'executable',
      'dependencies': [
        'webrtc_video_coding',
        '<(webrtc_root)/../test/test.gyp:test_support_main',
        '<(webrtc_root)/../testing/gtest.gyp:gtest',
        '<(webrtc_root)/../testing/gmock.gyp:gmock',
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
        'qm_select_unittest.cc',
      ],
    },
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
