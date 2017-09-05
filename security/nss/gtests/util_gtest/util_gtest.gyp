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
        'util_utf8_unittest.cc',
        'util_b64_unittest.cc',
        'util_pkcs11uri_unittest.cc',
        '<(DEPTH)/gtests/common/gtests.cc',
      ],
      'dependencies': [
        '<(DEPTH)/exports.gyp:nss_exports',
        '<(DEPTH)/gtests/google_test/google_test.gyp:gtest',
        '<(DEPTH)/lib/util/util.gyp:nssutil',
        '<(DEPTH)/lib/nss/nss.gyp:nss_static',
        '<(DEPTH)/lib/pk11wrap/pk11wrap.gyp:pk11wrap_static',
        '<(DEPTH)/lib/cryptohi/cryptohi.gyp:cryptohi',
        '<(DEPTH)/lib/certhigh/certhigh.gyp:certhi',
        '<(DEPTH)/lib/certdb/certdb.gyp:certdb',
        '<(DEPTH)/lib/base/base.gyp:nssb',
        '<(DEPTH)/lib/dev/dev.gyp:nssdev',
        '<(DEPTH)/lib/pki/pki.gyp:nsspki',
        '<(DEPTH)/lib/ssl/ssl.gyp:ssl',
        '<(DEPTH)/lib/libpkix/libpkix.gyp:libpkix',
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
