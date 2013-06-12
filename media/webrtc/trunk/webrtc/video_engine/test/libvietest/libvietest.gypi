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
      'target_name': 'libvietest',
      'type': '<(library)',
      'dependencies': [
        '<(webrtc_root)/system_wrappers/source/system_wrappers.gyp:system_wrappers',
        '<(DEPTH)/testing/gtest.gyp:gtest',
        '<(webrtc_root)/test/test.gyp:test_support',
        'video_engine_core',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          'include/',
        ]
      },
      'include_dirs': [
        'include/',
        'helpers/',
      ],
      'sources': [
        # Helper classes
        'include/vie_external_render_filter.h',
        'include/vie_fake_camera.h',
        'include/vie_file_capture_device.h',
        'include/vie_to_file_renderer.h',

        'helpers/vie_fake_camera.cc',
        'helpers/vie_file_capture_device.cc',
        'helpers/vie_to_file_renderer.cc',

        # Testbed classes
        'include/fake_network_pipe.h',
        'include/tb_capture_device.h',
        'include/tb_external_transport.h',
        'include/tb_I420_codec.h',
        'include/tb_interfaces.h',
        'include/tb_video_channel.h',

        'testbed/fake_network_pipe.cc',
        'testbed/tb_capture_device.cc',
        'testbed/tb_external_transport.cc',
        'testbed/tb_I420_codec.cc',
        'testbed/tb_interfaces.cc',
        'testbed/tb_video_channel.cc',
      ],
    },
  ],
  'conditions': [
    ['include_tests==1', {
      'targets': [
        {
          'target_name': 'libvietest_unittests',
          'type': 'executable',
          'dependencies': [
            'libvietest',
            '<(DEPTH)/testing/gtest.gyp:gtest',
            '<(DEPTH)/testing/gmock.gyp:gmock',
            '<(webrtc_root)/test/test.gyp:test_support_main',
          ],
          'include_dirs': [
            'include/',
          ],
          'sources': [
            'testbed/fake_network_pipe_unittest.cc',
          ],
        },
      ], #targets
    }], # include_tests
  ], # conditions
}
