# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
{
  'includes': [
    '../../../../coreconf/config.gypi'
  ],
  'targets': [
    {
      'target_name': 'lib_libpkix_pkix_pl_nss_module_exports',
      'type': 'none',
      'copies': [
        {
          'files': [
            'pkix_pl_aiamgr.h',
            'pkix_pl_colcertstore.h',
            'pkix_pl_httpcertstore.h',
            'pkix_pl_httpdefaultclient.h',
            'pkix_pl_ldapcertstore.h',
            'pkix_pl_ldapdefaultclient.h',
            'pkix_pl_ldaprequest.h',
            'pkix_pl_ldapresponse.h',
            'pkix_pl_ldapt.h',
            'pkix_pl_nsscontext.h',
            'pkix_pl_pk11certstore.h',
            'pkix_pl_socket.h'
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
