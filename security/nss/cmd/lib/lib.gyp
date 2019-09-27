# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
{
  'includes': [
    '../../coreconf/config.gypi'
  ],
  'targets': [
    {
      'target_name': 'sectool',
      'type': 'static_library',
      'standalone_static_library': 1,
      'sources': [
        'basicutil.c',
        'derprint.c',
        'ffs.c',
        'moreoids.c',
        'pk11table.c',
        'pppolicy.c',
        'secpwd.c',
        'secutil.c'
      ],
      'dependencies': [
        '<(DEPTH)/exports.gyp:nss_exports'
      ]
    }
  ],
  'target_defaults': {
    'defines': [
      'NSPR20',
      'NSS_USE_STATIC_LIBS'
    ]
  },
  'variables': {
    'module': 'nss'
  }
}