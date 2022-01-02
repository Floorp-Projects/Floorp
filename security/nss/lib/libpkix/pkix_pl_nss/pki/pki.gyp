# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
{
  'includes': [
    '../../../../coreconf/config.gypi'
  ],
  'targets': [
    {
      'target_name': 'pkixpki',
      'type': 'static_library',
      'sources': [
        'pkix_pl_basicconstraints.c',
        'pkix_pl_cert.c',
        'pkix_pl_certpolicyinfo.c',
        'pkix_pl_certpolicymap.c',
        'pkix_pl_certpolicyqualifier.c',
        'pkix_pl_crl.c',
        'pkix_pl_crldp.c',
        'pkix_pl_crlentry.c',
        'pkix_pl_date.c',
        'pkix_pl_generalname.c',
        'pkix_pl_infoaccess.c',
        'pkix_pl_nameconstraints.c',
        'pkix_pl_ocspcertid.c',
        'pkix_pl_ocsprequest.c',
        'pkix_pl_ocspresponse.c',
        'pkix_pl_publickey.c',
        'pkix_pl_x500name.c'
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