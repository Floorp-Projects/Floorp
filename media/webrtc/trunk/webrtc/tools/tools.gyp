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
      'target_name': 'command_line_parser',
      'type': '<(library)',
      'include_dirs': [
        '.',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '.',
        ],
      },
      'sources': [
        'simple_command_line_parser.h',
        'simple_command_line_parser.cc',
      ],
    }, # command_line_parser
    {
      'target_name': 'video_quality_analysis',
      'type': '<(library)',
      'dependencies': [
        '<(DEPTH)/third_party/libyuv/libyuv.gyp:libyuv',
      ],
      'include_dirs': [
        'frame_analyzer',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          'frame_analyzer',
        ],
      },
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
        'command_line_parser',
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
        'command_line_parser',
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
        'command_line_parser',
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
      'type': '<(library)',
      'dependencies': [
        '<(webrtc_root)/common_video/common_video.gyp:common_video',
      ],
      'sources': [
        'frame_editing/frame_editing_lib.cc',
        'frame_editing/frame_editing_lib.h',
      ],
    }, # frame_editing_lib
    {
      'target_name': 'frame_editor',
      'type': 'executable',
      'dependencies': [
        'command_line_parser',
        'frame_editing_lib',
      ],
      'sources': [
        'frame_editing/frame_editing.cc',
      ],
    }, # frame_editing
  ],
  'conditions': [
    ['include_tests==1', {
      'targets' : [
        {
          'target_name': 'tools_unittests',
          'type': 'executable',
          'dependencies': [
            'frame_editing_lib',
            '<(webrtc_root)/test/test.gyp:test_support_main',
            '<(DEPTH)/testing/gtest.gyp:gtest',
          ],
          'sources': [
            'frame_editing/frame_editing_unittest.cc',
          ],
        }, # tools_unittests
      ], # targets
    }], # include_tests
  ], # conditions
}