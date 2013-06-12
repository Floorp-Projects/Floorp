# Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.
{
  'includes': [
    '../../build/common.gypi'
  ],
  'targets': [
    {
      'target_name': 'libtest',
      'type': 'static_library',
      'sources': [
        # Helper classes
        'include/bit_flip_encryption.h',
        'include/random_encryption.h',

        'helpers/bit_flip_encryption.cc',
        'helpers/random_encryption.cc',
      ],
    },
  ],
}
