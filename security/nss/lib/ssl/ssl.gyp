# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
{
  'includes': [
    '../../coreconf/config.gypi'
  ],
  'targets': [
    {
      'target_name': 'ssl',
      'type': 'static_library',
      'sources': [
        'authcert.c',
        'cmpcert.c',
        'dtlscon.c',
        'prelib.c',
        'ssl3con.c',
        'ssl3ecc.c',
        'ssl3ext.c',
        'ssl3exthandle.c',
        'ssl3gthr.c',
        'sslauth.c',
        'sslcert.c',
        'sslcon.c',
        'ssldef.c',
        'sslenum.c',
        'sslerr.c',
        'sslerrstrs.c',
        'sslgrp.c',
        'sslinfo.c',
        'sslinit.c',
        'sslmutex.c',
        'sslnonce.c',
        'sslreveal.c',
        'sslsecur.c',
        'sslsnce.c',
        'sslsock.c',
        'ssltrace.c',
        'sslver.c',
        'tls13con.c',
        'tls13exthandle.c',
        'tls13hkdf.c',
      ],
      'conditions': [
        [ 'OS=="win"', {
          'sources': [
            'win32err.c',
          ],
          'defines': [
            'IN_LIBSSL',
          ],
        }, {
          # Not Windows.
          'sources': [
            'unix_err.c'
          ],
        }],
        [ 'ssl_enable_zlib==1', {
          'dependencies': [
            '<(DEPTH)/lib/zlib/zlib.gyp:nss_zlib'
          ],
          'defines': [
            'NSS_SSL_ENABLE_ZLIB',
          ],
        }],
        [ 'fuzz==1', {
          'defines': [
            'UNSAFE_FUZZER_MODE',
          ],
        }],
        [ 'mozilla_client==1', {
          'defines': [
            'NSS_ENABLE_TLS13_SHORT_HEADERS',
          ],
        }],
      ],
      'dependencies': [
        '<(DEPTH)/exports.gyp:nss_exports',
        '<(DEPTH)/lib/freebl/freebl.gyp:freebl',
      ],
    },
    {
      'target_name': 'ssl3',
      'type': 'shared_library',
      'dependencies': [
        'ssl',
        '<(DEPTH)/lib/nss/nss.gyp:nss3',
        '<(DEPTH)/lib/util/util.gyp:nssutil3',
      ],
      'variables': {
        'mapfile': 'ssl.def'
      }
    }
  ],
  'target_defaults': {
    'defines': [
      'NSS_ALLOW_SSLKEYLOGFILE=1'
    ]
  },
  'variables': {
    'module': 'nss'
  }
}
