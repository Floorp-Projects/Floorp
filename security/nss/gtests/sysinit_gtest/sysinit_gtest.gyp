# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
{
  'includes': [
    '../../coreconf/config.gypi',
    '../common/gtest.gypi'
  ],
  'targets': [
    {
      'target_name': 'sysinit_gtest',
      'type': 'executable',
      'sources': [
        'sysinit_gtest.cc',
        'getUserDB_unittest.cc',
      ],
      'dependencies': [
        '<(DEPTH)/exports.gyp:nss_exports',
        '<(DEPTH)/gtests/google_test/google_test.gyp:gtest',
        '<(DEPTH)/lib/sysinit/sysinit.gyp:nsssysinit_static'
      ]
    }
  ],
  'target_defaults': {
    'include_dirs': [
      '../../lib/sysinit'
    ],
    'defines': [
      'NSS_USE_STATIC_LIBS'
    ]
  },
  'variables': {
    'module': 'nss'
  }
}
