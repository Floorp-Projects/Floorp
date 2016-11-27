# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
{
  'includes': [
    '../../coreconf/config.gypi',
    'gtest.gypi',
  ],
  'targets': [
    {
      'target_name': 'gtests',
      'type': 'executable',
      'sources': [
        'gtests.cc'
      ],
      'dependencies': [
        '<(DEPTH)/exports.gyp:nss_exports',
        '<(DEPTH)/lib/nss/nss.gyp:nss3',
        '<(DEPTH)/lib/util/util.gyp:nssutil3',
        '<(DEPTH)/lib/smime/smime.gyp:smime3',
        '<(DEPTH)/gtests/google_test/google_test.gyp:gtest',
        '<(DEPTH)/cmd/lib/lib.gyp:sectool'
      ]
    }
  ],
  'target_defaults': {
    'include_dirs': [
      '../../gtests/google_test/gtest/include',
      '../../gtests/common'
    ],
  },
  'variables': {
    'module': 'nss'
  }
}
