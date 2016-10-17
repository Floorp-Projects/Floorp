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
      'target_name': 'signtool',
      'type': 'executable',
      'sources': [
        'certgen.c',
        'javascript.c',
        'list.c',
        'sign.c',
        'signtool.c',
        'util.c',
        'verify.c',
        'zip.c'
      ],
      'dependencies': [
        '<(DEPTH)/exports.gyp:nss_exports',
        '<(DEPTH)/lib/jar/jar.gyp:jar',
        '<(DEPTH)/lib/zlib/zlib.gyp:nss_zlib'
      ]
    }
  ],
  'variables': {
    'module': 'nss'
  }
}
