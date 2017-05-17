# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
{
  'includes': [
    '../../coreconf/config.gypi'
  ],
  'targets': [
    {
      'target_name': 'nssutil',
      'type': 'static_library',
      'sources': [
        'derdec.c',
        'derenc.c',
        'dersubr.c',
        'dertime.c',
        'errstrs.c',
        'nssb64d.c',
        'nssb64e.c',
        'nssilock.c',
        'nssrwlk.c',
        'oidstring.c',
        'pkcs1sig.c',
        'pkcs11uri.c',
        'portreg.c',
        'quickder.c',
        'secalgid.c',
        'secasn1d.c',
        'secasn1e.c',
        'secasn1u.c',
        'secdig.c',
        'secitem.c',
        'secload.c',
        'secoid.c',
        'secport.c',
        'sectime.c',
        'templates.c',
        'utf8.c',
        'utilmod.c',
        'utilpars.c'
      ],
      'dependencies': [
        '<(DEPTH)/exports.gyp:nss_exports'
      ]
    },
    {
      'target_name': 'nssutil3',
      'type': 'shared_library',
      'dependencies': [
        'nssutil'
      ],
      'variables': {
        'mapfile': 'nssutil.def'
      }
    }
  ],
  'variables': {
    'module': 'nss'
  }
}