# Copyright (c) 2013 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'make_global_settings': [
    ['CC', '/usr/bin/clang'],
    ['CXX', '/usr/bin/clang++'],
  ],

  'targets': [
    {
      'target_name': 'arc_enabled',
      'type': 'static_library',
      'sources': [
        'c-file.c',
        'cc-file.cc',
        'm-file.m',
        'mm-file.mm',
      ],
      'xcode_settings': {
        'CLANG_ENABLE_OBJC_ARC': 'YES',
      },
    },

    {
      'target_name': 'weak_enabled',
      'type': 'static_library',
      'sources': [
        'c-file.c',
        'cc-file.cc',
        'm-file-arc-weak.m',
        'mm-file-arc-weak.mm',
      ],
      'xcode_settings': {
        'CLANG_ENABLE_OBJC_WEAK': 'YES',
      },
    },

    {
      'target_name': 'arc_disabled',
      'type': 'static_library',
      'sources': [
        'c-file.c',
        'cc-file.cc',
        'm-file-no-arc.m',
        'mm-file-no-arc.mm',
      ],
      'xcode_settings': {
      },
    },
  ],
}

