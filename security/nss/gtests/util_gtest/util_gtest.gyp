# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
{
  'includes': [
    '../../coreconf/config.gypi',
    '../common/gtest.gypi',
  ],
  'targets': [
    {
      'target_name': 'util_gtest',
      'type': 'executable',
      'sources': [
        'util_aligned_malloc_unittest.cc',
        'util_b64_unittest.cc',
        'util_gtests.cc',
        'util_memcmpzero_unittest.cc',
        'util_pkcs11uri_unittest.cc',
        'util_utf8_unittest.cc',
      ],
      'dependencies': [
        '<(DEPTH)/exports.gyp:nss_exports',
        '<(DEPTH)/gtests/google_test/google_test.gyp:gtest',
        '<(DEPTH)/lib/util/util.gyp:nssutil',
      ],
      'conditions': [
        [ 'OS=="win"', {
          'libraries': [
            'advapi32.lib',
          ],
        }],
      ],
      'defines': [
        'NSS_USE_STATIC_LIBS'
      ],
    }
  ],
  'target_defaults': {
    'include_dirs': [
      '../../lib/util'
    ]
  },
  'variables': {
    'module': 'nss'
  }
}
