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
      'target_name': 'freebl_gtest',
      'type': 'executable',
      'sources': [
        'mpi_unittest.cc',
        'dh_unittest.cc',
        'ecl_unittest.cc',
        'ghash_unittest.cc',
        '<(DEPTH)/gtests/common/gtests.cc'
      ],
      'dependencies': [
        '<(DEPTH)/exports.gyp:nss_exports',
        '<(DEPTH)/lib/util/util.gyp:nssutil3',
        '<(DEPTH)/gtests/google_test/google_test.gyp:gtest',
        '<(DEPTH)/lib/nss/nss.gyp:nss_static',
        '<(DEPTH)/lib/pk11wrap/pk11wrap.gyp:pk11wrap_static',
        '<(DEPTH)/lib/cryptohi/cryptohi.gyp:cryptohi',
        '<(DEPTH)/lib/certhigh/certhigh.gyp:certhi',
        '<(DEPTH)/lib/certdb/certdb.gyp:certdb',
        '<(DEPTH)/lib/base/base.gyp:nssb',
        '<(DEPTH)/lib/dev/dev.gyp:nssdev',
        '<(DEPTH)/lib/pki/pki.gyp:nsspki',
        '<(DEPTH)/lib/ssl/ssl.gyp:ssl',
      ],
    },
    {
      'target_name': 'prng_gtest',
      'type': 'executable',
      'sources': [
        'prng_kat_unittest.cc',
      ],
      'dependencies': [
        '<(DEPTH)/exports.gyp:nss_exports',
        '<(DEPTH)/lib/util/util.gyp:nssutil3',
        '<(DEPTH)/gtests/google_test/google_test.gyp:gtest',
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
    },
  ],
  'target_defaults': {
    'include_dirs': [
      '<(DEPTH)/lib/freebl/mpi',
      '<(DEPTH)/lib/freebl/',
    ],
    # For test builds we have to set MPI defines.
    'conditions': [
      [ 'ct_verif==1', {
        'defines': [
          'CT_VERIF',
        ],
      }],
      [ 'target_arch=="ia32"', {
        'defines': [
          'MP_USE_UINT_DIGIT',
          'MP_ASSEMBLY_MULTIPLY',
          'MP_ASSEMBLY_SQUARE',
          'MP_ASSEMBLY_DIV_2DX1D',
        ],
      }],
    ],
  },
  'variables': {
    'module': 'nss'
  }
}
