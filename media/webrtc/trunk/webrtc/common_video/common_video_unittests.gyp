# Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
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
      'target_name': 'common_video_unittests',
      'type': '<(gtest_target_type)',
      'dependencies': [
         '<(webrtc_root)/common_video/common_video.gyp:common_video',
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
            'common_video_unittests',
          ],
          'includes': [
            '../build/isolate.gypi',
            'common_video_unittests.isolate',
          ],
          'sources': [
            'common_video_unittests.isolate',
          ],
        },
      ],
    }],
  ],
}
