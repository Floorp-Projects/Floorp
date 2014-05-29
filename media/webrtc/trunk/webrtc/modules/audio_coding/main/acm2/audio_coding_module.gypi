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
      'target_name': 'acm2',
      'type': 'static_library',
      'defines': [
        '<@(audio_coding_defines)',
      ],
      'dependencies': [
        '<@(audio_coding_dependencies)',
        'NetEq4',
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
        'acm_common_defs.h',
        'acm_dtmf_playout.cc',
        'acm_dtmf_playout.h',
        'acm_g729.cc',
        'acm_g729.h',
        'acm_g7291.cc',
        'acm_g7291.h',
        'acm_generic_codec.cc',
        'acm_generic_codec.h',
        'acm_gsmfr.cc',
        'acm_gsmfr.h',
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
    },
  ],
}
