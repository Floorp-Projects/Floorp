# Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'includes': [ '../build/common.gypi', ],
  'targets': [
    {
      'target_name': 'system_wrappers',
      'type': 'static_library',
      'dependencies': [
        '<(webrtc_root)/common.gyp:webrtc_common',
        '../base/base.gyp:rtc_base_approved',
      ],
      'sources': [
        'interface/aligned_array.h',
        'interface/aligned_malloc.h',
        'interface/atomic32.h',
        'interface/clock.h',
        'interface/condition_variable_wrapper.h',
        'interface/cpu_info.h',
        'interface/cpu_features_wrapper.h',
        'interface/critical_section_wrapper.h',
        'interface/data_log.h',
        'interface/data_log_c.h',
        'interface/data_log_impl.h',
        'interface/event_tracer.h',
        'interface/event_wrapper.h',
        'interface/field_trial.h',
        'interface/file_wrapper.h',
        'interface/fix_interlocked_exchange_pointer_win.h',
        'interface/logcat_trace_context.h',
        'interface/logging.h',
        'interface/metrics.h',
        'interface/ref_count.h',
        'interface/rtp_to_ntp.h',
        'interface/rw_lock_wrapper.h',
        'interface/scoped_refptr.h',
        'interface/scoped_vector.h',
        'interface/sleep.h',
        'interface/sort.h',
        'interface/static_instance.h',
        'interface/stl_util.h',
        'interface/stringize_macros.h',
        'interface/thread_wrapper.h',
        'interface/tick_util.h',
        'interface/timestamp_extrapolator.h',
        'interface/trace.h',
        'interface/trace_event.h',
        'interface/utf_util_win.h',
        'source/aligned_malloc.cc',
        'source/atomic32_mac.cc',
        'source/atomic32_posix.cc',
        'source/atomic32_win.cc',
        'source/clock.cc',
        'source/condition_variable.cc',
        'source/condition_variable_posix.cc',
        'source/condition_variable_posix.h',
        'source/condition_variable_event_win.cc',
        'source/condition_variable_event_win.h',
        'source/condition_variable_native_win.cc',
        'source/condition_variable_native_win.h',
        'source/cpu_info.cc',
        'source/cpu_features.cc',
        'source/critical_section.cc',
        'source/critical_section_posix.cc',
        'source/critical_section_posix.h',
        'source/critical_section_win.cc',
        'source/critical_section_win.h',
        'source/data_log.cc',
        'source/data_log_c.cc',
        'source/data_log_no_op.cc',
        'source/event.cc',
        'source/event_posix.cc',
        'source/event_posix.h',
        'source/event_tracer.cc',
        'source/event_win.cc',
        'source/event_win.h',
        'source/file_impl.cc',
        'source/file_impl.h',
        'source/logcat_trace_context.cc',
        'source/logging.cc',
        'source/rtp_to_ntp.cc',
        'source/rw_lock.cc',
        'source/rw_lock_generic.cc',
        'source/rw_lock_generic.h',
        'source/rw_lock_posix.cc',
        'source/rw_lock_posix.h',
        'source/rw_lock_win.cc',
        'source/rw_lock_win.h',
        'source/sleep.cc',
        'source/sort.cc',
        'source/tick_util.cc',
        'source/thread.cc',
        'source/thread_posix.cc',
        'source/thread_posix.h',
        'source/thread_win.cc',
        'source/thread_win.h',
        'source/timestamp_extrapolator.cc',
        'source/trace_impl.cc',
        'source/trace_impl.h',
        'source/trace_posix.cc',
        'source/trace_posix.h',
        'source/trace_win.cc',
        'source/trace_win.h',
      ],
      'conditions': [
        ['enable_data_logging==1', {
          'sources!': [ 'source/data_log_no_op.cc', ],
        }, {
          'sources!': [ 'source/data_log.cc', ],
        },],
        ['build_with_mozilla', {
          'sources': [
            'source/metrics_default.cc',
          ],
        }],
        ['OS=="android" or moz_widget_toolkit_gonk==1', {
          'defines': [
            'WEBRTC_THREAD_RR',
            # TODO(leozwang): Investigate CLOCK_REALTIME and CLOCK_MONOTONIC
            # support on Android. Keep WEBRTC_CLOCK_TYPE_REALTIME for now,
            # remove it after I verify that CLOCK_MONOTONIC is fully functional
            # with condition and event functions in system_wrappers.
            'WEBRTC_CLOCK_TYPE_REALTIME',
           ],
          'conditions': [
            ['build_with_chromium==1', {
              'dependencies': [
                'cpu_features_chromium.gyp:cpu_features_android',
              ],
            }, {
              'dependencies': [
                'cpu_features_webrtc.gyp:cpu_features_android',
              ],
            }],
          ],
          'sources!': [
            # Android doesn't have these in <=2.2
            'rw_lock_posix.cc',
            'rw_lock_posix.h',
          ],
          'link_settings': {
            'libraries': [
              '-llog',
            ],
          },
        }, {  # OS!="android"
          'sources!': [
            'interface/logcat_trace_context.h',
            'source/logcat_trace_context.cc',
          ],
        }],
        ['OS=="linux"', {
          'defines': [
            'WEBRTC_THREAD_RR',
            # TODO(andrew): can we select this automatically?
            # Define this if the Linux system does not support CLOCK_MONOTONIC.
            #'WEBRTC_CLOCK_TYPE_REALTIME',
          ],
          'link_settings': {
            'libraries': [ '-lrt', ],
          },
        }],
        ['OS=="mac"', {
          'link_settings': {
            'libraries': [ '$(SDKROOT)/System/Library/Frameworks/ApplicationServices.framework', ],
          },
          'sources!': [
            'source/atomic32_posix.cc',
          ],
        }],
        ['OS=="ios" or OS=="mac"', {
          'defines': [
            'WEBRTC_THREAD_RR',
            'WEBRTC_CLOCK_TYPE_REALTIME',
          ],
        }],
        ['OS=="win"', {
          'link_settings': {
            'libraries': [ '-lwinmm.lib', ],
          },
        }],
      ], # conditions
      'target_conditions': [
        # We need to do this in a target_conditions block to override the
        # filename_rules filters.
        ['OS=="ios"', {
          # Pull in specific Mac files for iOS (which have been filtered out
          # by file name rules).
          'sources/': [
            ['include', '^source/atomic32_mac\\.'],
          ],
          'sources!': [
            'source/atomic32_posix.cc',
          ],
        }],
      ],
      # Disable warnings to enable Win64 build, issue 1323.
      'msvs_disabled_warnings': [
        4267,  # size_t to int truncation.
        4334,  # Ignore warning on shift operator promotion.
      ],
    }, {
      'target_name': 'field_trial_default',
      'type': 'static_library',
      'sources': [
        'interface/field_trial_default.h',
        'source/field_trial_default.cc',
      ],
      'dependencies': [
        'system_wrappers',
      ]
    }, {
      'target_name': 'metrics_default',
      'type': 'static_library',
      'sources': [
        'source/metrics_default.cc',
      ],
      'dependencies': [
        'system_wrappers',
      ]
    }, {
      'target_name': 'system_wrappers_default',
      'type': 'static_library',
      'dependencies': [
        'field_trial_default',
        'metrics_default',
      ]
    },
  ], # targets
}

