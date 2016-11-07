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
      'target_name': 'ssl_gtest',
      'type': 'executable',
      'sources': [
        'libssl_internals.c',
        'ssl_0rtt_unittest.cc',
        'ssl_agent_unittest.cc',
        'ssl_auth_unittest.cc',
        'ssl_cert_ext_unittest.cc',
        'ssl_ciphersuite_unittest.cc',
        'ssl_damage_unittest.cc',
        'ssl_dhe_unittest.cc',
        'ssl_drop_unittest.cc',
        'ssl_ecdh_unittest.cc',
        'ssl_ems_unittest.cc',
        'ssl_extension_unittest.cc',
        'ssl_fuzz_unittest.cc',
        'ssl_gtest.cc',
        'ssl_hrr_unittest.cc',
        'ssl_loopback_unittest.cc',
        'ssl_record_unittest.cc',
        'ssl_resumption_unittest.cc',
        'ssl_skip_unittest.cc',
        'ssl_staticrsa_unittest.cc',
        'ssl_v2_client_hello_unittest.cc',
        'ssl_version_unittest.cc',
        'test_io.cc',
        'tls_agent.cc',
        'tls_connect.cc',
        'tls_filter.cc',
        'tls_hkdf_unittest.cc',
        'tls_parser.cc'
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
    'include_dirs': [
      '../../gtests/google_test/gtest/include',
      '../../gtests/common',
      '../../lib/ssl'
    ],
    'defines': [
      'NSS_USE_STATIC_LIBS'
    ]
  },
  'variables': {
    'module': 'nss',
    'use_static_libs': 1
  }
}
