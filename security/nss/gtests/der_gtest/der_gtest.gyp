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
      'target_name': 'der_gtest',
      'type': 'executable',
      'sources': [
        'der_getint_unittest.cc',
        'der_quickder_unittest.cc',
        '<(DEPTH)/gtests/common/gtests.cc'
      ],
      'dependencies': [
        '<(DEPTH)/exports.gyp:nss_exports',
        '<(DEPTH)/gtests/google_test/google_test.gyp:gtest',
        '<(DEPTH)/lib/util/util.gyp:nssutil3',
        '<(DEPTH)/lib/ssl/ssl.gyp:ssl3',
        '<(DEPTH)/lib/nss/nss.gyp:nss3',
      ]
    }
  ],
  'variables': {
    'module': 'nss'
  }
}
