# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
{
  'includes': [
    '../../coreconf/config.gypi'
  ],
  'targets': [
    {
      'target_name': 'lib_softoken_exports',
      'type': 'none',
      'copies': [
        {
          'files': [
            'lowkeyi.h',
            'lowkeyti.h'
          ],
          'destination': '<(nss_public_dist_dir)/<(module)'
        },
        {
          'files': [
            'lgglue.h',
            'pkcs11ni.h',
            'sdb.h',
            'sftkdbt.h',
            'softkver.h',
            'softoken.h',
            'softoknt.h'
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
