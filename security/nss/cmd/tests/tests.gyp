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
      'target_name': 'baddbdir',
      'type': 'executable',
      'sources': [
        'baddbdir.c'
      ],
      'dependencies': [
        '<(DEPTH)/exports.gyp:dbm_exports',
        '<(DEPTH)/exports.gyp:nss_exports'
      ]
    },
    {
      'target_name': 'conflict',
      'type': 'executable',
      'sources': [
        'conflict.c'
      ],
      'dependencies': [
        '<(DEPTH)/exports.gyp:dbm_exports',
        '<(DEPTH)/exports.gyp:nss_exports'
      ]
    },
    {
      'target_name': 'dertimetest',
      'type': 'executable',
      'sources': [
        'dertimetest.c'
      ],
      'dependencies': [
        '<(DEPTH)/exports.gyp:dbm_exports',
        '<(DEPTH)/exports.gyp:nss_exports'
      ]
    },
    {
      'target_name': 'encodeinttest',
      'type': 'executable',
      'sources': [
        'encodeinttest.c'
      ],
      'dependencies': [
        '<(DEPTH)/exports.gyp:dbm_exports',
        '<(DEPTH)/exports.gyp:nss_exports'
      ]
    },
    {
      'target_name': 'nonspr10',
      'type': 'executable',
      'sources': [
        'nonspr10.c'
      ],
      'dependencies': [
        '<(DEPTH)/exports.gyp:dbm_exports',
        '<(DEPTH)/exports.gyp:nss_exports'
      ]
    },
    {
      'target_name': 'remtest',
      'type': 'executable',
      'sources': [
        'remtest.c'
      ],
      'dependencies': [
        '<(DEPTH)/exports.gyp:dbm_exports',
        '<(DEPTH)/exports.gyp:nss_exports'
      ]
    },
    {
      'target_name': 'secmodtest',
      'type': 'executable',
      'sources': [
        'secmodtest.c'
      ],
      'dependencies': [
        '<(DEPTH)/exports.gyp:dbm_exports',
        '<(DEPTH)/exports.gyp:nss_exports'
      ]
    }
  ],
  'variables': {
    'module': 'nss'
  }
}