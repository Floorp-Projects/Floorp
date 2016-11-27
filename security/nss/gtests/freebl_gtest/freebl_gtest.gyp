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
        '<(DEPTH)/gtests/common/gtests.cc'
      ],
      'dependencies': [
        '<(DEPTH)/exports.gyp:nss_exports',
        '<(DEPTH)/lib/freebl/freebl.gyp:<(freebl_name)',
        '<(DEPTH)/gtests/google_test/google_test.gyp:gtest',
      ],
      'defines': [
        'CT_VERIF',
      ],
    }
  ],
  'target_defaults': {
    'include_dirs': [
      '<(DEPTH)/gtests/google_test/gtest/include',
      '<(DEPTH)/gtests/common',
      '<(DEPTH)/lib/freebl/mpi',
    ]
  },
  'variables': {
    'module': 'nss'
  }
}
