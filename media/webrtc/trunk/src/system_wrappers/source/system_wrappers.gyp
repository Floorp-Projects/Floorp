# Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'includes': [ '../../build/common.gypi', ],
  'targets': [
    {
      'target_name': 'system_wrappers',
      'type': '<(library)',
      'include_dirs': [
        'spreadsortlib',
        '../interface',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '../interface',
        ],
      },
      'sources': [
        '../interface/aligned_malloc.h',
        '../interface/atomic32_wrapper.h',
        '../interface/compile_assert.h',
        '../interface/condition_variable_wrapper.h',
        '../interface/cpu_info.h',
        '../interface/cpu_wrapper.h',
        '../interface/cpu_features_wrapper.h',
        '../interface/critical_section_wrapper.h',
        '../interface/data_log.h',
        '../interface/data_log_c.h',
        '../interface/data_log_impl.h',
        '../interface/event_wrapper.h',
        '../interface/file_wrapper.h',
        '../interface/fix_interlocked_exchange_pointer_win.h',
        '../interface/list_wrapper.h',
        '../interface/map_wrapper.h',
        '../interface/ref_count.h',
        '../interface/rw_lock_wrapper.h',
        '../interface/scoped_ptr.h',
        '../interface/scoped_refptr.h',
        '../interface/sort.h',
        '../interface/static_instance.h',
        '../interface/thread_wrapper.h',
        '../interface/tick_util.h',
        '../interface/trace.h',
        'aligned_malloc.cc',
        'atomic32.cc',
        'atomic32_linux.h',
        'atomic32_mac.h',
        'atomic32_win.h',
        'condition_variable.cc',
        'condition_variable_posix.cc',
        'condition_variable_posix.h',
        'condition_variable_win.cc',
        'condition_variable_win.h',
        'cpu.cc',
        'cpu_no_op.cc',
        'cpu_info.cc',
        'cpu_linux.cc',
        'cpu_linux.h',
        'cpu_mac.cc',
        'cpu_mac.h',
        'cpu_win.cc',
        'cpu_win.h',
        'cpu_features.cc',
        'critical_section.cc',
        'critical_section_posix.cc',
        'critical_section_posix.h',
        'critical_section_win.cc',
        'critical_section_win.h',
        'data_log.cc',
        'data_log_c.cc',
        'data_log_no_op.cc',
        'event.cc',
        'event_posix.cc',
        'event_posix.h',
        'event_win.cc',
        'event_win.h',
        'file_impl.cc',
        'file_impl.h',
        'list_no_stl.cc',
        'map.cc',
        'rw_lock.cc',
        'rw_lock_posix.cc',
        'rw_lock_posix.h',
        'rw_lock_win.cc',
        'rw_lock_win.h',
        'sort.cc',
        'thread.cc',
        'thread_posix.cc',
        'thread_posix.h',
        'thread_win.cc',
        'thread_win.h',
        'set_thread_name_win.h',
        'trace_impl.cc',
        'trace_impl.h',
        'trace_impl_no_op.cc',
        'trace_posix.cc',
        'trace_posix.h',
        'trace_win.cc',
        'trace_win.h',
      ],
      'conditions': [
        ['enable_data_logging==1', {
          'sources!': [ 'data_log_no_op.cc', ],
        },{
          'sources!': [ 'data_log.cc', ],
        },],
        ['OS=="linux"', {
          'link_settings': {
            'libraries': [ '-lrt', ],
          },
        }],
        ['OS=="mac"', {
          'link_settings': {
            'libraries': [ '$(SDKROOT)/System/Library/Frameworks/ApplicationServices.framework', ],
          },
        }],
        ['OS=="win"', {
          'link_settings': {
            'libraries': [ '-lwinmm.lib', ],
          },
        }],
        ['build_with_chromium==1', {
          'sources!': [
            'cpu.cc',
            'cpu_linux.h',
            'cpu_mac.h',
            'cpu_win.h',
            'trace_impl.cc',
            'trace_impl.h',
            'trace_posix.cc',
            'trace_posix.h',
            'trace_win.cc',
            'trace_win.h',
          ],
        }, {
          'sources!': [
            'cpu_no_op.cc',
            'trace_impl_no_op.cc',
          ],
        }]
      ] # conditions
    },
  ], # targets
  'conditions': [
    ['build_with_chromium==0', {
      'targets': [
        {
          'target_name': 'system_wrappers_unittests',
          'type': 'executable',
          'dependencies': [
            'system_wrappers',
            '<(webrtc_root)/../testing/gtest.gyp:gtest',
            '<(webrtc_root)/../test/test.gyp:test_support_main',
          ],
          'sources': [
            'cpu_wrapper_unittest.cc',
            'cpu_measurement_harness.h',
            'cpu_measurement_harness.cc',
            'list_unittest.cc',
            'map_unittest.cc',
            'data_log_unittest.cc',
            'data_log_unittest_disabled.cc',
            'data_log_helpers_unittest.cc',
            'data_log_c_helpers_unittest.c',
            'data_log_c_helpers_unittest.h',
            'trace_unittest.cc',
          ],
          'conditions': [
            ['enable_data_logging==1', {
              'sources!': [ 'data_log_unittest_disabled.cc', ],
            }, {
              'sources!': [ 'data_log_unittest.cc', ],
            }],
          ],
        },
      ], # targets
    }], # build_with_chromium
  ], # conditions
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
