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
      'target_name': 'webrtc_libyuv',
      'type': '<(library)',
      'conditions': [
        ['build_libyuv==1', {
          'dependencies': [
            '<(DEPTH)/third_party/libyuv/libyuv.gyp:libyuv'
          ],
        }, {
          # Need to add a directory normally exported by libyuv.gyp.
          'include_dirs': [ '<(libyuv_dir)/include', ],
        }],
      ],
      'sources': [
        'include/webrtc_libyuv.h',
        'include/scaler.h',
        'webrtc_libyuv.cc',
        'scaler.cc',
      ],
    },
  ], # targets
  'conditions': [
    ['include_tests==1', {
      'targets': [
        {
          'target_name': 'libyuv_unittests',
          'type': 'executable',
          'dependencies': [
            'webrtc_libyuv',
            '<(DEPTH)/testing/gtest.gyp:gtest',
            '<(webrtc_root)/system_wrappers/source/system_wrappers.gyp:system_wrappers',
            '<(webrtc_root)/test/test.gyp:test_support_main',
          ],
          'sources': [
            'libyuv_unittest.cc',
            'scaler_unittest.cc',
          ],
        },
      ], # targets
    }], # include_tests
  ], # conditions
}
