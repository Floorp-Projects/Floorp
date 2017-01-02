# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
{
  'includes': [
    '../../../../coreconf/config.gypi'
  ],
  'targets': [
    {
      'target_name': 'lib_libpkix_pkix_params_exports',
      'type': 'none',
      'copies': [
        {
          'files': [
            'pkix_procparams.h',
            'pkix_resourcelimits.h',
            'pkix_trustanchor.h',
            'pkix_valparams.h'
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
