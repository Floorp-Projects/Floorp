# Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  # Exclude the test target when building with chromium.
  'conditions': [   
    ['build_with_chromium==0', {
      'targets': [
        {
          'target_name': 'video_codecs_test_framework',
          'type': '<(library)',
          'dependencies': [
            '<(webrtc_root)/../test/test.gyp:test_support',
          ],
          'sources': [
            'mock/mock_packet_manipulator.h',
            'packet_manipulator.h',
            'packet_manipulator.cc',
            'predictive_packet_manipulator.h',
            'predictive_packet_manipulator.cc',
            'stats.h',
            'stats.cc',
            'videoprocessor.h',
            'videoprocessor.cc',
          ],
        },
        {
          'target_name': 'video_codecs_test_framework_unittests',
          'type': 'executable',
          'dependencies': [
            'video_codecs_test_framework',
            'webrtc_video_coding',
            '<(webrtc_root)/system_wrappers/source/system_wrappers.gyp:system_wrappers',
            '<(webrtc_root)/../testing/gmock.gyp:gmock',
            '<(webrtc_root)/../test/test.gyp:test_support_main',
          ],
          'sources': [
            'packet_manipulator_unittest.cc',
            'stats_unittest.cc',
            'videoprocessor_unittest.cc',
          ],
        },
        {
          'target_name': 'video_codecs_test_framework_integrationtests',
          'type': 'executable',
          'dependencies': [
            'video_codecs_test_framework',
            'webrtc_video_coding',
            'webrtc_vp8',
            '<(webrtc_root)/../testing/gtest.gyp:gtest',
            '<(webrtc_root)/../test/metrics.gyp:metrics',
            '<(webrtc_root)/../test/test.gyp:test_support_main',
          ],
          'sources': [
            'videoprocessor_integrationtest.cc',
          ],
        },
      ], # targets
    }], # build_with_chromium
  ], # conditions
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2: