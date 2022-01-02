# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
{
  'includes': [
    '../../coreconf/config.gypi'
  ],
  'targets': [
    {
      'target_name': 'lib_mozpkix_exports',
      'type': 'none',
      'copies': [
        {
          'files': [
            '<(DEPTH)/cpputil/nss_scoped_ptrs.h',
            'include/pkix/Input.h',
            'include/pkix/Time.h',
            'include/pkix/Result.h',
            'include/pkix/pkix.h',
            'include/pkix/pkixc.h',
            'include/pkix/pkixnss.h',
            'include/pkix/pkixtypes.h',
            'include/pkix/pkixutil.h',
            'include/pkix/pkixcheck.h',
            'include/pkix/pkixder.h',
          ],
          'destination': '<(nss_public_dist_dir)/<(module)/mozpkix'
        },
      ],
    },
    {
      'target_name': 'lib_mozpkix_test_exports',
      'type': 'none',
      'copies': [
        {
          'files': [
            'include/pkix-test/pkixtestutil.h',
            'include/pkix-test/pkixtestnss.h',
          ],
          'destination': '<(nss_public_dist_dir)/<(module)/mozpkix/test'
        },
      ],
    }
  ],
  'variables': {
    'module': 'nss'
  }
}
