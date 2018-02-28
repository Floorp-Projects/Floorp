# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
{
  'includes' : [
    '../coreconf/config.gypi',
    '../cmd/platlibs.gypi',
  ],
  'targets' : [
    {
      'target_name' : 'nss',
      'type' : 'executable',
      'sources' : [
        'nss_tool.cc',
        'common/argparse.cc',
        'common/util.cc',
        'db/dbtool.cc',
        'enc/enctool.cc',
        'digest/digesttool.cc'
      ],
      'include_dirs': [
        'common',
      ],
      'dependencies' : [
        '<(DEPTH)/cpputil/cpputil.gyp:cpputil',
        '<(DEPTH)/exports.gyp:dbm_exports',
        '<(DEPTH)/exports.gyp:nss_exports',
      ],
    },
    {
      'target_name': 'hw-support',
      'type': 'executable',
      'sources': [
        'hw-support.c',
      ],
      'conditions': [
        [ 'OS=="win"', {
          'libraries': [
            'advapi32.lib',
          ],
        }],
      ],
      'dependencies' : [
        '<(DEPTH)/exports.gyp:nss_exports',
        '<(DEPTH)/lib/util/util.gyp:nssutil3',
        '<(DEPTH)/lib/nss/nss.gyp:nss_static',
        '<(DEPTH)/lib/pk11wrap/pk11wrap.gyp:pk11wrap_static',
        '<(DEPTH)/lib/cryptohi/cryptohi.gyp:cryptohi',
        '<(DEPTH)/lib/certhigh/certhigh.gyp:certhi',
        '<(DEPTH)/lib/certdb/certdb.gyp:certdb',
        '<(DEPTH)/lib/base/base.gyp:nssb',
        '<(DEPTH)/lib/dev/dev.gyp:nssdev',
        '<(DEPTH)/lib/pki/pki.gyp:nsspki',
      ],
      'include_dirs': [
        '<(DEPTH)/lib/freebl',
        '<(DEPTH)/lib/freebl/mpi',
      ],
      'defines': [
        'NSS_USE_STATIC_LIBS'
      ],
      'variables': {
        'module': 'nss',
        'use_static_libs': 1
      },
    },
  ],
}
