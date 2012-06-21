# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'ssl',
      'type': 'none',
      'direct_dependent_settings': {
        'defines': [
          'USE_OPENSSL',
        ],
        'include_dirs': [
          '../../third_party/openssl/openssl/include',
          '../../third_party/openssl/config/android',
        ],
      },
      'dependencies': [
        '../../third_party/openssl/openssl.gyp:openssl',
      ],
    },
  ],
}
