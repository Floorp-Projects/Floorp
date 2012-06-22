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
      'target_name': 'audio_coding_module',
      'type': '<(library)',
      'dependencies': [
        'CNG',
        'G711',
        'G722',
        'iLBC',
        'iSAC',
        'iSACFix',
        'PCM16B',
        'NetEq',
        '<(webrtc_root)/common_audio/common_audio.gyp:resampler',
        '<(webrtc_root)/common_audio/common_audio.gyp:signal_processing',
        '<(webrtc_root)/common_audio/common_audio.gyp:vad',
        '<(webrtc_root)/system_wrappers/source/system_wrappers.gyp:system_wrappers',
      ],
      'include_dirs': [
        '../interface',
        '../../../interface',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
        '../interface',
        '../../../interface',
        ],
      },
      'sources': [
        '../interface/audio_coding_module.h',
        '../interface/audio_coding_module_typedefs.h',
        'acm_amr.cc',
        'acm_amr.h',
        'acm_amrwb.cc',
        'acm_amrwb.h',
        'acm_celt.cc',
        'acm_celt.h',
        'acm_cng.cc',
        'acm_cng.h',
        'acm_codec_database.cc',
        'acm_codec_database.h',
        'acm_dtmf_detection.cc',
        'acm_dtmf_detection.h',
        'acm_dtmf_playout.cc',
        'acm_dtmf_playout.h',
        'acm_g722.cc',
        'acm_g722.h',
        'acm_g7221.cc',
        'acm_g7221.h',
        'acm_g7221c.cc',
        'acm_g7221c.h',
        'acm_g729.cc',
        'acm_g729.h',
        'acm_g7291.cc',
        'acm_g7291.h',
        'acm_generic_codec.cc',
        'acm_generic_codec.h',
        'acm_gsmfr.cc',
        'acm_gsmfr.h',
        'acm_ilbc.cc',
        'acm_ilbc.h',
        'acm_isac.cc',
        'acm_isac.h',
        'acm_isac_macros.h',
        'acm_neteq.cc',
        'acm_neteq.h',
        'acm_opus.cc',
        'acm_opus.h',
        'acm_speex.cc',
        'acm_speex.h',
        'acm_pcm16b.cc',
        'acm_pcm16b.h',
        'acm_pcma.cc',
        'acm_pcma.h',
        'acm_pcmu.cc',
        'acm_pcmu.h',
        'acm_red.cc',
        'acm_red.h',
        'acm_resampler.cc',
        'acm_resampler.h',
        'audio_coding_module.cc',
        'audio_coding_module_impl.cc',
        'audio_coding_module_impl.h',
      ],
    },
  ],
  # Exclude the test targets when building with chromium.
  'conditions': [
    ['build_with_chromium==0', {
      'targets': [
        {
          'target_name': 'audio_coding_module_test',
          'type': 'executable',
          'dependencies': [
            'audio_coding_module',
            '<(webrtc_root)/../test/test.gyp:test_support_main',
            '<(webrtc_root)/../testing/gtest.gyp:gtest',
            '<(webrtc_root)/system_wrappers/source/system_wrappers.gyp:system_wrappers',
          ],
          'sources': [
             '../test/ACMTest.cc',
             '../test/APITest.cc',
             '../test/Channel.cc',
             '../test/EncodeDecodeTest.cc',
             '../test/iSACTest.cc',
             '../test/PCMFile.cc',
             '../test/RTPFile.cc',
             '../test/SpatialAudio.cc',
             '../test/TestAllCodecs.cc',
             '../test/Tester.cc',
             '../test/TestFEC.cc',
             '../test/TestStereo.cc',
             '../test/TestVADDTX.cc',
             '../test/TimedTrace.cc',
             '../test/TwoWayCommunication.cc',
             '../test/utility.cc',
          ],
        },
        {
          'target_name': 'audio_coding_unittests',
          'type': 'executable',
          'dependencies': [
            'audio_coding_module',
            'NetEq',
            '<(webrtc_root)/common_audio/common_audio.gyp:vad',
            '<(webrtc_root)/../testing/gtest.gyp:gtest',
            '<(webrtc_root)/../test/test.gyp:test_support_main',
            '<(webrtc_root)/system_wrappers/source/system_wrappers.gyp:system_wrappers',
          ],
          'sources': [
             'acm_neteq_unittest.cc',
          ],
        }, # audio_coding_unittests
      ],
    }],
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
