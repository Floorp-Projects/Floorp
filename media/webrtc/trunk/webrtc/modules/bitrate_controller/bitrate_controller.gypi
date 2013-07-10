# Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'targets': [
    {
      'target_name': 'bitrate_controller',
      'type': 'static_library',
      'dependencies': [
        '<(webrtc_root)/system_wrappers/source/system_wrappers.gyp:system_wrappers',
      ],
      'include_dirs': [
        '../interface',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '../interface',
          '<(webrtc_root)/modules/rtp_rtcp/interface',
        ],
      },
      'sources': [
        'bitrate_controller_impl.cc',
        'bitrate_controller_impl.h',
        'include/bitrate_controller.h',
        'send_side_bandwidth_estimation.cc',
        'send_side_bandwidth_estimation.h',
      ],
      # TODO(jschuh): Bug 1348: fix size_t to int truncations.
      'msvs_disabled_warnings': [ 4267, ],
    },
  ], # targets

  'conditions': [
    ['include_tests==1', {
      'targets' : [
        {
          'target_name': 'bitrate_controller_unittests',
          'type': 'executable',
          'dependencies': [
            'bitrate_controller',
            '<(webrtc_root)/test/test.gyp:test_support_main',
            '<(DEPTH)/testing/gtest.gyp:gtest',
          ],
          'sources': [
            'bitrate_controller_unittest.cc',
           ],
         },
       ], # targets
    }], # include_tests
  ], # conditions

}
