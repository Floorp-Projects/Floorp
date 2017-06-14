# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
{
  'includes': [
    '../../coreconf/config.gypi'
  ],
  'targets': [
    {
      'target_name': 'nss_static',
      'type': 'static_library',
      'sources': [
        'nssinit.c',
        'nssoptions.c',
        'nssver.c',
        'utilwrap.c'
      ],
      'dependencies': [
        '<(DEPTH)/exports.gyp:nss_exports',
      ]
    },
    {
      # This is here so Firefox can link this without having to
      # repeat the list of dependencies.
      'target_name': 'nss3_deps',
      'type': 'none',
      'dependencies': [
        'nss_static',
        '<(DEPTH)/lib/certhigh/certhigh.gyp:certhi',
        '<(DEPTH)/lib/cryptohi/cryptohi.gyp:cryptohi',
        '<(DEPTH)/lib/pk11wrap/pk11wrap.gyp:pk11wrap',
        '<(DEPTH)/lib/certdb/certdb.gyp:certdb',
        '<(DEPTH)/lib/pki/pki.gyp:nsspki',
        '<(DEPTH)/lib/dev/dev.gyp:nssdev',
        '<(DEPTH)/lib/base/base.gyp:nssb',
        '<(DEPTH)/lib/libpkix/libpkix.gyp:libpkix',
      ],
    },
    {
      'target_name': 'nss3',
      'type': 'shared_library',
      'dependencies': [
        'nss3_deps',
        '<(DEPTH)/lib/util/util.gyp:nssutil3',
      ],
      'variables': {
        'mapfile': 'nss.def'
      }
    }
  ],
  'conditions': [
    [ 'moz_fold_libs==1', {
      'targets': [
        {
          'target_name': 'nss3_static',
          'type': 'static_library',
          'dependencies': [
            'nss3_deps',
          ],
        }
      ],
    }],
  ],
  'variables': {
    'module': 'nss'
  }
}
