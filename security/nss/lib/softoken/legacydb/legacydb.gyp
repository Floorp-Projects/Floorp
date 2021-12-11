# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
{
  'includes': [
    '../../../coreconf/config.gypi'
  ],
  'targets': [
    {
      'target_name': 'nssdbm',
      'type': 'static_library',
      'sources': [
        'dbmshim.c',
        'keydb.c',
        'lgattr.c',
        'lgcreate.c',
        'lgdestroy.c',
        'lgfind.c',
        'lgfips.c',
        'lginit.c',
        'lgutil.c',
        'lowcert.c',
        'lowkey.c',
        'pcertdb.c',
        'pk11db.c'
      ],
      'dependencies': [
        '<(DEPTH)/exports.gyp:dbm_exports',
        '<(DEPTH)/exports.gyp:nss_exports',
        '<(DEPTH)/lib/freebl/freebl.gyp:freebl',
        '<(DEPTH)/lib/dbm/src/src.gyp:dbm'
      ]
    },
    {
      'target_name': 'nssdbm3',
      'type': 'shared_library',
      'dependencies': [
        'nssdbm'
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
        'mapfile': 'nssdbm.def'
      }
    }
  ],
  'target_defaults': {
    'defines': [
      'SHLIB_SUFFIX=\"<(dll_suffix)\"',
      'SHLIB_PREFIX=\"<(dll_prefix)\"',
      'LG_LIB_NAME=\"<(dll_prefix)nssdbm3.<(dll_suffix)\"'
    ]
  },
  'variables': {
    'module': 'nss'
  }
}
