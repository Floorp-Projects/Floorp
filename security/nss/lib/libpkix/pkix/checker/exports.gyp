# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
{
  'includes': [
    '../../../../coreconf/config.gypi'
  ],
  'targets': [
    {
      'target_name': 'lib_libpkix_pkix_checker_exports',
      'type': 'none',
      'copies': [
        {
          'files': [
            'pkix_basicconstraintschecker.h',
            'pkix_certchainchecker.h',
            'pkix_crlchecker.h',
            'pkix_ekuchecker.h',
            'pkix_expirationchecker.h',
            'pkix_namechainingchecker.h',
            'pkix_nameconstraintschecker.h',
            'pkix_ocspchecker.h',
            'pkix_policychecker.h',
            'pkix_revocationchecker.h',
            'pkix_revocationmethod.h',
            'pkix_signaturechecker.h',
            'pkix_targetcertchecker.h'
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
