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
     'target_name': 'video_processing_unittests',
      'type': 'executable',
      'dependencies': [
        'video_processing',
        'webrtc_utility',
        '<(webrtc_root)/../test/test.gyp:test_support_main',
        '<(webrtc_root)/../testing/gtest.gyp:gtest',
      ],
      'sources': [
        # headers
        'unit_test/unit_test.h',
        # sources
        'unit_test/brightness_detection_test.cc',
        'unit_test/color_enhancement_test.cc',
        'unit_test/content_metrics_test.cc',
        'unit_test/deflickering_test.cc',
        'unit_test/denoising_test.cc',
        'unit_test/unit_test.cc',
      ], # sources
    },
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
