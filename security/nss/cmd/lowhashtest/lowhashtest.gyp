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
      'target_name': 'lowhashtest',
      'type': 'executable',
      'sources': [
        'lowhashtest.c'
      ],
      'dependencies': [
        '<(DEPTH)/exports.gyp:dbm_exports',
        '<(DEPTH)/exports.gyp:nss_exports',
        '<(DEPTH)/lib/freebl/freebl.gyp:freebl3',
        '<(DEPTH)/cmd/lib/lib.gyp:sectool'
      ]
    }
  ],
  'target_defaults': {
    'include_dirs': [
      '../../nss/lib/freebl'
    ]
  },
  'variables': {
    'module': 'nss'
  }
}