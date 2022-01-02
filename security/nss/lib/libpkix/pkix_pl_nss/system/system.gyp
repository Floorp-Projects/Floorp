# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
{
  'includes': [
    '../../../../coreconf/config.gypi'
  ],
  'targets': [
    {
      'target_name': 'pkixsystem',
      'type': 'static_library',
      'sources': [
        'pkix_pl_bigint.c',
        'pkix_pl_bytearray.c',
        'pkix_pl_common.c',
        'pkix_pl_error.c',
        'pkix_pl_hashtable.c',
        'pkix_pl_lifecycle.c',
        'pkix_pl_mem.c',
        'pkix_pl_monitorlock.c',
        'pkix_pl_mutex.c',
        'pkix_pl_object.c',
        'pkix_pl_oid.c',
        'pkix_pl_primhash.c',
        'pkix_pl_rwlock.c',
        'pkix_pl_string.c'
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