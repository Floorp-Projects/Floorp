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
      'target_name': 'video_processing',
      'type': 'static_library',
      'dependencies': [
        'webrtc_utility',
        '<(webrtc_root)/common_audio/common_audio.gyp:signal_processing',
        '<(webrtc_root)/common_video/common_video.gyp:common_video',
        '<(webrtc_root)/system_wrappers/source/system_wrappers.gyp:system_wrappers',
      ],
      'include_dirs': [
        '../interface',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '../interface',
        ],
      },
      'sources': [
        '../interface/video_processing.h',
        '../interface/video_processing_defines.h',
        'brighten.cc',
        'brighten.h',
        'brightness_detection.cc',
        'brightness_detection.h',
        'color_enhancement.cc',
        'color_enhancement.h',
        'color_enhancement_private.h',
        'content_analysis.cc',
        'content_analysis.h',
        'deflickering.cc',
        'deflickering.h',
        'denoising.cc',
        'denoising.h',
        'frame_preprocessor.cc',
        'frame_preprocessor.h',
        'spatial_resampler.cc',
        'spatial_resampler.h',
        'video_decimator.cc',
        'video_decimator.h',
        'video_processing_impl.cc',
        'video_processing_impl.h',
      ],
      'conditions': [
        ['target_arch=="ia32" or target_arch=="x64"', {
          'dependencies': [ 'video_processing_sse2', ],
        }],
      ],
    },
  ],
  'conditions': [
    ['target_arch=="ia32" or target_arch=="x64"', {
      'targets': [
        {
          'target_name': 'video_processing_sse2',
          'type': 'static_library',
          'sources': [
            'content_analysis_sse2.cc',
          ],
          'include_dirs': [
            '../interface',
            '../../../interface',
          ],
          'conditions': [
            ['os_posix==1 and OS!="mac"', {
              'cflags': [ '-msse2', ],
            }],
            ['OS=="mac"', {
              'xcode_settings': {
                'OTHER_CFLAGS': [ '-msse2', ],
              },
            }],
          ],
        },
      ],
    }],
  ],
}

