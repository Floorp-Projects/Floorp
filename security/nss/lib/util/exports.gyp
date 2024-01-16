# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
{
  'includes': [
    '../../coreconf/config.gypi'
  ],
  'targets': [
    {
      'target_name': 'lib_util_exports',
      'type': 'none',
      'copies': [
        {
          'files': [
            'base64.h',
            'ciferfam.h',
            'eccutil.h',
            'hasht.h',
            'kyber.h',
            'nssb64.h',
            'nssb64t.h',
            'nssilckt.h',
            'nssilock.h',
            'nsslocks.h',
            'nssrwlk.h',
            'nssrwlkt.h',
            'nssutil.h',
            'pkcs11.h',
            'pkcs11f.h',
            'pkcs11n.h',
            'pkcs11p.h',
            'pkcs11t.h',
            'pkcs11u.h',
            'pkcs11uri.h',
            'pkcs1sig.h',
            'portreg.h',
            'secasn1.h',
            'secasn1t.h',
            'seccomon.h',
            'secder.h',
            'secdert.h',
            'secdig.h',
            'secdigt.h',
            'secerr.h',
            'secitem.h',
            'secoid.h',
            'secoidt.h',
            'secport.h',
            'utilmodt.h',
            'utilpars.h',
            'utilparst.h',
            'utilrename.h'
          ],
          'destination': '<(nss_public_dist_dir)/<(module)'
        },
        {
          'files': [
            'templates.c',
            'verref.h'
          ],
          'destination': '<(nss_private_dist_dir)/<(module)'
        }
      ]
    }
  ],
  'variables': {
    'module': 'nss'
  }
}
