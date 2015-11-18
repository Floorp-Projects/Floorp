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
        '<(webrtc_root)/common_audio/common_audio.gyp:common_audio',
        '<(webrtc_root)/common_video/common_video.gyp:common_video',
        '<(webrtc_root)/system_wrappers/system_wrappers.gyp:system_wrappers',
      ],
      'sources': [
        'main/interface/video_processing.h',
        'main/interface/video_processing_defines.h',
        'main/source/brighten.cc',
        'main/source/brighten.h',
        'main/source/brightness_detection.cc',
        'main/source/brightness_detection.h',
        'main/source/color_enhancement.cc',
        'main/source/color_enhancement.h',
        'main/source/color_enhancement_private.h',
        'main/source/content_analysis.cc',
        'main/source/content_analysis.h',
        'main/source/deflickering.cc',
        'main/source/deflickering.h',
        'main/source/frame_preprocessor.cc',
        'main/source/frame_preprocessor.h',
        'main/source/spatial_resampler.cc',
        'main/source/spatial_resampler.h',
        'main/source/video_decimator.cc',
        'main/source/video_decimator.h',
        'main/source/video_processing_impl.cc',
        'main/source/video_processing_impl.h',
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
            'main/source/content_analysis_sse2.cc',
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

