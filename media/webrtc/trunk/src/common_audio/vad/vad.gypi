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
      'target_name': 'vad',
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
        'include/webrtc_vad.h',
        'webrtc_vad.c',
        'vad_core.c',
        'vad_core.h',
        'vad_filterbank.c',
        'vad_filterbank.h',
        'vad_gmm.c',
        'vad_gmm.h',
        'vad_sp.c',
        'vad_sp.h',
      ],
    },
  ], # targets
   'conditions': [
    ['build_with_chromium==0', {
      'targets' : [
        {
          'target_name': 'vad_unittests',
          'type': 'executable',
          'dependencies': [
            'vad',
            '<(webrtc_root)/../test/test.gyp:test_support_main',
            '<(webrtc_root)/../testing/gtest.gyp:gtest',
          ],
          'sources': [
            'vad_core_unittest.cc',
            'vad_filterbank_unittest.cc',
            'vad_gmm_unittest.cc',
            'vad_sp_unittest.cc',
            'vad_unittest.cc',
            'vad_unittest.h',
          ],
        }, # vad_unittests
      ], # targets
    }], # build_with_chromium
  ], # conditions
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
