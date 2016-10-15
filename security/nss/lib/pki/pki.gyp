# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
{
  'includes': [
    '../../coreconf/config.gypi'
  ],
  'targets': [
    {
      'target_name': 'nsspki',
      'type': 'static_library',
      'sources': [
        'asymmkey.c',
        'certdecode.c',
        'certificate.c',
        'cryptocontext.c',
        'pki3hack.c',
        'pkibase.c',
        'pkistore.c',
        'symmkey.c',
        'tdcache.c',
        'trustdomain.c'
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