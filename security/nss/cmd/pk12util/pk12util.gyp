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
      'target_name': 'pk12util',
      'type': 'executable',
      'sources': [
        'pk12util.c'
      ],
      'dependencies': [
        '<(DEPTH)/exports.gyp:dbm_exports',
        '<(DEPTH)/exports.gyp:nss_exports'
      ]
    }
  ],
  'target_defaults': {
    'defines': [
      'NSPR20'
    ]
  },
  'variables': {
    'module': 'nss'
  }
}