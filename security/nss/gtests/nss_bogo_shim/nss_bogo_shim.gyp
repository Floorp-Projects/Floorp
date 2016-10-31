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
        '<(DEPTH)/lib/nss/nss.gyp:nss_static',
        '<(DEPTH)/lib/pk11wrap/pk11wrap.gyp:pk11wrap',
        '<(DEPTH)/lib/certhigh/certhigh.gyp:certhi',
        '<(DEPTH)/lib/zlib/zlib.gyp:nss_zlib'
      ],
      'conditions': [
        [ 'disable_dbm==0', {
          'dependencies': [
            '<(DEPTH)/lib/dbm/src/src.gyp:dbm',
          ],
        }],
        [ 'disable_libpkix==0', {
          'dependencies': [
            '<(DEPTH)/lib/libpkix/pkix/certsel/certsel.gyp:pkixcertsel',
            '<(DEPTH)/lib/libpkix/pkix/checker/checker.gyp:pkixchecker',
            '<(DEPTH)/lib/libpkix/pkix/crlsel/crlsel.gyp:pkixcrlsel',
            '<(DEPTH)/lib/libpkix/pkix/params/params.gyp:pkixparams',
            '<(DEPTH)/lib/libpkix/pkix/results/results.gyp:pkixresults',
            '<(DEPTH)/lib/libpkix/pkix/store/store.gyp:pkixstore',
            '<(DEPTH)/lib/libpkix/pkix/top/top.gyp:pkixtop',
            '<(DEPTH)/lib/libpkix/pkix/util/util.gyp:pkixutil',
            '<(DEPTH)/lib/libpkix/pkix_pl_nss/system/system.gyp:pkixsystem',
            '<(DEPTH)/lib/libpkix/pkix_pl_nss/module/module.gyp:pkixmodule',
            '<(DEPTH)/lib/libpkix/pkix_pl_nss/pki/pki.gyp:pkixpki',
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
