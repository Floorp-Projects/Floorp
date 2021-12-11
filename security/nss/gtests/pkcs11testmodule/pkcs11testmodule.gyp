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
      'target_name': 'pkcs11testmodule',
      'type': 'shared_library',
      'sources': [
        'pkcs11testmodule.cpp',
      ],
      'dependencies': [
        '<(DEPTH)/exports.gyp:nss_exports',
        '<(DEPTH)/cpputil/cpputil.gyp:cpputil',
      ],
    }
  ],
  'variables': {
    'module': 'nss'
  }
}
