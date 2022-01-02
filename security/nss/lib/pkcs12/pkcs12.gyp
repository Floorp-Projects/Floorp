# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
{
  'includes': [
    '../../coreconf/config.gypi'
  ],
  'targets': [
    {
      'target_name': 'pkcs12',
      'type': 'static_library',
      'sources': [
        'p12creat.c',
        'p12d.c',
        'p12dec.c',
        'p12e.c',
        'p12local.c',
        'p12plcy.c',
        'p12tmpl.c'
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