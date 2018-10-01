# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
{
  'includes': [
    '../../coreconf/config.gypi'
  ],
  'targets': [
    {
      'target_name': 'mozpkix',
      'type': 'static_library',
      'standalone_static_library': 1,
      'sources': [
        'lib/pkixbuild.cpp',
        'lib/pkixcert.cpp',
        'lib/pkixcheck.cpp',
        'lib/pkixder.cpp',
        'lib/pkixnames.cpp',
        'lib/pkixnss.cpp',
        'lib/pkixocsp.cpp',
        'lib/pkixresult.cpp',
        'lib/pkixtime.cpp',
        'lib/pkixverify.cpp',
      ],
      'dependencies': [
        '<(DEPTH)/exports.gyp:nss_mozpkix_exports',
      ],
      'conditions': [
        [ 'mozpkix_only==0', {
          'dependencies': [
            '<(DEPTH)/exports.gyp:nss_exports'
          ],
        }],
      ],
    },
    {
      'target_name': 'mozpkix-testlib',
      'type': 'static_library',
      'standalone_static_library': 1,
      'sources': [
        'test-lib/pkixtestalg.cpp',
        'test-lib/pkixtestnss.cpp',
        'test-lib/pkixtestutil.cpp',
      ],
      'dependencies': [
        '<(DEPTH)/exports.gyp:nss_mozpkix_exports',
      ],
      'conditions': [
        [ 'mozpkix_only==0', {
          'dependencies': [
            '<(DEPTH)/exports.gyp:nss_exports'
          ],
        }],
      ],
    },
  ],
  'variables': {
    'module': 'nss',
  }
}
