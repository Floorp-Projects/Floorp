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
      'type': 'static_library',
      'dependencies': [
        'webrtc_i420',
        '<(webrtc_root)/common_video/common_video.gyp:common_video',
        '<(webrtc_root)/modules/video_coding/utility/video_coding_utility.gyp:video_coding_utility',
        '<(webrtc_root)/system_wrappers/system_wrappers.gyp:system_wrappers',
        '<(webrtc_vp8_dir)/vp8.gyp:webrtc_vp8',
        '<(webrtc_vp9_dir)/vp9.gyp:webrtc_vp9',
      ],
      'sources': [
        # interfaces
        'main/interface/video_coding.h',
        'main/interface/video_coding_defines.h',

        # headers
        'main/source/codec_database.h',
        'main/source/codec_timer.h',
        'main/source/content_metrics_processing.h',
        'main/source/decoding_state.h',
        'main/source/encoded_frame.h',
        'main/source/er_tables_xor.h',
        'main/source/fec_tables_xor.h',
        'main/source/frame_buffer.h',
        'main/source/generic_decoder.h',
        'main/source/generic_encoder.h',
        'main/source/inter_frame_delay.h',
        'main/source/internal_defines.h',
        'main/source/jitter_buffer.h',
        'main/source/jitter_buffer_common.h',
        'main/source/jitter_estimator.h',
        'main/source/media_opt_util.h',
        'main/source/media_optimization.h',
        'main/source/nack_fec_tables.h',
        'main/source/packet.h',
        'main/source/qm_select_data.h',
        'main/source/qm_select.h',
        'main/source/receiver.h',
        'main/source/rtt_filter.h',
        'main/source/session_info.h',
        'main/source/timestamp_map.h',
        'main/source/timing.h',
        'main/source/video_coding_impl.h',

        # sources
        'main/source/codec_database.cc',
        'main/source/codec_timer.cc',
        'main/source/content_metrics_processing.cc',
        'main/source/decoding_state.cc',
        'main/source/encoded_frame.cc',
        'main/source/frame_buffer.cc',
        'main/source/generic_decoder.cc',
        'main/source/generic_encoder.cc',
        'main/source/inter_frame_delay.cc',
        'main/source/jitter_buffer.cc',
        'main/source/jitter_estimator.cc',
        'main/source/media_opt_util.cc',
        'main/source/media_optimization.cc',
        'main/source/packet.cc',
        'main/source/qm_select.cc',
        'main/source/receiver.cc',
        'main/source/rtt_filter.cc',
        'main/source/session_info.cc',
        'main/source/timestamp_map.cc',
        'main/source/timing.cc',
        'main/source/video_coding_impl.cc',
        'main/source/video_sender.cc',
        'main/source/video_receiver.cc',
      ], # source
      # TODO(jschuh): Bug 1348: fix size_t to int truncations.
      'msvs_disabled_warnings': [ 4267, ],
    },
  ],
}
