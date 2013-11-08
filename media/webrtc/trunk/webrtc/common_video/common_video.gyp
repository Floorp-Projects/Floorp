# Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'includes': ['../build/common.gypi'],
  'targets': [
    {
      'target_name': 'common_video',
      'type': 'static_library',
      'include_dirs': [
        '<(webrtc_root)/modules/interface/',
        'interface',
        'libyuv/include',
      ],
      'dependencies': [
        '<(webrtc_root)/system_wrappers/source/system_wrappers.gyp:system_wrappers',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          'interface',
          'libyuv/include',
        ],
      },
      'conditions': [
        ['build_libyuv==1', {
          'dependencies': ['<(DEPTH)/third_party/libyuv/libyuv.gyp:libyuv',],
        }, {
          # Need to add a directory normally exported by libyuv.gyp.
          'include_dirs': ['<(libyuv_dir)/include',],
        }],
      ],
      'sources': [
        'interface/i420_video_frame.h',
        'interface/native_handle.h',
        'interface/texture_video_frame.h',
        'i420_video_frame.cc',
        'libyuv/include/webrtc_libyuv.h',
        'libyuv/include/scaler.h',
        'libyuv/webrtc_libyuv.cc',
        'libyuv/scaler.cc',
        'plane.h',
        'plane.cc',
        'texture_video_frame.cc'
      ],
    },
  ],  # targets
  'conditions': [
    ['include_tests==1', {
      'targets': [
        {
          'target_name': 'frame_generator',
          'type': 'static_library',
          'sources': [
            'test/frame_generator.h',
            'test/frame_generator.cc',
          ],
          'dependencies': [
            'common_video',
          ],
        },
        {
          'target_name': 'common_video_unittests',
          'type': '<(gtest_target_type)',
          'dependencies': [
             'common_video',
             '<(DEPTH)/testing/gtest.gyp:gtest',
             '<(webrtc_root)/system_wrappers/source/system_wrappers.gyp:system_wrappers',
             '<(webrtc_root)/test/test.gyp:test_support_main',
          ],
          'sources': [
            'i420_video_frame_unittest.cc',
            'libyuv/libyuv_unittest.cc',
            'libyuv/scaler_unittest.cc',
            'plane_unittest.cc',
            'texture_video_frame_unittest.cc'
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
        },
      ],  # targets
      'conditions': [
        # TODO(henrike): remove build_with_chromium==1 when the bots are using
        # Chromium's buildbots.
        ['build_with_chromium==1 and OS=="android" and gtest_target_type=="shared_library"', {
          'targets': [
            {
              'target_name': 'common_video_unittests_apk_target',
              'type': 'none',
              'dependencies': [
                '<(apk_tests_path):common_video_unittests_apk',
              ],
            },
          ],
        }],
        ['test_isolation_mode != "noop"', {
          'targets': [
            {
              'target_name': 'common_video_unittests_run',
              'type': 'none',
              'dependencies': [
                '<(import_isolate_path):import_isolate_gypi',
                'common_video_unittests',
              ],
              'includes': [
                'common_video_unittests.isolate',
              ],
              'sources': [
                'common_video_unittests.isolate',
              ],
            },
          ],
        }],
      ],
    }],  # include_tests
  ],
}
