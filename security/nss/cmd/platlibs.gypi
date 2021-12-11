# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
{
  'variables': {
    'use_static_libs%': 0,
  },
  'target_defaults': {
    'dependencies': [
      '<(DEPTH)/cmd/lib/lib.gyp:sectool',
    ],
    'conditions': [
      ['moz_fold_libs==0', {
        'dependencies': [
          '<(DEPTH)/lib/util/util.gyp:nssutil3',
        ],
      }],
      ['<(use_static_libs)==1', {
        'defines': ['NSS_USE_STATIC_LIBS'],
        'dependencies': [
          '<(DEPTH)/lib/smime/smime.gyp:smime',
          '<(DEPTH)/lib/ssl/ssl.gyp:ssl',
          '<(DEPTH)/lib/nss/nss.gyp:nss_static',
          '<(DEPTH)/lib/pkcs12/pkcs12.gyp:pkcs12',
          '<(DEPTH)/lib/pkcs7/pkcs7.gyp:pkcs7',
          '<(DEPTH)/lib/certhigh/certhigh.gyp:certhi',
          '<(DEPTH)/lib/cryptohi/cryptohi.gyp:cryptohi',
          '<(DEPTH)/lib/pk11wrap/pk11wrap.gyp:pk11wrap',
          '<(DEPTH)/lib/softoken/softoken.gyp:softokn',
          '<(DEPTH)/lib/certdb/certdb.gyp:certdb',
          '<(DEPTH)/lib/pki/pki.gyp:nsspki',
          '<(DEPTH)/lib/dev/dev.gyp:nssdev',
          '<(DEPTH)/lib/base/base.gyp:nssb',
          '<(DEPTH)/lib/freebl/freebl.gyp:freebl',
          '<(DEPTH)/lib/sqlite/sqlite.gyp:sqlite3',
          '<(DEPTH)/lib/libpkix/libpkix.gyp:libpkix',
        ],
        'conditions': [
          [ 'disable_dbm==0', {
            'dependencies': [
              '<(DEPTH)/lib/dbm/src/src.gyp:dbm',
              '<(DEPTH)/lib/softoken/legacydb/legacydb.gyp:nssdbm',
            ],
          }],
        ]},{ # !use_static_libs
          'conditions': [
            ['moz_fold_libs==0', {
              'dependencies': [
                '<(DEPTH)/lib/ssl/ssl.gyp:ssl3',
                '<(DEPTH)/lib/smime/smime.gyp:smime3',
                '<(DEPTH)/lib/nss/nss.gyp:nss3',
              ],
            }, {
              'libraries': [
                '<(moz_folded_library_name)',
              ],
            }]
          ],
        }],
    ],
  }
}
