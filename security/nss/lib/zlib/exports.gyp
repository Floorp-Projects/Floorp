# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
{
  'includes': [
    '../../coreconf/config.gypi'
  ],
  'variables': {
    'module': 'nss',
  },
  'conditions': [
    ['use_system_zlib==1', {
      'targets': [{
        'target_name': 'lib_zlib_exports',
        'type': 'none',
      }],
    }, {
      'targets': [{
        'target_name': 'lib_zlib_exports',
        'type': 'none',
        'copies': [
          {
          'files': [
            'zlib.h',
            'zconf.h',
          ],
          'destination': '<(nss_private_dist_dir)/<(module)'
          }
      ]
    }],
    }],
  ],
}
