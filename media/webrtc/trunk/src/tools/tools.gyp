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
  ],
}
