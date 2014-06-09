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
      'common_audio/common_audio.gyp:*',
      'common_video/common_video.gyp:*',
      'modules/modules.gyp:*',
      'system_wrappers/source/system_wrappers.gyp:*',
      'video_engine/video_engine.gyp:*',
      'voice_engine/voice_engine.gyp:*',
      '<(webrtc_vp8_dir)/vp8.gyp:*',
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
            'system_wrappers/source/system_wrappers_tests.gyp:*',
            'test/metrics.gyp:*',
            'test/test.gyp:*',
            'test/webrtc_test_common.gyp:webrtc_test_common_unittests',
            'tools/tools.gyp:*',
            'webrtc_tests',
          ],
        }],
        ['build_with_chromium==0 and OS=="android"', {
          'dependencies': [
            '../tools/android/android_tools_precompiled.gyp:*',
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
        '<@(webrtc_video_dependencies)',
      ],
    },
  ],
}
