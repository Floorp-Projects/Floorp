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
    },
  ],
  'target_defaults': {
    'include_dirs': [
      'gtest'
    ],
  },
  'variables': {
    'module': 'gtest'
  }
}
