# Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'variables': {
    'use_libjpeg_turbo%': '<(use_libjpeg_turbo)',
    'conditions': [
      ['use_libjpeg_turbo==1', {
        'libjpeg_include_dir%': [ '<(DEPTH)/third_party/libjpeg_turbo', ],
      }, {
        'libjpeg_include_dir%': [ '<(DEPTH)/third_party/libjpeg', ],
       }],
    ],
  },
  'includes': ['../build/common.gypi'],
  'targets': [
    {
      'target_name': 'common_video',
      'type': 'static_library',
      'include_dirs': [
        '<(webrtc_root)/modules/interface/',
        'interface',
        'jpeg/include',
        'libyuv/include',
      ],
      'dependencies': [
        '<(webrtc_root)/system_wrappers/source/system_wrappers.gyp:system_wrappers',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          'interface',
          'jpeg/include',
          'libyuv/include',
        ],
      },
      'conditions': [
        ['build_libjpeg==1', {
          'dependencies': ['<(libjpeg_gyp_path):libjpeg',],
        }, {
          # Need to add a directory normally exported by libjpeg.gyp.
          'include_dirs': ['<(libjpeg_include_dir)'],
        }],
        ['build_libyuv==1', {
          'dependencies': ['<(DEPTH)/third_party/libyuv/libyuv.gyp:libyuv',],
        }, {
          # Need to add a directory normally exported by libyuv.gyp.
          'include_dirs': ['<(libyuv_dir)/include',],
        }],
      ],
      'sources': [
        'interface/i420_video_frame.h',
        'i420_video_frame.cc',
        'jpeg/include/jpeg.h',
        'jpeg/data_manager.cc',
        'jpeg/data_manager.h',
        'jpeg/jpeg.cc',
        'libyuv/include/webrtc_libyuv.h',
        'libyuv/include/scaler.h',
        'libyuv/webrtc_libyuv.cc',
        'libyuv/scaler.cc',
        'plane.h',
        'plane.cc',
      ],
    },
  ],  # targets
  'conditions': [
    ['include_tests==1', {
      'targets': [
        {
          'target_name': 'common_video_unittests',
          'type': 'executable',
          'dependencies': [
             'common_video',
             '<(DEPTH)/testing/gtest.gyp:gtest',
             '<(webrtc_root)/system_wrappers/source/system_wrappers.gyp:system_wrappers',
             '<(webrtc_root)/test/test.gyp:test_support_main',
          ],
          'sources': [
            'i420_video_frame_unittest.cc',
            'jpeg/jpeg_unittest.cc',
            'libyuv/libyuv_unittest.cc',
            'libyuv/scaler_unittest.cc',
            'plane_unittest.cc',
          ],
        },
      ],  # targets
    }],  # include_tests
  ],
}