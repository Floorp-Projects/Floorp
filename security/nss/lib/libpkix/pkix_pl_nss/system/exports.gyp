# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
{
  'includes': [
    '../../../../coreconf/config.gypi'
  ],
  'targets': [
    {
      'target_name': 'lib_libpkix_pkix_pl_nss_system_exports',
      'type': 'none',
      'copies': [
        {
          'files': [
            'pkix_pl_bigint.h',
            'pkix_pl_bytearray.h',
            'pkix_pl_common.h',
            'pkix_pl_hashtable.h',
            'pkix_pl_lifecycle.h',
            'pkix_pl_mem.h',
            'pkix_pl_monitorlock.h',
            'pkix_pl_mutex.h',
            'pkix_pl_object.h',
            'pkix_pl_oid.h',
            'pkix_pl_primhash.h',
            'pkix_pl_rwlock.h',
            'pkix_pl_string.h'
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
