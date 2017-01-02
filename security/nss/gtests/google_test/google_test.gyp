# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
{
  'includes': [
    '../../coreconf/config.gypi',
    '../common/gtest.gypi',
  ],
  'targets': [
    {
      'target_name': 'gtest',
      'type': 'static_library',
      'sources': [
        'gtest/src/gtest-all.cc'
      ],
      'dependencies': [
        '<(DEPTH)/lib/nss/nss.gyp:nss3',
        '<(DEPTH)/lib/util/util.gyp:nssutil3',
        '<(DEPTH)/lib/smime/smime.gyp:smime3',
        '<(DEPTH)/lib/ssl/ssl.gyp:ssl3',
        '<(DEPTH)/cmd/lib/lib.gyp:sectool'
      ]
    },
    {
      'target_name': 'gtest1',
      'type': 'shared_library',
      'dependencies': [
        'gtest'
      ],
      # Work around a gyp bug. Fixed upstream in gyp:
      # https://chromium.googlesource.com/external/gyp/+/93cc6e2c23e4d5ebd179f388e67aa907d0dfd43d
      'conditions': [
        ['OS!="win"', {
          'libraries': [
            '-lstdc++',
          ],
        }],
      ],
      # For some reason when just linking static libraries into
      # a DLL the link fails without this.
      'msvs_settings': {
        'VCLinkerTool': {
          'AdditionalDependencies': [
            '/DEFAULTLIB:MSVCRT',
          ],
        },
      },
    }
  ],
  'target_defaults': {
    'include_dirs': [
      'gtest/include/',
      'gtest'
    ],
  },
  'variables': {
    'module': 'gtest'
  }
}
