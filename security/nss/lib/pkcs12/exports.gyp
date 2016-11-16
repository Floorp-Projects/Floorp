# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
{
  'includes': [
    '../../coreconf/config.gypi'
  ],
  'targets': [
    {
      'target_name': 'lib_pkcs12_exports',
      'type': 'none',
      'copies': [
        {
          'files': [
            'p12.h',
            'p12plcy.h',
            'p12t.h',
            'pkcs12.h',
            'pkcs12t.h'
          ],
          'destination': '<(nss_public_dist_dir)/<(module)'
        }
      ]
    }
  ],
  'variables': {
    'module': 'nss'
  }
}
