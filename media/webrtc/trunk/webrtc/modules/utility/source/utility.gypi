# Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'targets': [
    {
      'target_name': 'webrtc_utility',
      'type': 'static_library',
      'dependencies': [
        'audio_coding_module',
        '<(webrtc_root)/common_audio/common_audio.gyp:resampler',
        '<(webrtc_root)/system_wrappers/source/system_wrappers.gyp:system_wrappers',
      ],
      'include_dirs': [
        '../interface',
        '../../interface',
        '../../media_file/interface',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '../interface',
          '../../interface',
          '../../audio_coding/main/interface',
        ],
      },
      'sources': [
        '../interface/audio_frame_operations.h',
        '../interface/file_player.h',
        '../interface/file_recorder.h',
        '../interface/process_thread.h',
        '../interface/rtp_dump.h',
        'audio_frame_operations.cc',
        'coder.cc',
        'coder.h',
        'file_player_impl.cc',
        'file_player_impl.h',
        'file_recorder_impl.cc',
        'file_recorder_impl.h',
        'process_thread_impl.cc',
        'process_thread_impl.h',
        'rtp_dump_impl.cc',
        'rtp_dump_impl.h',
      ],
      'conditions': [
        ['enable_video==1', {
          # Adds support for video recording.
          'defines': [
            'WEBRTC_MODULE_UTILITY_VIDEO',
          ],
          'dependencies': [
            'webrtc_video_coding',
          ],
          'include_dirs': [
            '../../video_coding/main/interface',
          ],
          'sources': [
            'frame_scaler.cc',
            'video_coder.cc',
            'video_frames_queue.cc',
          ],
        }],
      ],
    },
  ], # targets
  'conditions': [
    ['include_tests==1', {
      'targets': [
        {
          'target_name': 'webrtc_utility_unittests',
          'type': 'executable',
          'dependencies': [
            'webrtc_utility',
            '<(DEPTH)/testing/gtest.gyp:gtest',
            '<(webrtc_root)/test/test.gyp:test_support_main',
          ],
          'sources': [
            'audio_frame_operations_unittest.cc',
          ],
        }, # webrtc_utility_unittests
      ], # targets
    }], # include_tests
  ], # conditions
}
