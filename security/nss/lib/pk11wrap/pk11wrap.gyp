# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
{
  'includes': [
    '../../coreconf/config.gypi'
  ],
  'targets': [
    {
      'target_name': 'pk11wrap_static',
      'type': 'static_library',
      'defines': [
        'NSS_STATIC_SOFTOKEN',
      ],
      'dependencies': [
        'pk11wrap_base',
        '<(DEPTH)/exports.gyp:nss_exports',
        '<(DEPTH)/lib/softoken/softoken.gyp:softokn_static',
      ],
    },
    {
      'target_name': 'pk11wrap',
      'type': 'static_library',
      'dependencies': [
        'pk11wrap_base',
        '<(DEPTH)/exports.gyp:nss_exports',
      ],
    },
    {
      'target_name': 'pk11wrap_base',
      'type': 'none',
      'direct_dependent_settings': {
        'sources': [
          'dev3hack.c',
          'pk11akey.c',
          'pk11auth.c',
          'pk11cert.c',
          'pk11cxt.c',
          'pk11err.c',
          'pk11hpke.c',
          'pk11kea.c',
          'pk11list.c',
          'pk11load.c',
          'pk11mech.c',
          'pk11merge.c',
          'pk11nobj.c',
          'pk11obj.c',
          'pk11pars.c',
          'pk11pbe.c',
          'pk11pk12.c',
          'pk11pqg.c',
          'pk11sdr.c',
          'pk11skey.c',
          'pk11slot.c',
          'pk11util.c'
        ],
      },
    },
  ],
  'target_defaults': {
    'defines': [
      'SHLIB_SUFFIX=\"<(dll_suffix)\"',
      'SHLIB_PREFIX=\"<(dll_prefix)\"',
      'NSS_SHLIB_VERSION=\"3\"',
      'SOFTOKEN_SHLIB_VERSION=\"3\"'
    ],
  },
  'variables': {
    'module': 'nss'
  }
}
