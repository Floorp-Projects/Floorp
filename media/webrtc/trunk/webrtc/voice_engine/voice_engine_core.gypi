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
      'target_name': 'voice_engine_core',
      'type': '<(library)',
      'dependencies': [
        '<(webrtc_root)/common_audio/common_audio.gyp:resampler',
        '<(webrtc_root)/common_audio/common_audio.gyp:signal_processing',
        '<(webrtc_root)/modules/modules.gyp:audio_coding_module',
        '<(webrtc_root)/modules/modules.gyp:audio_conference_mixer',
        '<(webrtc_root)/modules/modules.gyp:audio_device',
        '<(webrtc_root)/modules/modules.gyp:audio_processing',
        '<(webrtc_root)/modules/modules.gyp:media_file',
        '<(webrtc_root)/modules/modules.gyp:rtp_rtcp',
        '<(webrtc_root)/modules/modules.gyp:udp_transport',
        '<(webrtc_root)/modules/modules.gyp:webrtc_utility',
        '<(webrtc_root)/system_wrappers/source/system_wrappers.gyp:system_wrappers',
      ],
      'include_dirs': [
        'include',
        '<(webrtc_root)/modules/audio_device',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          'include',
        ],
      },
      'defines': [
        'WEBRTC_EXTERNAL_TRANSPORT',
      ],
      'sources': [
        '../common_types.h',
        '../engine_configurations.h',
        '../typedefs.h',
        'include/voe_audio_processing.h',
        'include/voe_base.h',
        'include/voe_call_report.h',
        'include/voe_codec.h',
        'include/voe_dtmf.h',
        'include/voe_encryption.h',
        'include/voe_errors.h',
        'include/voe_external_media.h',
        'include/voe_file.h',
        'include/voe_hardware.h',
        'include/voe_neteq_stats.h',
        'include/voe_network.h',
        'include/voe_rtp_rtcp.h',
        'include/voe_video_sync.h',
        'include/voe_volume_control.h',
        'channel.cc',
        'channel.h',
        'channel_manager.cc',
        'channel_manager.h',
        'channel_manager_base.cc',
        'channel_manager_base.h',
        'dtmf_inband.cc',
        'dtmf_inband.h',
        'dtmf_inband_queue.cc',
        'dtmf_inband_queue.h',
        'level_indicator.cc',
        'level_indicator.h',
        'monitor_module.cc',
        'monitor_module.h',
        'output_mixer.cc',
        'output_mixer.h',
        'output_mixer_internal.cc',
        'output_mixer_internal.h',
        'shared_data.cc',
        'shared_data.h',
        'statistics.cc',
        'statistics.h',
        'transmit_mixer.cc',
        'transmit_mixer.h',
        'utility.cc',
        'utility.h',
        'voe_audio_processing_impl.cc',
        'voe_audio_processing_impl.h',
        'voe_base_impl.cc',
        'voe_base_impl.h',
        'voe_call_report_impl.cc',
        'voe_call_report_impl.h',
        'voe_codec_impl.cc',
        'voe_codec_impl.h',
        'voe_dtmf_impl.cc',
        'voe_dtmf_impl.h',
        'voe_encryption_impl.cc',
        'voe_encryption_impl.h',
        'voe_external_media_impl.cc',
        'voe_external_media_impl.h',
        'voe_file_impl.cc',
        'voe_file_impl.h',
        'voe_hardware_impl.cc',
        'voe_hardware_impl.h',
        'voe_neteq_stats_impl.cc',
        'voe_neteq_stats_impl.h',
        'voe_network_impl.cc',
        'voe_network_impl.h',
        'voe_rtp_rtcp_impl.cc',
        'voe_rtp_rtcp_impl.h',
        'voe_video_sync_impl.cc',
        'voe_video_sync_impl.h',
        'voe_volume_control_impl.cc',
        'voe_volume_control_impl.h',
        'voice_engine_defines.h',
        'voice_engine_impl.cc',
        'voice_engine_impl.h',
      ],
    },
  ],
  'conditions': [
    ['OS=="win"', {
      'defines': ['WEBRTC_DRIFT_COMPENSATION_SUPPORTED',],
    }],
    ['include_tests==1', {
      'targets': [
        {
          'target_name': 'voice_engine_unittests',
          'type': 'executable',
          'dependencies': [
            'voice_engine_core',
            '<(DEPTH)/testing/gtest.gyp:gtest',
            '<(webrtc_root)/test/test.gyp:test_support_main',
            # The rest are to satisfy the unittests' include chain.
            # This would be unnecessary if we used qualified includes.
            '<(webrtc_root)/common_audio/common_audio.gyp:resampler',
            '<(webrtc_root)/modules/modules.gyp:audio_device',
            '<(webrtc_root)/modules/modules.gyp:audio_processing',
            '<(webrtc_root)/modules/modules.gyp:audio_coding_module',
            '<(webrtc_root)/modules/modules.gyp:audio_conference_mixer',
            '<(webrtc_root)/modules/modules.gyp:media_file',
            '<(webrtc_root)/modules/modules.gyp:rtp_rtcp',
            '<(webrtc_root)/modules/modules.gyp:udp_transport',
            '<(webrtc_root)/modules/modules.gyp:webrtc_utility',
            '<(webrtc_root)/system_wrappers/source/system_wrappers.gyp:system_wrappers',
          ],
          'include_dirs': [
            'include',
          ],
          'sources': [
            'channel_unittest.cc',
            'output_mixer_unittest.cc',
            'transmit_mixer_unittest.cc',
            'voe_audio_processing_unittest.cc',
            'voe_codec_unittest.cc',
          ],
        },
      ], # targets
    }], # include_tests
  ], # conditions
}
