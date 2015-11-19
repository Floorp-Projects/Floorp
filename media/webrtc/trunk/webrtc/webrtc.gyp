# Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.
{
  'conditions': [
    ['include_tests==1', {
      'includes': [
        'libjingle/xmllite/xmllite_tests.gypi',
        'libjingle/xmpp/xmpp_tests.gypi',
        'p2p/p2p_tests.gypi',
        'sound/sound_tests.gypi',
        'webrtc_tests.gypi',
      ],
    }],
  ],
  'includes': [
    'build/common.gypi',
    'video/webrtc_video.gypi',
  ],
  'variables': {
    'webrtc_all_dependencies': [
      'base/base.gyp:*',
      'sound/sound.gyp:*',
      'common.gyp:*',
      'common_audio/common_audio.gyp:*',
      'common_video/common_video.gyp:*',
      'modules/modules.gyp:*',
      'p2p/p2p.gyp:*',
      'system_wrappers/system_wrappers.gyp:*',
      'tools/tools.gyp:*',
      'video_engine/video_engine.gyp:*',
      'voice_engine/voice_engine.gyp:*',
      '<(webrtc_vp8_dir)/vp8.gyp:*',
      '<(webrtc_vp9_dir)/vp9.gyp:*',
    ],
  },
  'targets': [
    {
      'target_name': 'webrtc_all',
      'type': 'none',
      'dependencies': [
        '<@(webrtc_all_dependencies)',
        'webrtc',
      ],
      'conditions': [
        ['include_tests==1', {
          'dependencies': [
            'common_video/common_video_unittests.gyp:*',
            'system_wrappers/system_wrappers_tests.gyp:*',
            'test/metrics.gyp:*',
            'test/test.gyp:*',
            'test/webrtc_test_common.gyp:webrtc_test_common_unittests',
            'webrtc_tests',
            'rtc_unittests',
          ],
        }],
      ],
    },
    {
      # TODO(pbos): This is intended to contain audio parts as well as soon as
      #             VoiceEngine moves to the same new API format.
      'target_name': 'webrtc',
      'type': 'static_library',
      'sources': [
        'call.h',
        'config.h',
        'experiments.h',
        'frame_callback.h',
        'transport.h',
        'video_receive_stream.h',
        'video_renderer.h',
        'video_send_stream.h',

        '<@(webrtc_video_sources)',
      ],
      'dependencies': [
        'common.gyp:*',
        '<@(webrtc_video_dependencies)',
      ],
      'conditions': [
        # TODO(andresp): Chromium libpeerconnection should link directly with
        # this and no if conditions should be needed on webrtc build files.
        ['build_with_chromium==1', {
          'dependencies': [
            '<(webrtc_root)/modules/modules.gyp:video_capture',
            '<(webrtc_root)/modules/modules.gyp:video_render',
          ],
        }],
      ],
    },
  ],
}
