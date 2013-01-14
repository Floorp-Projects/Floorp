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
      'target_name': 'paced_sender',
      'type': '<(library)',
      'dependencies': [
        '<(webrtc_root)/system_wrappers/source/system_wrappers.gyp:system_wrappers',
      ],
      'include_dirs': [
        'include',
      ],
      'sources': [
        'include/paced_sender.h',
        'paced_sender.cc',
      ],
    },
  ], # targets

  'conditions': [
    ['include_tests==1', {
      'targets' : [
        {
          'target_name': 'paced_sender_unittests',
          'type': 'executable',
          'dependencies': [
            'paced_sender',
            '<(webrtc_root)/test/test.gyp:test_support_main',
            '<(DEPTH)/testing/gmock.gyp:gmock',
            '<(DEPTH)/testing/gtest.gyp:gtest',
          ],
          'sources': [
            'paced_sender_unittest.cc',
           ],
         },
       ], # targets
    }], # include_tests
  ], # conditions

}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2
