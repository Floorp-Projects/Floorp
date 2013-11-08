# Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'includes': ['build/common.gypi',],
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
      'target_name': 'webrtc',
      'type': 'none',
      'dependencies': [
        '<@(webrtc_all_dependencies)',
      ],
      'conditions': [
        ['include_tests==1', {
          'dependencies': [
            'system_wrappers/source/system_wrappers_tests.gyp:*',
            'test/metrics.gyp:*',
            'test/test.gyp:*',
            'tools/tools.gyp:*',
            '../tools/e2e_quality/e2e_quality.gyp:*',
          ],
        }],
        ['OS=="android"', {
          'dependencies': [
            '../tools/android/android_tools_precompiled.gyp:*',
            '../tools/android-dummy-test/android_dummy_test.gyp:*',
          ],
        }],
      ],
    },
  ],
}
