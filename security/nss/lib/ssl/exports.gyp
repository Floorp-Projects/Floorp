# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
{
  'includes': [
    '../../coreconf/config.gypi'
  ],
  'targets': [
    {
      'target_name': 'lib_ssl_exports',
      'type': 'none',
      'copies': [
        {
          'files': [
            'preenc.h',
            'ssl.h',
            'sslerr.h',
            'sslproto.h',
            'sslt.h'
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
