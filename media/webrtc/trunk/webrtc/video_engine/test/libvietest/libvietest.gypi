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
      'type': 'static_library',
      'dependencies': [
        '<(webrtc_root)/common.gyp:webrtc_common',
        '<(webrtc_root)/system_wrappers/system_wrappers.gyp:system_wrappers',
        '<(DEPTH)/testing/gtest.gyp:gtest',
        '<(webrtc_root)/test/test.gyp:test_support',
        'video_engine_core',
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
        'include/tb_capture_device.h',
        'include/tb_external_transport.h',
        'include/tb_I420_codec.h',
        'include/tb_interfaces.h',
        'include/tb_video_channel.h',

        'testbed/tb_capture_device.cc',
        'testbed/tb_external_transport.cc',
        'testbed/tb_I420_codec.cc',
        'testbed/tb_interfaces.cc',
        'testbed/tb_video_channel.cc',
      ],
      # Disable warnings to enable Win64 build, issue 1323.
      'msvs_disabled_warnings': [
        4267,  # size_t to int truncation.
      ],
    },
  ],
}
