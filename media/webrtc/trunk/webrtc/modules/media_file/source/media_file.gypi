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
      'target_name': 'media_file',
      'type': '<(library)',
      'dependencies': [
        'webrtc_utility',
        '<(webrtc_root)/system_wrappers/source/system_wrappers.gyp:system_wrappers',
      ],
      'defines': [
        'WEBRTC_MODULE_UTILITY_VIDEO', # for compiling support for video recording
      ],
      'include_dirs': [
        '../interface',
        '../../interface',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '../interface',
          '../../interface',
        ],
      },
      'sources': [
        '../interface/media_file.h',
        '../interface/media_file_defines.h',
        'avi_file.cc',
        'avi_file.h',
        'media_file_impl.cc',
        'media_file_impl.h',
        'media_file_utility.cc',
        'media_file_utility.h',
      ], # source
    },
  ], # targets
  'conditions': [
    ['include_tests==1', {
      'targets': [
        {
          'target_name': 'media_file_unittests',
          'type': 'executable',
          'dependencies': [
            'media_file',
            '<(DEPTH)/testing/gtest.gyp:gtest',
            '<(webrtc_root)/test/test.gyp:test_support_main',
          ],
          'sources': [
            'media_file_unittest.cc',
          ],
        }, # media_file_unittests
      ], # targets
    }], # include_tests
  ], # conditions
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
