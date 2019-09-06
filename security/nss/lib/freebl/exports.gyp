# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
{
  'includes': [
    '../../coreconf/config.gypi'
  ],
  'targets': [
    {
      'target_name': 'lib_freebl_exports',
      'type': 'none',
      'copies': [
        {
          'files': [
            'blapit.h',
            'ecl/ecl-exp.h',
            'shsign.h'
          ],
          'conditions': [
            [ 'OS=="linux"', {
              'files': [
                'nsslowhash.h',
              ],
            }],
          ],
          'destination': '<(nss_public_dist_dir)/<(module)'
        },
        {
          'files': [
            'cmac.h',
            'alghmac.h',
            'blapi.h',
            'blake2b.h',
            'chacha20poly1305.h',
            'ec.h',
            'ecl/ecl-curve.h',
            'ecl/ecl.h',
            'ecl/eclt.h',
            'hmacct.h',
            'secmpi.h',
            'secrng.h'
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
