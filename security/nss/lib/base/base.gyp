# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
{
  'includes': [
    '../../coreconf/config.gypi'
  ],
  'targets': [
    {
      'target_name': 'nssb',
      'type': 'static_library',
      'sources': [
        'arena.c',
        'error.c',
        'errorval.c',
        'hash.c',
        'hashops.c',
        'item.c',
        'libc.c',
        'list.c',
        'tracker.c',
        'utf8.c'
      ],
      'dependencies': [
        '<(DEPTH)/exports.gyp:nss_exports'
      ]
    }
  ],
  'variables': {
    'module': 'nss'
  }
}