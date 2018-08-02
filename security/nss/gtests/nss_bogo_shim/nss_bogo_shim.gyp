# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
{
  'includes': [
    '../../coreconf/config.gypi'
  ],
  'targets': [
    {
      'target_name': 'nss_bogo_shim',
      'type': 'executable',
      'sources': [
        'config.cc',
        'nss_bogo_shim.cc',
        'nsskeys.cc'
      ],
      'dependencies': [
        '<(DEPTH)/exports.gyp:nss_exports',
        '<(DEPTH)/lib/util/util.gyp:nssutil3',
        '<(DEPTH)/lib/sqlite/sqlite.gyp:sqlite3',
        '<(DEPTH)/gtests/google_test/google_test.gyp:gtest',
        '<(DEPTH)/lib/softoken/softoken.gyp:softokn',
        '<(DEPTH)/lib/smime/smime.gyp:smime',
        '<(DEPTH)/lib/ssl/ssl.gyp:ssl',
        '<(DEPTH)/lib/nss/nss.gyp:nss_static',
        '<(DEPTH)/cmd/lib/lib.gyp:sectool',
        '<(DEPTH)/lib/pkcs12/pkcs12.gyp:pkcs12',
        '<(DEPTH)/lib/pkcs7/pkcs7.gyp:pkcs7',
        '<(DEPTH)/lib/certhigh/certhigh.gyp:certhi',
        '<(DEPTH)/lib/cryptohi/cryptohi.gyp:cryptohi',
        '<(DEPTH)/lib/pk11wrap/pk11wrap.gyp:pk11wrap',
        '<(DEPTH)/lib/softoken/softoken.gyp:softokn',
        '<(DEPTH)/lib/certdb/certdb.gyp:certdb',
        '<(DEPTH)/lib/pki/pki.gyp:nsspki',
        '<(DEPTH)/lib/dev/dev.gyp:nssdev',
        '<(DEPTH)/lib/base/base.gyp:nssb',
        '<(DEPTH)/lib/freebl/freebl.gyp:freebl',
        '<(DEPTH)/lib/zlib/zlib.gyp:nss_zlib',
        '<(DEPTH)/lib/libpkix/libpkix.gyp:libpkix',
        '<(DEPTH)/cpputil/cpputil.gyp:cpputil',
      ],
      'conditions': [
        [ 'disable_dbm==0', {
          'dependencies': [
            '<(DEPTH)/lib/dbm/src/src.gyp:dbm',
          ],
        }],
      ],
    }
  ],
  'target_defaults': {
    'defines': [
      'NSS_USE_STATIC_LIBS'
    ],
    'include_dirs': [
      '../../lib/ssl'
    ],
  },
  'variables': {
    'module': 'nss',
    'use_static_libs': 1
  }
}
