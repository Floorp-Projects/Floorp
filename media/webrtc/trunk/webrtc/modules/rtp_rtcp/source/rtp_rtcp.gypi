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
      'target_name': 'rtp_rtcp',
      'type': '<(library)',
      'dependencies': [
        '<(webrtc_root)/system_wrappers/source/system_wrappers.gyp:system_wrappers',
        '<(webrtc_root)/modules/modules.gyp:remote_bitrate_estimator',
        '<(webrtc_root)/modules/modules.gyp:paced_sender',
      ],
      'include_dirs': [
        '../interface',
        '../../interface',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '../interface',
          '../../interface',
        ],
      },
      'sources': [
        # Common
        '../interface/rtp_rtcp.h',
        '../interface/rtp_rtcp_defines.h',
        'bitrate.cc',
        'bitrate.h',
        'rtp_rtcp_config.h',
        'rtp_rtcp_impl.cc',
        'rtp_rtcp_impl.h',
        'rtcp_receiver.cc',
        'rtcp_receiver.h',
        'rtcp_receiver_help.cc',
        'rtcp_receiver_help.h',
        'rtcp_sender.cc',
        'rtcp_sender.h',
        'rtcp_utility.cc',
        'rtcp_utility.h',
        'rtp_header_extension.cc',
        'rtp_header_extension.h',
        'rtp_receiver.cc',
        'rtp_receiver.h',
        'rtp_sender.cc',
        'rtp_sender.h',
        'rtp_utility.cc',
        'rtp_utility.h',
        'ssrc_database.cc',
        'ssrc_database.h',
        'tmmbr_help.cc',
        'tmmbr_help.h',
        # Audio Files
        'dtmf_queue.cc',
        'dtmf_queue.h',
        'rtp_receiver_audio.cc',
        'rtp_receiver_audio.h',
        'rtp_sender_audio.cc',
        'rtp_sender_audio.h',
        # Video Files
        'fec_private_tables_random.h',
        'fec_private_tables_bursty.h',
        'forward_error_correction.cc',
        'forward_error_correction.h',
        'forward_error_correction_internal.cc',
        'forward_error_correction_internal.h',
        'producer_fec.cc',
        'producer_fec.h',
        'rtp_packet_history.cc',
        'rtp_packet_history.h',
        'rtp_receiver_strategy.cc',
        'rtp_receiver_strategy.h',
        'rtp_receiver_video.cc',
        'rtp_receiver_video.h',
        'rtp_sender_video.cc',
        'rtp_sender_video.h',
        'receiver_fec.cc',
        'receiver_fec.h',
        'video_codec_information.h',
        'rtp_format_vp8.cc',
        'rtp_format_vp8.h',
        'vp8_partition_aggregator.cc',
        'vp8_partition_aggregator.h',
        # Mocks
        '../mocks/mock_rtp_rtcp.h',
      ], # source
    },
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
