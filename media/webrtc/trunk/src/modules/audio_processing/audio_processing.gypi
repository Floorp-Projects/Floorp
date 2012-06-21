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
      'type': '<(library)',
      'conditions': [
        ['prefer_fixed_point==1', {
          'dependencies': [ 'ns_fix' ],
          'defines': [ 'WEBRTC_NS_FIXED' ],
        }, {
          'dependencies': [ 'ns' ],
          'defines': [ 'WEBRTC_NS_FLOAT' ],
        }],
        ['enable_protobuf==1', {
          'dependencies': [ 'audioproc_debug_proto' ],
          'defines': [ 'WEBRTC_AUDIOPROC_DEBUG_DUMP' ],
        }],
      ],
      'dependencies': [
        'aec',
        'aecm',
        'agc',
        '<(webrtc_root)/common_audio/common_audio.gyp:signal_processing',
        '<(webrtc_root)/common_audio/common_audio.gyp:vad',
        '<(webrtc_root)/system_wrappers/source/system_wrappers.gyp:system_wrappers',
      ],
      'include_dirs': [
        'include',
        '../interface',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          'include',
          '../interface',
        ],
      },
      'sources': [
        'include/audio_processing.h',
        'audio_buffer.cc',
        'audio_buffer.h',
        'audio_processing_impl.cc',
        'audio_processing_impl.h',
        'echo_cancellation_impl.cc',
        'echo_cancellation_impl.h',
        'echo_control_mobile_impl.cc',
        'echo_control_mobile_impl.h',
        'gain_control_impl.cc',
        'gain_control_impl.h',
        'high_pass_filter_impl.cc',
        'high_pass_filter_impl.h',
        'level_estimator_impl.cc',
        'level_estimator_impl.h',
        'noise_suppression_impl.cc',
        'noise_suppression_impl.h',
        'splitting_filter.cc',
        'splitting_filter.h',
        'processing_component.cc',
        'processing_component.h',
        'voice_detection_impl.cc',
        'voice_detection_impl.h',
      ],
    },
  ],
  'conditions': [
    ['enable_protobuf==1', {
      'targets': [
        {
          'target_name': 'audioproc_debug_proto',
          'type': 'static_library',
          'sources': [ 'debug.proto', ],
          'variables': {
            'proto_in_dir': '.',
            # Workaround to protect against gyp's pathname relativization when
            # this file is included by modules.gyp.
            'proto_out_protected': 'webrtc/audio_processing',
            'proto_out_dir': '<(proto_out_protected)',
          },
          'includes': [ '../../build/protoc.gypi', ],
        },
      ],
    }],
  ],
}
