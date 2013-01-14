# Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
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
        '../interface/atomic32.h',
        '../interface/compile_assert.h',
        '../interface/condition_variable_wrapper.h',
        '../interface/cpu_info.h',
        '../interface/cpu_wrapper.h',
        '../interface/cpu_features_wrapper.h',
        '../interface/critical_section_wrapper.h',
        '../interface/data_log.h',
        '../interface/data_log_c.h',
        '../interface/data_log_impl.h',
        '../interface/event_tracer.h',
        '../interface/event_wrapper.h',
        '../interface/file_wrapper.h',
        '../interface/fix_interlocked_exchange_pointer_win.h',
        '../interface/list_wrapper.h',
        '../interface/logging.h',
        '../interface/map_wrapper.h',
        '../interface/ref_count.h',
        '../interface/rw_lock_wrapper.h',
        '../interface/scoped_ptr.h',
        '../interface/scoped_refptr.h',
        '../interface/sleep.h',
        '../interface/sort.h',
        '../interface/static_instance.h',
        '../interface/thread_wrapper.h',
        '../interface/tick_util.h',
        '../interface/trace.h',
        '../interface/trace_event.h',
        'aligned_malloc.cc',
        'atomic32_mac.cc',
        'atomic32_posix.cc',
        'atomic32_win.cc',
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
        'event_tracer.cc',
        'event_win.cc',
        'event_win.h',
        'file_impl.cc',
        'file_impl.h',
        'list_no_stl.cc',
        'logging.cc',
        'logging_no_op.cc',
        'map.cc',
        'rw_lock.cc',
        'rw_lock_generic.cc',
        'rw_lock_generic.h',
        'rw_lock_posix.cc',
        'rw_lock_posix.h',
        'rw_lock_win.cc',
        'rw_lock_win.h',
        'set_thread_name_win.h',
        'sleep.cc',
        'sort.cc',
        'tick_util.cc',
        'thread.cc',
        'thread_posix.cc',
        'thread_posix.h',
        'thread_win.cc',
        'thread_win.h',
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
        }, {
          'sources!': [ 'data_log.cc', ],
        },],
        ['enable_tracing==1', {
          'sources!': [
            'logging_no_op.cc',
            'trace_impl_no_op.cc',
          ],
        }, {
          'sources!': [
            'logging.cc',
            'trace_impl.cc',
            'trace_impl.h',
            'trace_posix.cc',
            'trace_posix.h',
            'trace_win.cc',
            'trace_win.h',
          ],
        }],
        ['OS=="android"', {
          'dependencies': [ 'cpu_features_android', ],
        }],
        ['OS=="linux"', {
          'link_settings': {
            'libraries': [ '-lrt', ],
          },
        }],
        ['OS=="mac"', {
          'link_settings': {
            'libraries': [ '$(SDKROOT)/System/Library/Frameworks/ApplicationServices.framework', ],
          },
          'sources!': [
            'atomic32_posix.cc',
          ],
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
          ],
        }, {
          'sources!': [
            'cpu_no_op.cc',
          ],
        }],
      ], # conditions
      'target_conditions': [
        # We need to do this in a target_conditions block to override the
        # filename_rules filters.
        ['OS=="ios"', {
          # Pull in specific Mac files for iOS (which have been filtered out
          # by file name rules).
          'sources/': [
            ['include', '^atomic32_mac\\.'],
            ['include', '^cpu_mac\\.'],
          ],
          'sources!': [
            'atomic32_posix.cc',
          ],
        }],
      ],
    },
  ], # targets
  'conditions': [
    ['OS=="android"', {
      'targets': [
        {
          'variables': {
            # Treat this as third-party code.
            'chromium_code': 0,
          },
          'target_name': 'cpu_features_android',
          'type': '<(library)',
          'sources': [
            # TODO(leozwang): Ideally we want to audomatically exclude .c files
            # as with .cc files, gyp currently only excludes .cc files.
            'cpu_features_android.c',
          ],
          'conditions': [
            ['build_with_chromium==1', {
              'conditions': [
                ['android_build_type != 0', {
                  'libraries': [
                    'cpufeatures.a'
                  ],
                }, {
                  'dependencies': [
                    '<(android_ndk_root)/android_tools_ndk.gyp:cpu_features',
                  ],
                }],
              ],
            }, {
              'sources': [
                'android/cpu-features.c',
                'android/cpu-features.h',
              ],
            }],
          ],
        },
      ],
    }],
    ['include_tests==1', {
      'targets': [
        {
          'target_name': 'system_wrappers_unittests',
          'type': 'executable',
          'dependencies': [
            'system_wrappers',
            '<(DEPTH)/testing/gtest.gyp:gtest',
            '<(webrtc_root)/test/test.gyp:test_support_main',
          ],
          'sources': [
            'aligned_malloc_unittest.cc',
            'condition_variable_unittest.cc',
            'cpu_wrapper_unittest.cc',
            'cpu_measurement_harness.h',
            'cpu_measurement_harness.cc',
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
            'thread_unittest.cc',
            'thread_posix_unittest.cc',
            'trace_unittest.cc',
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
        },
      ], # targets
    }], # include_tests
  ], # conditions
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
