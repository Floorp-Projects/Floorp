# Copyright 2011 The LibYuv Project Authors. All rights reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'targets': [
    {
      'target_name': 'libyuv_unittest',
      'type': 'executable',
      'dependencies': [
        'libyuv.gyp:libyuv',
        # The tests are based on gtest
        'testing/gtest.gyp:gtest',
        'testing/gtest.gyp:gtest_main',
      ],
      'defines': [
        'LIBYUV_SVNREVISION="<!(svnversion -n)"',
        # 'LIBYUV_USING_SHARED_LIBRARY',
      ],
      'sources': [
        # headers
        'unit_test/unit_test.h',

        # sources
        'unit_test/compare_test.cc',
        'unit_test/cpu_test.cc',
        'unit_test/planar_test.cc',
        'unit_test/rotate_argb_test.cc',
        'unit_test/rotate_test.cc',
        'unit_test/scale_argb_test.cc',
        'unit_test/scale_test.cc',
        'unit_test/unit_test.cc',
        'unit_test/version_test.cc',
      ],
      'conditions': [
        ['OS=="linux"', {
          'cflags': [
            '-fexceptions',
          ],
        }],
      ], # conditions
    },

    {
      'target_name': 'compare',
      'type': 'executable',
      'dependencies': [
        'libyuv.gyp:libyuv',
      ],
      'sources': [
        # sources
        'util/compare.cc',
      ],
      'conditions': [
        ['OS=="linux"', {
          'cflags': [
            '-fexceptions',
          ],
        }],
      ], # conditions
    },

  ], # targets
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
