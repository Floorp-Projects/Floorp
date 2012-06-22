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
          'target_name': 'test_framework',
          'type': '<(library)',

          'dependencies': [
            '<(webrtc_root)/../test/metrics.gyp:metrics',
            '<(webrtc_root)/../test/test.gyp:test_support',
            '<(webrtc_root)/../testing/gtest.gyp:gtest',
            '<(webrtc_root)/system_wrappers/source/system_wrappers.gyp:system_wrappers',
            '<(webrtc_root)/common_video/common_video.gyp:webrtc_libyuv',
          ],

          'include_dirs': [
            '../interface',
            '<(webrtc_root)/../testing/gtest/include',
            '../../../../common_video/interface',
          ],

          'direct_dependent_settings': {
            'include_dirs': [
              '../interface',
            ],
          },

          'sources': [
            # header files
            'benchmark.h',
            'normal_async_test.h',
            'normal_test.h',
            'packet_loss_test.h',
            'performance_test.h',
            'test.h',
            'unit_test.h',
            'video_buffer.h',
            'video_source.h',

            # source files
            'benchmark.cc',
            'normal_async_test.cc',
            'normal_test.cc',
            'packet_loss_test.cc',
            'performance_test.cc',
            'test.cc',
            'unit_test.cc',
            'video_buffer.cc',
            'video_source.cc',
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
