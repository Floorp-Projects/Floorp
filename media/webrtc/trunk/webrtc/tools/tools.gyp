# Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
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
      'target_name': 'video_quality_analysis',
      'type': 'static_library',
      'dependencies': [
        '<(DEPTH)/third_party/libyuv/libyuv.gyp:libyuv',
      ],
      'export_dependent_settings': [
        '<(DEPTH)/third_party/libyuv/libyuv.gyp:libyuv',
      ],
      'sources': [
        'frame_analyzer/video_quality_analysis.h',
        'frame_analyzer/video_quality_analysis.cc',
      ],
    }, # video_quality_analysis
    {
      'target_name': 'frame_analyzer',
      'type': 'executable',
      'dependencies': [
        '<(webrtc_root)/tools/internal_tools.gyp:command_line_parser',
        'video_quality_analysis',
      ],
      'sources': [
        'frame_analyzer/frame_analyzer.cc',
      ],
    }, # frame_analyzer
    {
      'target_name': 'psnr_ssim_analyzer',
      'type': 'executable',
      'dependencies': [
        '<(webrtc_root)/tools/internal_tools.gyp:command_line_parser',
        'video_quality_analysis',
      ],
      'sources': [
        'psnr_ssim_analyzer/psnr_ssim_analyzer.cc',
      ],
    }, # psnr_ssim_analyzer
    {
      'target_name': 'rgba_to_i420_converter',
      'type': 'executable',
      'dependencies': [
        '<(webrtc_root)/tools/internal_tools.gyp:command_line_parser',
        '<(DEPTH)/third_party/libyuv/libyuv.gyp:libyuv',
      ],
      'sources': [
        'converter/converter.h',
        'converter/converter.cc',
        'converter/rgba_to_i420_converter.cc',
      ],
    }, # rgba_to_i420_converter
    {
      'target_name': 'frame_editing_lib',
      'type': 'static_library',
      'dependencies': [
        '<(webrtc_root)/common_video/common_video.gyp:common_video',
      ],
      'sources': [
        'frame_editing/frame_editing_lib.cc',
        'frame_editing/frame_editing_lib.h',
      ],
      # Disable warnings to enable Win64 build, issue 1323.
      'msvs_disabled_warnings': [
        4267,  # size_t to int truncation.
      ],
    }, # frame_editing_lib
    {
      'target_name': 'frame_editor',
      'type': 'executable',
      'dependencies': [
        '<(webrtc_root)/tools/internal_tools.gyp:command_line_parser',
        'frame_editing_lib',
      ],
      'sources': [
        'frame_editing/frame_editing.cc',
      ],
    }, # frame_editing
    {
      'target_name': 'force_mic_volume_max',
      'type': 'executable',
      'dependencies': [
        '<(webrtc_root)/voice_engine/voice_engine.gyp:voice_engine',
      ],
      'sources': [
        'force_mic_volume_max/force_mic_volume_max.cc',
      ],
    }, # force_mic_volume_max
  ],
  'conditions': [
    ['include_tests==1', {
      'targets' : [
        {
          'target_name': 'audio_e2e_harness',
          'type': 'executable',
          'dependencies': [
            '<(webrtc_root)/test/test.gyp:channel_transport',
            '<(webrtc_root)/voice_engine/voice_engine.gyp:voice_engine',
            '<(DEPTH)/testing/gtest.gyp:gtest',
            '<(DEPTH)/third_party/gflags/gflags.gyp:gflags',
          ],
          'sources': [
            'e2e_quality/audio/audio_e2e_harness.cc',
          ],
        }, # audio_e2e_harness
        {
          'target_name': 'tools_unittests',
          'type': '<(gtest_target_type)',
          'dependencies': [
            'frame_editing_lib',
            'video_quality_analysis',
            '<(webrtc_root)/tools/internal_tools.gyp:command_line_parser',
            '<(webrtc_root)/test/test.gyp:test_support_main',
            '<(DEPTH)/testing/gtest.gyp:gtest',
          ],
          'sources': [
            'simple_command_line_parser_unittest.cc',
            'frame_editing/frame_editing_unittest.cc',
            'frame_analyzer/video_quality_analysis_unittest.cc',
          ],
          # Disable warnings to enable Win64 build, issue 1323.
          'msvs_disabled_warnings': [
            4267,  # size_t to int truncation.
          ],
          'conditions': [
            # TODO(henrike): remove build_with_chromium==1 when the bots are
            # using Chromium's buildbots.
            ['build_with_chromium==1 and OS=="android" and gtest_target_type=="shared_library"', {
              'dependencies': [
                '<(DEPTH)/testing/android/native_test.gyp:native_test_native_code',
              ],
            }],
          ],
        }, # tools_unittests
      ], # targets
      # TODO(henrike): remove build_with_chromium==1 when the bots are using
      # Chromium's buildbots.
      'conditions': [
        ['build_with_chromium==1 and OS=="android" and gtest_target_type=="shared_library"', {
          'targets': [
            {
              'target_name': 'tools_unittests_apk_target',
              'type': 'none',
              'dependencies': [
                '<(apk_tests_path):tools_unittests_apk',
              ],
            },
          ],
        }],
        ['test_isolation_mode != "noop"', {
          'targets': [
            {
              'target_name': 'tools_unittests_run',
              'type': 'none',
              'dependencies': [
                'tools_unittests',
              ],
              'includes': [
                '../build/isolate.gypi',
                'tools_unittests.isolate',
              ],
              'sources': [
                'tools_unittests.isolate',
              ],
            },
          ],
        }],
      ],
    }], # include_tests
  ], # conditions
}
