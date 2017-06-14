# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
{
  'includes': [
    '../../coreconf/config.gypi'
  ],
  'targets': [
    {
      'target_name': 'libpkix',
      'type': 'none',
      'conditions': [
        [ 'disable_libpkix==0', {
          'dependencies': [
            'pkix/certsel/certsel.gyp:pkixcertsel',
            'pkix/checker/checker.gyp:pkixchecker',
            'pkix/crlsel/crlsel.gyp:pkixcrlsel',
            'pkix/params/params.gyp:pkixparams',
            'pkix/results/results.gyp:pkixresults',
            'pkix/store/store.gyp:pkixstore',
            'pkix/top/top.gyp:pkixtop',
            'pkix/util/util.gyp:pkixutil',
            'pkix_pl_nss/module/module.gyp:pkixmodule',
            'pkix_pl_nss/pki/pki.gyp:pkixpki',
            'pkix_pl_nss/system/system.gyp:pkixsystem',
          ],
        }],
      ],
    },
  ],
}