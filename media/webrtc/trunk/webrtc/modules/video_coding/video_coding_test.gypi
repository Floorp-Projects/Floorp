# Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'targets': [
    {
      'target_name': 'rtp_player',
      'type': 'executable',
      'dependencies': [
         'rtp_rtcp',
         'webrtc_video_coding',
         '<(DEPTH)/third_party/gflags/gflags.gyp:gflags',
         '<(webrtc_root)/system_wrappers/system_wrappers.gyp:system_wrappers_default',
         '<(webrtc_root)/test/webrtc_test_common.gyp:webrtc_test_common',
      ],
      'sources': [
        # headers
        'main/test/receiver_tests.h',
        'main/test/rtp_player.h',
        'main/test/vcm_payload_sink_factory.h',

        # sources
        'main/test/rtp_player.cc',
        'main/test/test_util.cc',
        'main/test/tester_main.cc',
        'main/test/vcm_payload_sink_factory.cc',
        'main/test/video_rtp_play.cc',
      ], # sources
    },
  ],
}
