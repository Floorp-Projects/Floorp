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
      'dependencies': [
        '<(DEPTH)/lib/freebl/freebl.gyp:<(freebl_name)',
      ]
    }
  ],
  'target_defaults': {
    'include_dirs': [
      '<(DEPTH)/lib/freebl/mpi',
      '<(DEPTH)/lib/util',
    ]
  },
  'variables': {
    'module': 'nss'
  }
}
