# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
{
  'includes': [
    '../../coreconf/config.gypi'
  ],
  'targets': [
    {
      'target_name': 'nssckfw',
      'type': 'static_library',
      'sources': [
        'crypto.c',
        'find.c',
        'hash.c',
        'instance.c',
        'mechanism.c',
        'mutex.c',
        'object.c',
        'session.c',
        'sessobj.c',
        'slot.c',
        'token.c',
        'wrap.c'
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