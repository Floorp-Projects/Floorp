# Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'includes': [
    '../build/common.gypi',
  ],
  'targets': [
    {
      'target_name': 'common_audio',
      'type': 'static_library',
      'dependencies': [
        '<(webrtc_root)/system_wrappers/source/system_wrappers.gyp:system_wrappers',
      ],
      'include_dirs': [
        'resampler/include',
        'signal_processing/include',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          'resampler/include',
          'signal_processing/include',
          'vad/include',
        ],
      },
      'sources': [
        'audio_util.cc',
        'include/audio_util.h',
        'resampler/include/push_resampler.h',
        'resampler/include/resampler.h',
        'resampler/push_resampler.cc',
        'resampler/push_sinc_resampler.cc',
        'resampler/push_sinc_resampler.h',
        'resampler/resampler.cc',
        'resampler/sinc_resampler.cc',
        'resampler/sinc_resampler.h',
        'signal_processing/include/real_fft.h',
        'signal_processing/include/signal_processing_library.h',
        'signal_processing/include/spl_inl.h',
        'signal_processing/auto_corr_to_refl_coef.c',
        'signal_processing/auto_correlation.c',
        'signal_processing/complex_fft.c',
        'signal_processing/complex_fft_tables.h',
        'signal_processing/complex_bit_reverse.c',
        'signal_processing/copy_set_operations.c',
        'signal_processing/cross_correlation.c',
        'signal_processing/division_operations.c',
        'signal_processing/dot_product_with_scale.c',
        'signal_processing/downsample_fast.c',
        'signal_processing/energy.c',
        'signal_processing/filter_ar.c',
        'signal_processing/filter_ar_fast_q12.c',
        'signal_processing/filter_ma_fast_q12.c',
        'signal_processing/get_hanning_window.c',
        'signal_processing/get_scaling_square.c',
        'signal_processing/ilbc_specific_functions.c',
        'signal_processing/levinson_durbin.c',
        'signal_processing/lpc_to_refl_coef.c',
        'signal_processing/min_max_operations.c',
        'signal_processing/randomization_functions.c',
        'signal_processing/refl_coef_to_lpc.c',
        'signal_processing/real_fft.c',
        'signal_processing/resample.c',
        'signal_processing/resample_48khz.c',
        'signal_processing/resample_by_2.c',
        'signal_processing/resample_by_2_internal.c',
        'signal_processing/resample_by_2_internal.h',
        'signal_processing/resample_fractional.c',
        'signal_processing/spl_init.c',
        'signal_processing/spl_sqrt.c',
        'signal_processing/spl_sqrt_floor.c',
        'signal_processing/spl_version.c',
        'signal_processing/splitting_filter.c',
        'signal_processing/sqrt_of_one_minus_x_squared.c',
        'signal_processing/vector_scaling_operations.c',
        'vad/include/webrtc_vad.h',
        'vad/webrtc_vad.c',
        'vad/vad_core.c',
        'vad/vad_core.h',
        'vad/vad_filterbank.c',
        'vad/vad_filterbank.h',
        'vad/vad_gmm.c',
        'vad/vad_gmm.h',
        'vad/vad_sp.c',
        'vad/vad_sp.h',
      ],
      'conditions': [
        ['target_arch=="ia32" or target_arch=="x64"', {
          'dependencies': ['common_audio_sse2',],
        }],
        ['target_arch=="arm"', {
          'sources': [
            'signal_processing/complex_bit_reverse_arm.S',
            'signal_processing/spl_sqrt_floor_arm.S',
          ],
          'sources!': [
            'signal_processing/complex_bit_reverse.c',
            'signal_processing/spl_sqrt_floor.c',
          ],
          'conditions': [
            ['armv7==1', {
              'dependencies': ['common_audio_neon',],
              'sources': [
                'signal_processing/filter_ar_fast_q12_armv7.S',
              ],
              'sources!': [
                'signal_processing/filter_ar_fast_q12.c',
              ],
            }],
          ],  # conditions
        }],
        ['target_arch=="mipsel"', {
          'sources': [
            'signal_processing/complex_bit_reverse_mips.c',
            'signal_processing/complex_fft_mips.c',
            'signal_processing/downsample_fast_mips.c',
            'signal_processing/filter_ar_fast_q12_mips.c',
            'signal_processing/min_max_operations_mips.c',
            'signal_processing/resample_by_2_mips.c',
          ],
          'sources!': [
            'signal_processing/complex_bit_reverse.c',
            'signal_processing/complex_fft.c',
            'signal_processing/filter_ar_fast_q12.c',
          ],
        }],
      ],  # conditions
      # Ignore warning on shift operator promotion.
      'msvs_disabled_warnings': [ 4334, ],
    },
  ],  # targets
  'conditions': [
    ['target_arch=="ia32" or target_arch=="x64"', {
      'targets': [
        {
          'target_name': 'common_audio_sse2',
          'type': 'static_library',
          'sources': [
            'resampler/sinc_resampler_sse.cc',
          ],
          'cflags': ['-msse2',],
          'cflags_mozilla': ['-msse2',],
          'xcode_settings': {
            'OTHER_CFLAGS': ['-msse2',],
          },
        },
      ],  # targets
    }],
    ['target_arch=="arm" and armv7==1', {
      'targets': [
        {
          'target_name': 'common_audio_neon',
          'type': 'static_library',
          'includes': ['../build/arm_neon.gypi',],
          'sources': [
            'resampler/sinc_resampler_neon.cc',
            'signal_processing/cross_correlation_neon.S',
            'signal_processing/downsample_fast_neon.S',
            'signal_processing/min_max_operations_neon.S',
            'signal_processing/vector_scaling_operations_neon.S',
          ],
        },
      ],  # targets
    }],
    ['include_tests==1', {
      'targets' : [
        {
          'target_name': 'common_audio_unittests',
          'type': 'executable',
          'dependencies': [
            'common_audio',
            '<(webrtc_root)/test/test.gyp:test_support_main',
            '<(DEPTH)/testing/gmock.gyp:gmock',
            '<(DEPTH)/testing/gtest.gyp:gtest',
          ],
          'sources': [
            'audio_util_unittest.cc',
            'resampler/resampler_unittest.cc',
            'resampler/push_resampler_unittest.cc',
            'resampler/push_sinc_resampler_unittest.cc',
            'resampler/sinc_resampler_unittest.cc',
            'resampler/sinusoidal_linear_chirp_source.cc',
            'resampler/sinusoidal_linear_chirp_source.h',
            'signal_processing/real_fft_unittest.cc',
            'signal_processing/signal_processing_unittest.cc',
            'vad/vad_core_unittest.cc',
            'vad/vad_filterbank_unittest.cc',
            'vad/vad_gmm_unittest.cc',
            'vad/vad_sp_unittest.cc',
            'vad/vad_unittest.cc',
            'vad/vad_unittest.h',
          ],
        },
      ],  # targets
    }],
  ],  # conditions
}
