# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
{
  'includes': [
    '../../coreconf/config.gypi'
  ],
  'targets': [
    {
      'target_name': 'softokn',
      'type': 'static_library',
      'sources': [
        'fipsaudt.c',
        'fipstest.c',
        'fipstokn.c',
        'jpakesftk.c',
        'lgglue.c',
        'lowkey.c',
        'lowpbe.c',
        'padbuf.c',
        'pkcs11.c',
        'pkcs11c.c',
        'pkcs11u.c',
        'sdb.c',
        'sftkdb.c',
        'sftkhmac.c',
        'sftkpars.c',
        'sftkpwd.c',
        'softkver.c',
        'tlsprf.c'
      ],
      'dependencies': [
        '<(DEPTH)/exports.gyp:nss_exports',
        '<(DEPTH)/lib/sqlite/sqlite.gyp:sqlite3',
        '<(DEPTH)/lib/freebl/freebl.gyp:freebl',
      ]
    },
    {
      'target_name': 'softokn3',
      'type': 'shared_library',
      'dependencies': [
        'softokn',
      ],
      'conditions': [
        [ 'moz_fold_libs==0', {
          'dependencies': [
            '<(DEPTH)/lib/util/util.gyp:nssutil3',
          ],
        }, {
          'libraries': [
            '<(moz_folded_library_name)',
          ],
        }],
      ],
      'variables': {
        'mapfile': 'softokn.def'
      }
    }
  ],
  'target_defaults': {
    'defines': [
      'SHLIB_SUFFIX=\"<(dll_suffix)\"',
      'SHLIB_PREFIX=\"<(dll_prefix)\"',
      'SOFTOKEN_LIB_NAME=\"libsoftokn3.so\"',
      'SHLIB_VERSION=\"3\"'
    ]
  },
  'variables': {
    'module': 'nss'
  }
}
