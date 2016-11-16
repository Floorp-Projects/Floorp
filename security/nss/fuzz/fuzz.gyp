# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
{
  'includes': [
    '../coreconf/config.gypi',
    '../cmd/platlibs.gypi'
  ],
  'targets': [
    {
      'target_name': 'nssfuzz',
      'type': 'executable',
      'sources': [
        'cert_target.cc',
        'pkcs8_target.cc',
        'spki_target.cc',
        'nssfuzz.cc',
      ],
      'dependencies': [
        '<(DEPTH)/exports.gyp:nss_exports',
        '<(DEPTH)/fuzz/libFuzzer/libFuzzer.gyp:libFuzzer'
      ]
    }
  ],
  'target_defaults': {
    'include_dirs': [
      'libFuzzer',
    ],
  },
  'variables': {
    'module': 'nss',
  }
}
