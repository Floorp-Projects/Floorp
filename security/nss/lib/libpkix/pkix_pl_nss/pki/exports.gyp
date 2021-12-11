# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
{
  'includes': [
    '../../../../coreconf/config.gypi'
  ],
  'targets': [
    {
      'target_name': 'lib_libpkix_pkix_pl_nss_pki_exports',
      'type': 'none',
      'copies': [
        {
          'files': [
            'pkix_pl_basicconstraints.h',
            'pkix_pl_cert.h',
            'pkix_pl_certpolicyinfo.h',
            'pkix_pl_certpolicymap.h',
            'pkix_pl_certpolicyqualifier.h',
            'pkix_pl_crl.h',
            'pkix_pl_crldp.h',
            'pkix_pl_crlentry.h',
            'pkix_pl_date.h',
            'pkix_pl_generalname.h',
            'pkix_pl_infoaccess.h',
            'pkix_pl_nameconstraints.h',
            'pkix_pl_ocspcertid.h',
            'pkix_pl_ocsprequest.h',
            'pkix_pl_ocspresponse.h',
            'pkix_pl_publickey.h',
            'pkix_pl_x500name.h'
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
