# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
{
  'includes': [
    '../../coreconf/config.gypi'
  ],
  'conditions': [
    ['use_system_sqlite==1', {
      'targets': [{
        'target_name': 'sqlite3',
        'type': 'none',
        'link_settings': {
          'libraries': ['<(sqlite_libs)'],
        },
      }],
    }, {
      'targets': [
        {
          'target_name': 'sqlite',
          'type': 'static_library',
          'sources': [
            'sqlite3.c'
          ],
          'dependencies': [
            '<(DEPTH)/exports.gyp:nss_exports'
          ]
        },
        {
          'target_name': 'sqlite3',
          'type': 'shared_library',
          'dependencies': [
            'sqlite'
          ],
          'variables': {
            'mapfile': 'sqlite.def'
          }
        }
      ],
      'target_defaults': {
        'defines': [
          'SQLITE_THREADSAFE=1'
        ],
        'cflags': [
          '-w',
        ],
        'xcode_settings': {
          'OTHER_CFLAGS': [
            '-w',
          ],
        },
      },
      'variables': {
        'module': 'nss'
      }
    }]
  ],
}
