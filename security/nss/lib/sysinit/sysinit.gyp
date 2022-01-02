# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
{
  'includes': [
    '../../coreconf/config.gypi',
  ],
  'targets': [
    {
      'target_name': 'nsssysinit_static',
      'type': 'static_library',
      'sources': [
        'nsssysinit.c',
      ],
      'dependencies': [
        '<(DEPTH)/exports.gyp:nss_exports',
        '<(DEPTH)/lib/util/util.gyp:nssutil3'
      ],
    },
    {
      'target_name': 'nsssysinit',
      'type': 'shared_library',
      'dependencies': [
        'nsssysinit_static',
      ],
      'variables': {
        'mapfile': 'nsssysinit.def',
      },
    }
  ],
  'variables': {
    'module': 'nss',
  }
}
