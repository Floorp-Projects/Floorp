# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
{
 'includes': [
    "../../coreconf/config.gypi"
  ],
  'variables': {
    'module': 'nss',
  },
  'conditions': [
    ['use_system_zlib==1', {
      'targets': [{
        'target_name': 'nss_zlib',
        'type': 'none',
        'link_settings': {
          'libraries': ['<@(zlib_libs)'],
        },
      }],
    }, {
      'targets': [{
        'target_name': 'nss_zlib',
        'type': 'static_library',
        'sources': [
          'adler32.c',
          'compress.c',
          'crc32.c',
          'deflate.c',
          'gzclose.c',
          'gzlib.c',
          'gzread.c',
          'gzwrite.c',
          'infback.c',
          'inffast.c',
          'inflate.c',
          'inftrees.c',
          'trees.c',
          'uncompr.c',
          'zutil.c',
        ],
        'defines': [
          # Define verbose as -1 to turn off all zlib trace messages in
          # debug builds.
          'verbose=-1',
          'HAVE_STDARG_H',
        ],
        'conditions': [
          [ 'OS!="win"', {
            'defines': [
              'HAVE_UNISTD_H',
            ],
          }],
        ],
      }],
    }]
  ],
}
