# Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'targets': [
    {
      'target_name': 'signal_processing',
      'type': 'static_library',
      'include_dirs': [
        'include',
      ],
      'dependencies': [
        '<(webrtc_root)/system_wrappers/source/system_wrappers.gyp:system_wrappers',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          'include',
        ],
      },
      'sources': [
        'include/real_fft.h',
        'include/signal_processing_library.h',
        'include/spl_inl.h',
        'auto_corr_to_refl_coef.c',
        'auto_correlation.c',
        'complex_fft.c',
        'complex_bit_reverse.c',
        'copy_set_operations.c',
        'cross_correlation.c',
        'division_operations.c',
        'dot_product_with_scale.c',
        'downsample_fast.c',
        'energy.c',
        'filter_ar.c',
        'filter_ar_fast_q12.c',
        'filter_ma_fast_q12.c',
        'get_hanning_window.c',
        'get_scaling_square.c',
        'ilbc_specific_functions.c',
        'levinson_durbin.c',
        'lpc_to_refl_coef.c',
        'min_max_operations.c',
        'randomization_functions.c',
        'refl_coef_to_lpc.c',
        'real_fft.c',
        'resample.c',
        'resample_48khz.c',
        'resample_by_2.c',
        'resample_by_2_internal.c',
        'resample_by_2_internal.h',
        'resample_fractional.c',
        'spl_init.c',
        'spl_sqrt.c',
        'spl_sqrt_floor.c',
        'spl_version.c',
        'splitting_filter.c',
        'sqrt_of_one_minus_x_squared.c',
        'vector_scaling_operations.c',
      ],
      'conditions': [
        ['target_arch=="arm"', {
          'sources': [
            'complex_bit_reverse_arm.S',
            'spl_sqrt_floor_arm.S',
          ],
          'sources!': [
            'complex_bit_reverse.c',
            'spl_sqrt_floor.c',
          ],
          'conditions': [
            ['armv7==1', {
              'dependencies': ['signal_processing_neon',],
              'sources': [
                'filter_ar_fast_q12_armv7.S',
              ],
              'sources!': [
                'filter_ar_fast_q12.c',
              ],
            }],
          ],
        }],
        ['target_arch=="mipsel"', {
          'sources': [
            'min_max_operations_mips.c',
            'resample_by_2_mips.c',
          ],
        }],
      ],
      # Ignore warning on shift operator promotion.
      'msvs_disabled_warnings': [ 4334, ],
    }, # spl
  ], # targets
  'conditions': [
    ['include_tests==1', {
      'targets': [
        {
          'target_name': 'signal_processing_unittests',
          'type': 'executable',
          'dependencies': [
            'signal_processing',
            '<(DEPTH)/testing/gtest.gyp:gtest',
            '<(webrtc_root)/test/test.gyp:test_support_main',
          ],
          'sources': [
            'real_fft_unittest.cc',
            'signal_processing_unittest.cc',
          ],
        }, # spl_unittests
      ], # targets
    }], # include_tests
    ['target_arch=="arm" and armv7==1', {
      'targets': [
        {
          'target_name': 'signal_processing_neon',
          'type': 'static_library',
          'includes': ['../../build/arm_neon.gypi',],
          'sources': [
            'cross_correlation_neon.S',
            'downsample_fast_neon.S',
            'min_max_operations_neon.S',
            'vector_scaling_operations_neon.S',
          ],
        },
      ],
    }], # 'target_arch=="arm" and armv7==1'
  ], # conditions
}
