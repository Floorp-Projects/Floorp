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
      'target_name': 'softoken_gtest',
      'type': 'executable',
      'sources': [
        'softoken_gtest.cc',
        'softoken_nssckbi_testlib_gtest.cc',
      ],
      'dependencies': [
        '<(DEPTH)/exports.gyp:nss_exports',
        '<(DEPTH)/cpputil/cpputil.gyp:cpputil',
        '<(DEPTH)/lib/util/util.gyp:nssutil3',
        '<(DEPTH)/gtests/google_test/google_test.gyp:gtest',
      ],
      'conditions': [
        [ 'static_libs==1', {
          'dependencies': [
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
        }, {
          'dependencies': [
            '<(DEPTH)/lib/nss/nss.gyp:nss3',
            '<(DEPTH)/lib/ssl/ssl.gyp:ssl3',
            '<(DEPTH)/lib/sqlite/sqlite.gyp:sqlite3',
          ],
        }],
      ],
    }
  ],
  'target_defaults': {
    'include_dirs': [
      '../../lib/util'
    ],
    'defines': [
      'DLL_PREFIX=\"<(dll_prefix)\"',
      'DLL_SUFFIX=\"<(dll_suffix)\"'
    ]
  },
  'variables': {
    'module': 'nss'
  }
}
