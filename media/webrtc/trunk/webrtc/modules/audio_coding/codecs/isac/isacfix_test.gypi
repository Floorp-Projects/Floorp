# Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'targets': [
    # kenny
    {
      'target_name': 'iSACFixtest',
      'type': 'executable',
      'dependencies': [
        'iSACFix',
        '<(webrtc_root)/test/test.gyp:test_support',
      ],
      'include_dirs': [
        './fix/test',
        './fix/interface',
      ],
      'sources': [
        './fix/test/kenny.cc',
      ],
    },
  ],
}

# TODO(kma): Add bit-exact test for iSAC-fix.
