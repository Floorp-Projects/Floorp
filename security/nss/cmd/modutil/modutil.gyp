# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
{
  'includes': [
    '../../coreconf/config.gypi',
    '../../cmd/platlibs.gypi'
  ],
  'targets': [
    {
      'target_name': 'modutil',
      'type': 'executable',
      'sources': [
        'install-ds.c',
        'install.c',
        'installparse.c',
        'instsec.c',
        'lex.Pk11Install_yy.c',
        'modutil.c',
        'pk11.c'
      ],
      'dependencies': [
        '<(DEPTH)/exports.gyp:nss_exports',
        '<(DEPTH)/exports.gyp:dbm_exports',
        '<(DEPTH)/lib/jar/jar.gyp:jar',
        '<(DEPTH)/lib/zlib/zlib.gyp:nss_zlib'
      ]
    }
  ],
  'target_defaults': {
    'include_dirs': [
      '<(nss_private_dist_dir)/nss',
      '<(nss_private_dist_dir)/dbm'
    ],
    'defines': [
      'NSPR20',
      'YY_NO_UNPUT',
      'YY_NO_INPUT'
    ]
  },
  'variables': {
    'module': 'sectools'
  }
}
