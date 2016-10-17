# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
{
  'includes': [
    '../../../coreconf/config.gypi'
  ],
  'targets': [
    {
      'target_name': 'dbm',
      'type': 'static_library',
      'sources': [
        'db.c',
        'dirent.c',
        'h_bigkey.c',
        'h_func.c',
        'h_log2.c',
        'h_page.c',
        'hash.c',
        'hash_buf.c',
        'mktemp.c'
      ],
      'dependencies': [
        '<(DEPTH)/exports.gyp:dbm_exports'
      ]
    }
  ],
  'target_defaults': {
    'defines': [
      'STDC_HEADERS',
      'HAVE_STRERROR',
      'HAVE_SNPRINTF',
      'MEMMOVE',
      '__DBINTERFACE_PRIVATE'
    ]
  },
  'variables': {
    'module': 'dbm'
  }
}