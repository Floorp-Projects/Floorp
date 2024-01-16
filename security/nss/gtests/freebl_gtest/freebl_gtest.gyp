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
      # Dependencies for tests.
      'target_name': 'freebl_gtest_deps',
      'type': 'none',
      'dependencies': [
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
    },
    {
      'target_name': 'freebl_gtest',
      'type': 'executable',
      'sources': [
        'blake2b_unittest.cc',
        'shake_unittest.cc',
        'cmac_unittests.cc',
        'dh_unittest.cc',
        'ecl_unittest.cc',
        'ghash_unittest.cc',
        'kyber_unittest.cc',
        'mpi_unittest.cc',
        'prng_kat_unittest.cc',
        'rsa_unittest.cc',
        '<(DEPTH)/gtests/common/gtests.cc'
      ],
      'dependencies': [
        'freebl_gtest_deps',
        '<(DEPTH)/exports.gyp:nss_exports',
      ],
      'conditions': [
      [ 'cc_is_gcc==1 and (target_arch=="ia32" or target_arch=="x64")', {
         'cflags_cc': [
          '-msse2',
          ],
        }],
      ],
    },
  ],
  'target_defaults': {
    'include_dirs': [
      '<(DEPTH)/lib/freebl/ecl',
      '<(DEPTH)/lib/freebl/mpi',
      '<(DEPTH)/lib/freebl/',
      '<(DEPTH)/lib/ssl/',
      '<(DEPTH)/lib/util/',
      '<(DEPTH)/lib/certdb/',
      '<(DEPTH)/lib/cryptohi/',
      '<(DEPTH)/lib/pk11wrap/',
    ],
    'defines': [
      'NSS_USE_STATIC_LIBS',
    ],
    # For static builds we have to set MPI defines.
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
      [ 'OS=="win"', {
        'libraries': [
          'advapi32.lib',
        ],
      }],
    ],
  },
  'variables': {
    'module': 'nss'
  }
}
