# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
{
  'includes': [
    '../../../../coreconf/config.gypi'
  ],
  'targets': [
    {
      'target_name': 'pkixmodule',
      'type': 'static_library',
      'sources': [
        'pkix_pl_aiamgr.c',
        'pkix_pl_colcertstore.c',
        'pkix_pl_httpcertstore.c',
        'pkix_pl_httpdefaultclient.c',
        'pkix_pl_ldapcertstore.c',
        'pkix_pl_ldapdefaultclient.c',
        'pkix_pl_ldaprequest.c',
        'pkix_pl_ldapresponse.c',
        'pkix_pl_ldaptemplates.c',
        'pkix_pl_nsscontext.c',
        'pkix_pl_pk11certstore.c',
        'pkix_pl_socket.c'
      ],
      'dependencies': [
        '<(DEPTH)/exports.gyp:nss_exports'
      ]
    }
  ],
  'target_defaults': {
    'defines': [
      'SHLIB_SUFFIX=\"<(dll_suffix)\"',
      'SHLIB_PREFIX=\"<(dll_prefix)\"',
      'SHLIB_VERSION=\"\"'
    ]
  },
  'variables': {
    'module': 'nss'
  }
}