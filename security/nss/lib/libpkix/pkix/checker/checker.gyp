# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
{
  'includes': [
    '../../../../coreconf/config.gypi'
  ],
  'targets': [
    {
      'target_name': 'pkixchecker',
      'type': 'static_library',
      'sources': [
        'pkix_basicconstraintschecker.c',
        'pkix_certchainchecker.c',
        'pkix_crlchecker.c',
        'pkix_ekuchecker.c',
        'pkix_expirationchecker.c',
        'pkix_namechainingchecker.c',
        'pkix_nameconstraintschecker.c',
        'pkix_ocspchecker.c',
        'pkix_policychecker.c',
        'pkix_revocationchecker.c',
        'pkix_revocationmethod.c',
        'pkix_signaturechecker.c',
        'pkix_targetcertchecker.c'
      ],
      'dependencies': [
        '<(DEPTH)/exports.gyp:nss_exports'
      ]
    }
  ],
  'variables': {
    'module': 'nss'
  }
}