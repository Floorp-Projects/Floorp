# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
{
  'includes': [
    '../../../coreconf/config.gypi'
  ],
  'targets': [
    {
      'target_name': 'lib_libpkix_include_exports',
      'type': 'none',
      'copies': [
        {
          'files': [
            'pkix.h',
            'pkix_certsel.h',
            'pkix_certstore.h',
            'pkix_checker.h',
            'pkix_crlsel.h',
            'pkix_errorstrings.h',
            'pkix_params.h',
            'pkix_pl_pki.h',
            'pkix_pl_system.h',
            'pkix_results.h',
            'pkix_revchecker.h',
            'pkix_sample_modules.h',
            'pkix_util.h',
            'pkixt.h'
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
