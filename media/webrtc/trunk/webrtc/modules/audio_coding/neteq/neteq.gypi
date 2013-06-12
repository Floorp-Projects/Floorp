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
      'target_name': 'NetEq',
      'type': '<(library)',
      'dependencies': [
        'CNG',
        '<(webrtc_root)/common_audio/common_audio.gyp:signal_processing',
      ],
      'defines': [
        'NETEQ_VOICEENGINE_CODECS', # TODO: Should create a Chrome define which
        'SCRATCH',                  # specifies a subset of codecs to support.
      ],
      'include_dirs': [
        'interface',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          'interface',
        ],
      },
      'sources': [
        'interface/webrtc_neteq.h',
        'interface/webrtc_neteq_help_macros.h',
        'interface/webrtc_neteq_internal.h',
        'accelerate.c',
        'automode.c',
        'automode.h',
        'bgn_update.c',
        'buffer_stats.h',
        'bufstats_decision.c',
        'cng_internal.c',
        'codec_db.c',
        'codec_db.h',
        'codec_db_defines.h',
        'correlator.c',
        'delay_logging.h',
        'dsp.c',
        'dsp.h',
        'dsp_helpfunctions.c',
        'dsp_helpfunctions.h',
        'dtmf_buffer.c',
        'dtmf_buffer.h',
        'dtmf_tonegen.c',
        'dtmf_tonegen.h',
        'expand.c',
        'mcu.h',
        'mcu_address_init.c',
        'mcu_dsp_common.c',
        'mcu_dsp_common.h',
        'mcu_reset.c',
        'merge.c',
        'min_distortion.c',
        'mix_voice_unvoice.c',
        'mute_signal.c',
        'neteq_defines.h',
        'neteq_error_codes.h',
        'neteq_statistics.h',
        'normal.c',
        'packet_buffer.c',
        'packet_buffer.h',
        'peak_detection.c',
        'preemptive_expand.c',
        'random_vector.c',
        'recin.c',
        'recout.c',
        'rtcp.c',
        'rtcp.h',
        'rtp.c',
        'rtp.h',
        'set_fs.c',
        'signal_mcu.c',
        'split_and_insert.c',
        'unmute_signal.c',
        'webrtc_neteq.c',
      ],
    },
  ], # targets
  'conditions': [
    ['include_tests==1', {
      'targets': [
        {
          'target_name': 'neteq_unittests',
          'type': 'executable',
          'dependencies': [
            'NetEq',
            'NetEqTestTools',
            '<(DEPTH)/testing/gtest.gyp:gtest',
            '<(webrtc_root)/test/test.gyp:test_support_main',
          ],
          'sources': [
            'webrtc_neteq_unittest.cc',
          ],
        }, # neteq_unittests
        {
          'target_name': 'NetEqRTPplay',
          'type': 'executable',
          'dependencies': [
            'NetEq',          # NetEQ library defined above
            'NetEqTestTools', # Test helpers
            'G711',
            'G722',
            'PCM16B',
            'iLBC',
            'iSAC',
            'CNG',
          ],
          'defines': [
            # TODO: Make codec selection conditional on definitions in target NetEq
            'CODEC_ILBC',
            'CODEC_PCM16B',
            'CODEC_G711',
            'CODEC_G722',
            'CODEC_ISAC',
            'CODEC_PCM16B_WB',
            'CODEC_ISAC_SWB',
            'CODEC_ISAC_FB',
            'CODEC_PCM16B_32KHZ',
            'CODEC_CNGCODEC8',
            'CODEC_CNGCODEC16',
            'CODEC_CNGCODEC32',
            'CODEC_ATEVENT_DECODE',
            'CODEC_RED',
          ],
          'include_dirs': [
            '.',
            'test',
          ],
          'sources': [
            'test/NetEqRTPplay.cc',
          ],
        },
       {
          'target_name': 'RTPencode',
          'type': 'executable',
          'dependencies': [
            'NetEqTestTools',# Test helpers
            'G711',
            'G722',
            'PCM16B',
            'iLBC',
            'iSAC',
            'CNG',
            '<(webrtc_root)/common_audio/common_audio.gyp:vad',
          ],
          'defines': [
            # TODO: Make codec selection conditional on definitions in target NetEq
            'CODEC_ILBC',
            'CODEC_PCM16B',
            'CODEC_G711',
            'CODEC_G722',
            'CODEC_ISAC',
            'CODEC_PCM16B_WB',
            'CODEC_ISAC_SWB',
            'CODEC_ISAC_FB',
            'CODEC_PCM16B_32KHZ',
            'CODEC_CNGCODEC8',
            'CODEC_CNGCODEC16',
            'CODEC_CNGCODEC32',
            'CODEC_ATEVENT_DECODE',
            'CODEC_RED',
          ],
          'include_dirs': [
            'interface',
            'test',
          ],
          'sources': [
            'test/RTPencode.cc',
          ],
        },
        {
          'target_name': 'RTPjitter',
          'type': 'executable',
          'dependencies': [
            '<(DEPTH)/testing/gtest.gyp:gtest',
          ],
          'sources': [
            'test/RTPjitter.cc',
          ],
        },
        {
          'target_name': 'RTPanalyze',
          'type': 'executable',
          'dependencies': [
            'NetEqTestTools',
            '<(DEPTH)/testing/gtest.gyp:gtest',
          ],
          'sources': [
            'test/RTPanalyze.cc',
          ],
        },
        {
          'target_name': 'RTPchange',
          'type': 'executable',
          'dependencies': [
            'NetEqTestTools',
            '<(DEPTH)/testing/gtest.gyp:gtest',
          ],
          'sources': [
           'test/RTPchange.cc',
          ],
        },
        {
          'target_name': 'RTPtimeshift',
          'type': 'executable',
          'dependencies': [
           'NetEqTestTools',
            '<(DEPTH)/testing/gtest.gyp:gtest',
          ],
          'sources': [
            'test/RTPtimeshift.cc',
          ],
        },
        {
          'target_name': 'RTPcat',
          'type': 'executable',
          'dependencies': [
            'NetEqTestTools',
            '<(DEPTH)/testing/gtest.gyp:gtest',
          ],
          'sources': [
            'test/RTPcat.cc',
          ],
        },
        {
          'target_name': 'rtp_to_text',
          'type': 'executable',
          'dependencies': [
            'NetEqTestTools',
            '<(webrtc_root)/system_wrappers/source/system_wrappers.gyp:system_wrappers',
          ],
          'sources': [
            'test/rtp_to_text.cc',
          ],
        },
        {
         'target_name': 'NetEqTestTools',
          # Collection of useful functions used in other tests
          'type': '<(library)',
          'variables': {
            # Expects RTP packets without payloads when enabled.
            'neteq_dummy_rtp%': 0,
          },
          'dependencies': [
            'G711',
            'G722',
            'PCM16B',
            'iLBC',
            'iSAC',
            'CNG',
            '<(DEPTH)/testing/gtest.gyp:gtest',
          ],
          'direct_dependent_settings': {
            'include_dirs': [
              'interface',
              'test',
            ],
          },
          'defines': [
            # TODO: Make codec selection conditional on definitions in target NetEq
            'CODEC_ILBC',
            'CODEC_PCM16B',
            'CODEC_G711',
            'CODEC_G722',
            'CODEC_ISAC',
            'CODEC_PCM16B_WB',
            'CODEC_ISAC_SWB',
            'CODEC_ISAC_FB',
            'CODEC_PCM16B_32KHZ',
            'CODEC_CNGCODEC8',
            'CODEC_CNGCODEC16',
            'CODEC_CNGCODEC32',
            'CODEC_ATEVENT_DECODE',
            'CODEC_RED',
          ],
          'include_dirs': [
            'interface',
            'test',
          ],
          'sources': [
            'test/NETEQTEST_CodecClass.cc',
            'test/NETEQTEST_CodecClass.h',
            'test/NETEQTEST_DummyRTPpacket.cc',
            'test/NETEQTEST_DummyRTPpacket.h',
            'test/NETEQTEST_NetEQClass.cc',
            'test/NETEQTEST_NetEQClass.h',
            'test/NETEQTEST_RTPpacket.cc',
            'test/NETEQTEST_RTPpacket.h',
          ],
        },
      ], # targets
    }], # include_tests
  ], # conditions
}
