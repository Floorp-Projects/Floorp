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
        'pk11_aes_cmac_unittest.cc',
        'pk11_aes_gcm_unittest.cc',
        'pk11_aeskeywrap_unittest.cc',
        'pk11_aeskeywrapkwp_unittest.cc',
        'pk11_aeskeywrappad_unittest.cc',
        'pk11_cbc_unittest.cc',
        'pk11_chacha20poly1305_unittest.cc',
        'pk11_cipherop_unittest.cc',
        'pk11_curve25519_unittest.cc',
        'pk11_der_private_key_import_unittest.cc',
        'pk11_des_unittest.cc',
        'pk11_dsa_unittest.cc',
        'pk11_ecdsa_unittest.cc',
        'pk11_ecdh_unittest.cc',
        'pk11_encrypt_derive_unittest.cc',
        'pk11_find_certs_unittest.cc',
        'pk11_hkdf_unittest.cc',
        'pk11_hmac_unittest.cc',
        'pk11_hpke_unittest.cc',
        'pk11_ike_unittest.cc',
        'pk11_import_unittest.cc',
        'pk11_kbkdf.cc',
        'pk11_keygen.cc',
        'pk11_key_unittest.cc',
        'pk11_module_unittest.cc',
        'pk11_pbkdf2_unittest.cc',
        'pk11_prf_unittest.cc',
        'pk11_prng_unittest.cc',
        'pk11_rsaencrypt_unittest.cc',
        'pk11_rsaoaep_unittest.cc',
        'pk11_rsapkcs1_unittest.cc',
        'pk11_rsapss_unittest.cc',
        'pk11_seed_cbc_unittest.cc',
        'pk11_signature_test.cc',
        '<(DEPTH)/gtests/common/gtests.cc'
      ],
      'dependencies': [
        '<(DEPTH)/exports.gyp:nss_exports',
        '<(DEPTH)/cpputil/cpputil.gyp:cpputil',
        '<(DEPTH)/gtests/google_test/google_test.gyp:gtest',
        '<(DEPTH)/lib/util/util.gyp:nssutil3',
      ],
      'conditions': [
        [ 'static_libs==1', {
          'dependencies': [
            '<(DEPTH)/lib/base/base.gyp:nssb',
            '<(DEPTH)/lib/certdb/certdb.gyp:certdb',
            '<(DEPTH)/lib/certhigh/certhigh.gyp:certhi',
            '<(DEPTH)/lib/cryptohi/cryptohi.gyp:cryptohi',
            '<(DEPTH)/lib/dev/dev.gyp:nssdev',
            '<(DEPTH)/lib/nss/nss.gyp:nss_static',
            '<(DEPTH)/lib/pk11wrap/pk11wrap.gyp:pk11wrap_static',
            '<(DEPTH)/lib/pki/pki.gyp:nsspki',
            '<(DEPTH)/lib/ssl/ssl.gyp:ssl',
            '<(DEPTH)/lib/libpkix/libpkix.gyp:libpkix',
          ],
        }, {
          'dependencies': [
            '<(DEPTH)/lib/nss/nss.gyp:nss3',
            '<(DEPTH)/lib/ssl/ssl.gyp:ssl3',
          ],
        }],
      ],
    }
  ],
  'target_defaults': {
    'defines': [
      'DLL_PREFIX=\"<(dll_prefix)\"',
      'DLL_SUFFIX=\"<(dll_suffix)\"'
    ]
  },
  'variables': {
    'module': 'nss'
  }
}
