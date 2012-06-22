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
      'target_name': 'resampler',
      'type': '<(library)',
      'dependencies': [
        'signal_processing',
      ],
      'include_dirs': [
        'include',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          'include',
        ],
      },
      'sources': [
        'include/resampler.h',
        'resampler.cc',
      ],
    },
  ], # targets
  'conditions': [
    ['build_with_chromium==0', {
      'targets' : [
        {
          'target_name': 'resampler_unittests',
          'type': 'executable',
          'dependencies': [
            'resampler',
            '<(webrtc_root)/../test/test.gyp:test_support_main',
            '<(webrtc_root)/../testing/gtest.gyp:gtest',
          ],
          'sources': [
            'resampler_unittest.cc',
          ],
        }, # resampler_unittests
      ], # targets
    }], # build_with_chromium
  ], # conditions
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
