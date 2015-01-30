# Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'variables': {
    'audio_coding_dependencies': [
      'CNG',
      '<(webrtc_root)/common_audio/common_audio.gyp:common_audio',
      '<(webrtc_root)/system_wrappers/source/system_wrappers.gyp:system_wrappers',
    ],
    'audio_coding_defines': [],
    'conditions': [
      ['include_opus==1', {
        'audio_coding_dependencies': ['webrtc_opus',],
        'audio_coding_defines': ['WEBRTC_CODEC_OPUS',],
      }],
      ['include_g711==1', {
        'audio_coding_dependencies': ['G711',],
        'audio_coding_defines': ['WEBRTC_CODEC_G711',],
      }],
      ['include_g722==1', {
        'audio_coding_dependencies': ['G722',],
        'audio_coding_defines': ['WEBRTC_CODEC_G722',],
      }],
      ['include_ilbc==1', {
        'audio_coding_dependencies': ['iLBC',],
        'audio_coding_defines': ['WEBRTC_CODEC_ILBC',],
      }],
      ['include_isac==1', {
        'audio_coding_dependencies': ['iSAC', 'iSACFix',],
        'audio_coding_defines': ['WEBRTC_CODEC_ISAC', 'WEBRTC_CODEC_ISACFX',],
      }],
      ['include_pcm16b==1', {
        'audio_coding_dependencies': ['PCM16B',],
        'audio_coding_defines': ['WEBRTC_CODEC_PCM16',],
      }],
    ],
  },
  'targets': [
    {
      'target_name': 'audio_coding_module',
      'type': 'static_library',
      'defines': [
        '<@(audio_coding_defines)',
      ],
      'dependencies': [
        '<@(audio_coding_dependencies)',
        'neteq',
      ],
      'include_dirs': [
        '../interface',
        '../../../interface',
        '<(webrtc_root)',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '../interface',
          '../../../interface',
          '<(webrtc_root)',
        ],
      },
      'sources': [
        '../interface/audio_coding_module.h',
        '../interface/audio_coding_module_typedefs.h',
        'acm_cng.cc',
        'acm_cng.h',
        'acm_codec_database.cc',
        'acm_codec_database.h',
        'acm_common_defs.h',
        'acm_dtmf_playout.cc',
        'acm_dtmf_playout.h',
        'acm_generic_codec.cc',
        'acm_generic_codec.h',
        'acm_red.cc',
        'acm_red.h',
        'acm_receiver.cc',
        'acm_receiver.h',
        'acm_resampler.cc',
        'acm_resampler.h',
        'audio_coding_module.cc',
        'audio_coding_module_impl.cc',
        'audio_coding_module_impl.h',
        'call_statistics.cc',
        'call_statistics.h',
        'initial_delay_manager.cc',
        'initial_delay_manager.h',
        'nack.cc',
        'nack.h',
      ],
      'conditions': [
        ['include_opus==1', {
          'sources': [
            'acm_opus.cc',
           'acm_opus.h',
          ],
        }],
        ['include_g711==1', {
          'sources': [
            'acm_pcma.cc',
            'acm_pcma.h',
            'acm_pcmu.cc',
            'acm_pcmu.h',
          ],
        }],
        ['include_g722==1', {
          'sources': [
             'acm_g722.cc',
             'acm_g722.h',
          ],
        }],
        ['include_ilbc==1', {
          'sources': [
#           FIX
          ],
        }],
        ['include_isac==1', {
          'sources': [
#           FIX
          ],
        }],
        ['include_pcm16b==1', {
          'sources': [
            'acm_pcm16b.cc',
            'acm_pcm16b.h',
          ],
        }],
      ],
    },
  ],
  'conditions': [
    ['include_tests==1', {
      'targets': [
        {
          'target_name': 'acm_receive_test',
          'type': 'static_library',
          'defines': [
            '<@(audio_coding_defines)',
          ],
          'dependencies': [
            '<@(audio_coding_dependencies)',
            'audio_coding_module',
            'neteq_unittest_tools',
            '<(DEPTH)/testing/gtest.gyp:gtest',
          ],
          'sources': [
            'acm_receive_test.cc',
            'acm_receive_test.h',
            'acm_receive_test_oldapi.cc',
            'acm_receive_test_oldapi.h',
          ],
        }, # acm_receive_test
        {
          'target_name': 'acm_send_test',
          'type': 'static_library',
          'defines': [
            '<@(audio_coding_defines)',
          ],
          'dependencies': [
            '<@(audio_coding_dependencies)',
            'audio_coding_module',
            'neteq_unittest_tools',
            '<(DEPTH)/testing/gtest.gyp:gtest',
          ],
          'sources': [
            'acm_send_test.cc',
            'acm_send_test.h',
            'acm_send_test_oldapi.cc',
            'acm_send_test_oldapi.h',
          ],
        }, # acm_send_test
        {
          'target_name': 'delay_test',
          'type': 'executable',
          'dependencies': [
            'audio_coding_module',
            '<(DEPTH)/testing/gtest.gyp:gtest',
            '<(webrtc_root)/test/test.gyp:test_support',
            '<(webrtc_root)/system_wrappers/source/system_wrappers.gyp:system_wrappers',
            '<(webrtc_root)/system_wrappers/source/system_wrappers.gyp:system_wrappers_default',
            '<(DEPTH)/third_party/gflags/gflags.gyp:gflags',
          ],
          'sources': [
             '../test/delay_test.cc',
             '../test/Channel.cc',
             '../test/PCMFile.cc',
             '../test/utility.cc',
           ],
        }, # delay_test
        {
          'target_name': 'insert_packet_with_timing',
          'type': 'executable',
          'dependencies': [
            'audio_coding_module',
            '<(DEPTH)/testing/gtest.gyp:gtest',
            '<(webrtc_root)/test/test.gyp:test_support',
            '<(webrtc_root)/system_wrappers/source/system_wrappers.gyp:system_wrappers',
            '<(webrtc_root)/system_wrappers/source/system_wrappers.gyp:system_wrappers_default',
            '<(DEPTH)/third_party/gflags/gflags.gyp:gflags',
          ],
          'sources': [
             '../test/insert_packet_with_timing.cc',
             '../test/Channel.cc',
             '../test/PCMFile.cc',
           ],
        }, # delay_test
      ],
    }],
  ],
}
