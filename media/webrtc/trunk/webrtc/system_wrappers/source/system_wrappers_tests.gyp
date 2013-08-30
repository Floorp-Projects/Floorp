# Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'includes': ['../../build/common.gypi',],
  'targets': [
    {
      'target_name': 'system_wrappers_unittests',
      'type': 'executable',
      'dependencies': [
        '<(DEPTH)/testing/gtest.gyp:gtest',
        '<(webrtc_root)/system_wrappers/source/system_wrappers.gyp:system_wrappers',
        '<(webrtc_root)/test/test.gyp:test_support_main',
      ],
      'sources': [
        '../../common_unittest.cc',
        'aligned_malloc_unittest.cc',
        'clock_unittest.cc',
        'condition_variable_unittest.cc',
        'critical_section_unittest.cc',
        'event_tracer_unittest.cc',
        'list_unittest.cc',
        'logging_unittest.cc',
        'map_unittest.cc',
        'data_log_unittest.cc',
        'data_log_unittest_disabled.cc',
        'data_log_helpers_unittest.cc',
        'data_log_c_helpers_unittest.c',
        'data_log_c_helpers_unittest.h',
        'stringize_macros_unittest.cc',
        'thread_unittest.cc',
        'thread_posix_unittest.cc',
        'unittest_utilities_unittest.cc',
      ],
      'conditions': [
        ['enable_data_logging==1', {
          'sources!': [ 'data_log_unittest_disabled.cc', ],
        }, {
          'sources!': [ 'data_log_unittest.cc', ],
        }],
        ['os_posix==0', {
          'sources!': [ 'thread_posix_unittest.cc', ],
        }],
      ],
      # Disable warnings to enable Win64 build, issue 1323.
      'msvs_disabled_warnings': [
        4267,  # size_t to int truncation.
      ],
    },
  ],
}

