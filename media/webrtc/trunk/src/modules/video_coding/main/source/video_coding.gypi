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
      'target_name': 'webrtc_video_coding',
      'type': '<(library)',
      'dependencies': [
        'webrtc_i420',
        '<(webrtc_root)/common_video/common_video.gyp:webrtc_libyuv',
        '<(webrtc_root)/system_wrappers/source/system_wrappers.gyp:system_wrappers',
        '<(webrtc_vp8_dir)/vp8.gyp:webrtc_vp8',
      ],
      'include_dirs': [
        '../interface',
        '../../../interface',
        '../../codecs/interface',
        '../../../../common_video/interface',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '../interface',
          '../../codecs/interface',
        ],
      },
      'sources': [
        # interfaces
        '../interface/video_coding.h',
        '../interface/video_coding_defines.h',

        # headers
        'codec_database.h',
        'codec_timer.h',
        'content_metrics_processing.h',
        'decoding_state.h',
        'encoded_frame.h',
        'er_tables_xor.h',
        'event.h',
        'exp_filter.h',
        'fec_tables_xor.h',
        'frame_buffer.h',
        'frame_dropper.h',
        'generic_decoder.h',
        'generic_encoder.h',
        'inter_frame_delay.h',
        'internal_defines.h',
        'jitter_buffer.h',
        'jitter_buffer_common.h',
        'jitter_estimator.h',
        'media_opt_util.h',
        'media_optimization.h',
        'nack_fec_tables.h',
        'packet.h',
        'qm_select_data.h',
        'qm_select.h',
        'receiver.h',
        'rtt_filter.h',
        'session_info.h',
        'tick_time_base.h',
        'timestamp_extrapolator.h',
        'timestamp_map.h',
        'timing.h',
        'video_coding_impl.h',

        # sources
        'codec_database.cc',
        'codec_timer.cc',
        'content_metrics_processing.cc',
        'decoding_state.cc',
        'encoded_frame.cc',
        'exp_filter.cc',
        'frame_buffer.cc',
        'frame_dropper.cc',
        'generic_decoder.cc',
        'generic_encoder.cc',
        'inter_frame_delay.cc',
        'jitter_buffer.cc',
        'jitter_buffer_common.cc',
        'jitter_estimator.cc',
        'media_opt_util.cc',
        'media_optimization.cc',
        'packet.cc',
        'qm_select.cc',
        'receiver.cc',
        'rtt_filter.cc',
        'session_info.cc',
        'timestamp_extrapolator.cc',
        'timestamp_map.cc',
        'timing.cc',
        'video_coding_impl.cc',
      ], # source
    },
  ],
}
