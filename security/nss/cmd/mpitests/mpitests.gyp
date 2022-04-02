# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
{
  'includes': [
    '../../coreconf/config.gypi',
    '../../cmd/platlibs.gypi'
  ],
  'targets': [
    {
      'target_name': 'mpi_tests',
      'type': 'executable',
      'sources': [
        'mpi-test.c',
      ],
      'defines': [
        'NSS_USE_STATIC_LIBS'
      ],
      'dependencies': [
        '<(DEPTH)/exports.gyp:nss_exports',
        '<(DEPTH)/lib/util/util.gyp:nssutil3',
        '<(DEPTH)/lib/nss/nss.gyp:nss_static',
        '<(DEPTH)/lib/pk11wrap/pk11wrap.gyp:pk11wrap_static',
        '<(DEPTH)/lib/cryptohi/cryptohi.gyp:cryptohi',
        '<(DEPTH)/lib/certhigh/certhigh.gyp:certhi',
        '<(DEPTH)/lib/certdb/certdb.gyp:certdb',
        '<(DEPTH)/lib/base/base.gyp:nssb',
        '<(DEPTH)/lib/dev/dev.gyp:nssdev',
        '<(DEPTH)/lib/pki/pki.gyp:nsspki',
      ]
    }
  ],
  'target_defaults': {
    'include_dirs': [
      '<(DEPTH)/lib/freebl/mpi',
      '<(DEPTH)/lib/util',
    ],
    # This uses test builds and has to set defines for MPI.
    'conditions': [
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
    'module': 'nss',
    'use_static_libs': 1
  }
}
