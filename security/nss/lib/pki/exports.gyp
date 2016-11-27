# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
{
  'includes': [
    '../../coreconf/config.gypi'
  ],
  'targets': [
    {
      'target_name': 'lib_pki_exports',
      'type': 'none',
      'copies': [
        {
          'files': [
            'nsspki.h',
            'nsspkit.h',
            'pki.h',
            'pki3hack.h',
            'pkim.h',
            'pkistore.h',
            'pkit.h',
            'pkitm.h'
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
