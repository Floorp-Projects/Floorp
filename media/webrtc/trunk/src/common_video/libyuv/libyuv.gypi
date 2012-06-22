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
          'include_dirs': [ '<(DEPTH)/third_party/libyuv/include', ],
        }],
      ],
      'sources': [
        'include/libyuv.h',
        'include/scaler.h',
        'libyuv.cc',
        'scaler.cc',
      ],
      'include_dirs': [
        '<(DEPTH)',
      ],
    },
  ], # targets
  'conditions': [
    ['build_with_chromium==0', {
      'targets': [
        {
          'target_name': 'libyuv_unittests',
          'type': 'executable',
          'dependencies': [
            'webrtc_libyuv',
            '<(webrtc_root)/../testing/gtest.gyp:gtest',
            '<(webrtc_root)/system_wrappers/source/system_wrappers.gyp:system_wrappers',
            '<(webrtc_root)/../test/test.gyp:test_support_main',
          ],
          'sources': [
            'libyuv_unittest.cc',
            'scaler_unittest.cc', 
          ], 
        },
      ], # targets
    }], # build_with_chromium
  ], # conditions
}
