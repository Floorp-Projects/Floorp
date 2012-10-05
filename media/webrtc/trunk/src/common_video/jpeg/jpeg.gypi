# Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'variables': {
    'use_libjpeg_turbo%': '<(use_libjpeg_turbo)',
    'conditions': [
      ['use_libjpeg_turbo==1', {
        'libjpeg_include_dir%': [ '<(DEPTH)/third_party/libjpeg_turbo', ],
      }, {
        'libjpeg_include_dir%': [ '<(DEPTH)/third_party/libjpeg', ],
      }],
    ],
  },
  'targets': [
    {
      'target_name': 'webrtc_jpeg',
      'type': '<(library)',
      'dependencies': [
        'webrtc_libyuv',
      ],
      'include_dirs': [
        'include',
        '<(webrtc_root)',
        '<(webrtc_root)/common_video/interface',
# added for mozilla for use_system_libjpeg
        '$(DIST)/include',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          'include',
          '<(webrtc_root)/common_video/interface',
# added for mozilla for use_system_libjpeg
        '$(builddir)/../../../../../../dist/include',
        ],
      },
      'conditions': [
        ['build_libjpeg==1', {
          'dependencies': [
            '<(libjpeg_gyp_path):libjpeg',
          ],
        }, {
          # Need to add a directory normally exported by libjpeg.gyp.
          'include_dirs': [ '<(libjpeg_include_dir)' ],
        }],
      ],
      'sources': [
        'include/jpeg.h',
        'data_manager.cc',
        'data_manager.h',
        'jpeg.cc',
      ],
    },
  ], # targets
  'conditions': [
    ['include_tests==1', {
      'targets': [
        {
          'target_name': 'jpeg_unittests',
          'type': 'executable',
          'dependencies': [
             'webrtc_jpeg',
             '<(DEPTH)/testing/gtest.gyp:gtest',
             '<(webrtc_root)/test/test.gyp:test_support_main',
          ],
          'sources': [
            'jpeg_unittest.cc',
          ],
        },
      ] # targets
    }], # include_tests
  ], # conditions
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
