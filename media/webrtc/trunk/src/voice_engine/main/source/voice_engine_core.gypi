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
        '../../..',
        '../interface',
        '<(webrtc_root)/modules/audio_device/main/source',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '../../..',
          '../interface',
        ],
      },
      'sources': [
        '../../../common_types.h',
        '../../../engine_configurations.h',
        '../../../typedefs.h',
        '../interface/voe_audio_processing.h',
        '../interface/voe_base.h',
        '../interface/voe_call_report.h',
        '../interface/voe_codec.h',
        '../interface/voe_dtmf.h',
        '../interface/voe_encryption.h',
        '../interface/voe_errors.h',
        '../interface/voe_external_media.h',
        '../interface/voe_file.h',
        '../interface/voe_hardware.h',
        '../interface/voe_neteq_stats.h',
        '../interface/voe_network.h',
        '../interface/voe_rtp_rtcp.h',
        '../interface/voe_video_sync.h',
        '../interface/voe_volume_control.h',
        'audio_frame_operations.cc',
        'audio_frame_operations.h',
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
        'ref_count.cc',
        'ref_count.h',
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
    ['build_with_chromium==0', {
      'targets': [
        {
          'target_name': 'voice_engine_unittests',
          'type': 'executable',
          'dependencies': [
            'voice_engine_core',
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
            '<(webrtc_root)/../test/test.gyp:test_support_main',
            '<(webrtc_root)/../testing/gtest.gyp:gtest',
          ],
          'include_dirs': [            
            '../../..',
            '../interface',
          ],
          'sources': [
            'channel_unittest.cc',
          ],
        },
      ], # targets
    }], # build_with_chromium
  ], # conditions
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
