# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
{
  'includes': [
    '../coreconf/config.gypi',
  ],
  'targets': [
    {
      'target_name': 'cpputil',
      'type': 'static_library',
      'sources': [
        'databuffer.cc',
        'dummy_io.cc',
        'dummy_io_fwd.cc',
        'tls_parser.cc',
      ],
      'dependencies': [
        '<(DEPTH)/exports.gyp:nss_exports',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(DEPTH)/cpputil',
        ],
      },
    },
  ],
}

