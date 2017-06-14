# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
{
  'includes' : [
    '../coreconf/config.gypi',
    '../cmd/platlibs.gypi',
  ],
  'targets' : [
    {
      'target_name' : 'nss',
      'type' : 'executable',
      'sources' : [
        'nss_tool.cc',
        'common/argparse.cc',
        'common/util.cc',
        'db/dbtool.cc',
        'enc/enctool.cc',
        'digest/digesttool.cc'
      ],
      'include_dirs': [
        'common',
      ],
      'dependencies' : [
        '<(DEPTH)/cpputil/cpputil.gyp:cpputil',
        '<(DEPTH)/exports.gyp:dbm_exports',
        '<(DEPTH)/exports.gyp:nss_exports',
      ],
    }
  ],
}
