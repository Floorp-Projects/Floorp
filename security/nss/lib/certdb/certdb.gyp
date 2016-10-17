# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
{
  'includes': [
    '../../coreconf/config.gypi'
  ],
  'targets': [
    {
      'target_name': 'certdb',
      'type': 'static_library',
      'sources': [
        'alg1485.c',
        'certdb.c',
        'certv3.c',
        'certxutl.c',
        'crl.c',
        'genname.c',
        'polcyxtn.c',
        'secname.c',
        'stanpcertdb.c',
        'xauthkid.c',
        'xbsconst.c',
        'xconst.c'
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