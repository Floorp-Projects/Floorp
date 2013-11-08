# Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'targets': [
    {
      'target_name': 'audio_processing',
      'type': 'static_library',
      'variables': {
        # Outputs some low-level debug files.
        'aec_debug_dump%': 0,
      },
      'dependencies': [
        '<(webrtc_root)/common_audio/common_audio.gyp:common_audio',
        '<(webrtc_root)/system_wrappers/source/system_wrappers.gyp:system_wrappers',
      ],
      'include_dirs': [
        '../interface',
        'aec/include',
        'aecm/include',
        'agc/include',
        'include',
        'ns/include',
        'utility',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '../interface',
          'include',
        ],
      },
      'sources': [
        'aec/include/echo_cancellation.h',
        'aec/echo_cancellation.c',
        'aec/echo_cancellation_internal.h',
        'aec/aec_core.h',
        'aec/aec_core.c',
        'aec/aec_core_internal.h',
        'aec/aec_rdft.h',
        'aec/aec_rdft.c',
        'aec/aec_resampler.h',
        'aec/aec_resampler.c',
        'aecm/include/echo_control_mobile.h',
        'aecm/echo_control_mobile.c',
        'aecm/aecm_core.c',
        'aecm/aecm_core.h',
        'agc/include/gain_control.h',
        'agc/analog_agc.c',
        'agc/analog_agc.h',
        'agc/digital_agc.c',
        'agc/digital_agc.h',
        'audio_buffer.cc',
        'audio_buffer.h',
        'audio_processing_impl.cc',
        'audio_processing_impl.h',
        'echo_cancellation_impl.cc',
        'echo_cancellation_impl.h',
        'echo_cancellation_impl_wrapper.h',
        'echo_control_mobile_impl.cc',
        'echo_control_mobile_impl.h',
        'gain_control_impl.cc',
        'gain_control_impl.h',
        'high_pass_filter_impl.cc',
        'high_pass_filter_impl.h',
        'include/audio_processing.h',
        'level_estimator_impl.cc',
        'level_estimator_impl.h',
        'noise_suppression_impl.cc',
        'noise_suppression_impl.h',
        'splitting_filter.cc',
        'splitting_filter.h',
        'processing_component.cc',
        'processing_component.h',
        'utility/delay_estimator.c',
        'utility/delay_estimator.h',
        'utility/delay_estimator_internal.h',
        'utility/delay_estimator_wrapper.c',
        'utility/delay_estimator_wrapper.h',
        'utility/fft4g.c',
        'utility/fft4g.h',
        'utility/ring_buffer.c',
        'utility/ring_buffer.h',
        'voice_detection_impl.cc',
        'voice_detection_impl.h',
      ],
      'conditions': [
        ['aec_debug_dump==1', {
          'defines': ['WEBRTC_AEC_DEBUG_DUMP',],
        }],
        ['enable_protobuf==1', {
          'dependencies': ['audioproc_debug_proto'],
          'defines': ['WEBRTC_AUDIOPROC_DEBUG_DUMP'],
        }],
        ['prefer_fixed_point==1', {
          'defines': ['WEBRTC_NS_FIXED'],
          'sources': [
            'ns/include/noise_suppression_x.h',
            'ns/noise_suppression_x.c',
            'ns/nsx_core.c',
            'ns/nsx_core.h',
            'ns/nsx_defines.h',
          ],
        }, {
          'defines': ['WEBRTC_NS_FLOAT'],
          'sources': [
            'ns/defines.h',
            'ns/include/noise_suppression.h',
            'ns/noise_suppression.c',
            'ns/ns_core.c',
            'ns/ns_core.h',
            'ns/windows_private.h',
          ],
        }],
        ['target_arch=="ia32" or target_arch=="x64"', {
          'dependencies': ['audio_processing_sse2',],
        }],
        ['(target_arch=="arm" and armv7==1) or target_arch=="armv7"', {
          'dependencies': ['audio_processing_neon',],
        }],
      ],
      # TODO(jschuh): Bug 1348: fix size_t to int truncations.
      'msvs_disabled_warnings': [ 4267, ],
    },
  ],
  'conditions': [
    ['enable_protobuf==1', {
      'targets': [
        {
          'target_name': 'audioproc_debug_proto',
          'type': 'static_library',
          'sources': ['debug.proto',],
          'variables': {
            'proto_in_dir': '.',
            # Workaround to protect against gyp's pathname relativization when
            # this file is included by modules.gyp.
            'proto_out_protected': 'webrtc/audio_processing',
            'proto_out_dir': '<(proto_out_protected)',
          },
          'includes': ['../../build/protoc.gypi',],
        },
      ],
    }],
    ['target_arch=="ia32" or target_arch=="x64"', {
      'targets': [
        {
          'target_name': 'audio_processing_sse2',
          'type': 'static_library',
          'sources': [
            'aec/aec_core_sse2.c',
            'aec/aec_rdft_sse2.c',
          ],
          'cflags': ['-msse2',],
          'cflags_mozilla': [ '-msse2', ],
          'xcode_settings': {
            'OTHER_CFLAGS': ['-msse2',],
          },
        },
      ],
    }],
    ['(target_arch=="arm" and armv7==1) or target_arch=="armv7"', {
      'targets': [{
        'target_name': 'audio_processing_neon',
        'type': 'static_library',
        'includes': ['../../build/arm_neon.gypi',],
        'dependencies': [
          '<(webrtc_root)/common_audio/common_audio.gyp:common_audio',
        ],
        'sources': [
          'aecm/aecm_core_neon.c',
          'ns/nsx_core_neon.c',
        ],
        'conditions': [
          ['OS=="android" or OS=="ios"', {
            'dependencies': [
              'audio_processing_offsets',
            ],
	    #
	    # We disable the ASM source, because our gyp->Makefile translator
	    # does not support the build steps to get the asm offsets.
            'sources!': [
              'aecm/aecm_core_neon.S',
              'ns/nsx_core_neon.S',
            ],
            'sources': [
              'aecm/aecm_core_neon.c',
              'ns/nsx_core_neon.c',
            ],
            'includes!': ['../../build/arm_neon.gypi',],
          }],
        ],
      }],
      'conditions': [
        ['OS=="android" or OS=="ios"', {
          'targets': [{
            'target_name': 'audio_processing_offsets',
            'type': 'none',
            'sources': [
              'aecm/aecm_core_neon_offsets.c',
              'ns/nsx_core_neon_offsets.c',
            ],
            'variables': {
              'asm_header_dir': 'asm_offsets',
            },
            'includes': ['../../build/generate_asm_header.gypi',],
          }],
        }],
      ],
    }],
  ],
}
