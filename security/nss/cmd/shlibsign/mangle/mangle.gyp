# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
{
  'includes': [
    '../../../coreconf/config.gypi',
    '../../../cmd/platlibs.gypi'
  ],
  'targets': [
    {
      'target_name': 'mangle',
      'type': 'executable',
      'sources': [
        'mangle.c'
      ],
      'dependencies': [
        '<(DEPTH)/exports.gyp:nss_exports'
      ]
    }
  ],
  'target_defaults': {
    'defines': [
      'SHLIB_SUFFIX=\"<(dll_suffix)\"',
      'SHLIB_PREFIX=\"<(dll_prefix)\"'
    ]
  },
  'variables': {
    'module': 'nss',
    'use_static_libs': 1
  }
}