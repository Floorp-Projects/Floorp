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
      'target_name': 'pk11_gtest',
      'type': 'executable',
      'sources': [
        'pk11_aeskeywrap_unittest.cc',
        'pk11_chacha20poly1305_unittest.cc',
        'pk11_pbkdf2_unittest.cc',
        'pk11_prf_unittest.cc',
        'pk11_rsapss_unittest.cc',
        '<(DEPTH)/external_tests/common/gtests.cc'
      ],
      'dependencies': [
        '<(DEPTH)/exports.gyp:nss_exports',
        '<(DEPTH)/lib/nss/nss.gyp:nss3',
        '<(DEPTH)/lib/util/util.gyp:nssutil3',
        '<(DEPTH)/lib/smime/smime.gyp:smime3',
        '<(DEPTH)/lib/ssl/ssl.gyp:ssl3',
        '<(DEPTH)/external_tests/google_test/google_test.gyp:gtest',
        '<(DEPTH)/cmd/lib/lib.gyp:sectool'
      ]
    }
  ],
  'target_defaults': {
    'include_dirs': [
      '../../external_tests/google_test/gtest/include',
      '../../external_tests/common'
    ]
  },
  'variables': {
    'module': 'nss'
  }
}
