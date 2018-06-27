# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# TODO(mark): Upstream this file to googleurl.
{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'googleurl',
      'type': '<(component)',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../third_party/icu/icu.gyp:icudata',
        '../../third_party/icu/icu.gyp:icui18n',
        '../../third_party/icu/icu.gyp:icuuc',
      ],
      'sources': [
        '../../googleurl/src/gurl.cc',
        '../../googleurl/src/gurl.h',
        '../../googleurl/src/url_canon.h',
        '../../googleurl/src/url_canon_etc.cc',
        '../../googleurl/src/url_canon_fileurl.cc',
        '../../googleurl/src/url_canon_filesystemurl.cc',
        '../../googleurl/src/url_canon_host.cc',
        '../../googleurl/src/url_canon_icu.cc',
        '../../googleurl/src/url_canon_icu.h',
        '../../googleurl/src/url_canon_internal.cc',
        '../../googleurl/src/url_canon_internal.h',
        '../../googleurl/src/url_canon_internal_file.h',
        '../../googleurl/src/url_canon_ip.cc',
        '../../googleurl/src/url_canon_ip.h',
        '../../googleurl/src/url_canon_mailtourl.cc',
        '../../googleurl/src/url_canon_path.cc',
        '../../googleurl/src/url_canon_pathurl.cc',
        '../../googleurl/src/url_canon_query.cc',
        '../../googleurl/src/url_canon_relative.cc',
        '../../googleurl/src/url_canon_stdstring.h',
        '../../googleurl/src/url_canon_stdurl.cc',
        '../../googleurl/src/url_file.h',
        '../../googleurl/src/url_parse.cc',
        '../../googleurl/src/url_parse.h',
        '../../googleurl/src/url_parse_file.cc',
        '../../googleurl/src/url_parse_internal.h',
        '../../googleurl/src/url_util.cc',
        '../../googleurl/src/url_util.h',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '../..',
        ],
      },
      'defines': [
        'FULL_FILESYSTEM_URL_SUPPORT=1',
      ],
      'conditions': [
        ['component=="shared_library"', {
          'defines': [
            'GURL_DLL',
            'GURL_IMPLEMENTATION=1',
          ],
          'direct_dependent_settings': {
            'defines': [
              'GURL_DLL',
            ],
          },
        }],
      ],
    },
    {
      'target_name': 'googleurl_unittests',
      'type': 'executable',
      'dependencies': [
        'googleurl',
        '../../base/base.gyp:base_i18n',
        '../../base/base.gyp:run_all_unittests',
        '../../testing/gtest.gyp:gtest',
        '../../third_party/icu/icu.gyp:icuuc',
      ],
      'sources': [
        '../../googleurl/src/gurl_unittest.cc',
        '../../googleurl/src/url_canon_unittest.cc',
        '../../googleurl/src/url_parse_unittest.cc',
        '../../googleurl/src/url_test_utils.h',
        '../../googleurl/src/url_util_unittest.cc',
      ],
      'defines': [
        'FULL_FILESYSTEM_URL_SUPPORT=1',
      ],
      'conditions': [
        ['os_posix==1 and OS!="mac" and OS!="ios"', {
          'conditions': [
            ['linux_use_tcmalloc==1', {
              'dependencies': [
                '../../base/allocator/allocator.gyp:allocator',
              ],
            }],
          ],
        }],
      ],
    },
  ],
}
