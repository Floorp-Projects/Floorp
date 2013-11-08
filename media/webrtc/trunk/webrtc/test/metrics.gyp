# Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
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
      # The metrics code must be kept in its own GYP file in order to
      # avoid a circular dependency error due to the dependency on libyuv.
      # If the code would be put in test.gyp a circular dependency error during
      # GYP generation would occur, because the libyuv.gypi unittest target
      # depends on test_support_main. See issue #160 for more info.
      'target_name': 'metrics',
      'type': 'static_library',
      'dependencies': [
        '<(webrtc_root)/common_video/common_video.gyp:common_video',
        '<(webrtc_root)/system_wrappers/source/system_wrappers.gyp:system_wrappers',
      ],
      'include_dirs': [
        '.',
      ],
      'sources': [
        'testsupport/metrics/video_metrics.h',
        'testsupport/metrics/video_metrics.cc',
      ],
    },
    {
      'target_name': 'metrics_unittests',
      'type': '<(gtest_target_type)',
      'dependencies': [
        'metrics',
        '<(webrtc_root)/test/test.gyp:test_support_main',
        '<(DEPTH)/testing/gtest.gyp:gtest',
      ],
      'sources': [
        'testsupport/metrics/video_metrics_unittest.cc',
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
  ], # targets
  'conditions': [
    # TODO(henrike): remove build_with_chromium==1 when the bots are using
    # Chromium's buildbots.
    ['include_tests==1 and build_with_chromium==1 and OS=="android" and gtest_target_type=="shared_library"', {
      'targets': [
        {
          'target_name': 'metrics_unittests_apk_target',
          'type': 'none',
          'dependencies': [
            '<(apk_tests_path):metrics_unittests_apk',
          ],
        },
      ],
    }],
    ['test_isolation_mode != "noop"', {
      'targets': [
        {
          'target_name': 'metrics_unittests_run',
          'type': 'none',
          'dependencies': [
            '<(import_isolate_path):import_isolate_gypi',
            'metrics_unittests',
          ],
          'includes': [
            'metrics_unittests.isolate',
          ],
          'sources': [
            'metrics_unittests.isolate',
          ],
        },
      ],
    }],
  ],
}
