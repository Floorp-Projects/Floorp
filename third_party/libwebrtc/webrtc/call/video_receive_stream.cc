/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "call/video_receive_stream.h"

namespace webrtc {

VideoReceiveStream::Decoder::Decoder() = default;
VideoReceiveStream::Decoder::Decoder(const Decoder&) = default;
VideoReceiveStream::Decoder::~Decoder() = default;

std::string VideoReceiveStream::Decoder::ToString() const {
  std::stringstream ss;
  ss << "{decoder: " << (decoder ? "(VideoDecoder)" : "nullptr");
  ss << ", payload_type: " << payload_type;
  ss << ", payload_name: " << payload_name;
  ss << ", codec_params: {";
  for (const auto& it : codec_params)
    ss << it.first << ": " << it.second;
  ss << '}';
  ss << '}';

  return ss.str();
}

VideoReceiveStream::Stats::Stats() = default;
VideoReceiveStream::Stats::~Stats() = default;

std::string VideoReceiveStream::Stats::ToString(int64_t time_ms) const {
  std::stringstream ss;
  ss << "VideoReceiveStream stats: " << time_ms << ", {ssrc: " << ssrc << ", ";
  ss << "total_bps: " << total_bitrate_bps << ", ";
  ss << "width: " << width << ", ";
  ss << "height: " << height << ", ";
  ss << "key: " << frame_counts.key_frames << ", ";
  ss << "delta: " << frame_counts.delta_frames << ", ";
  ss << "network_fps: " << network_frame_rate << ", ";
  ss << "decode_fps: " << decode_frame_rate << ", ";
  ss << "render_fps: " << render_frame_rate << ", ";
  ss << "decode_ms: " << decode_ms << ", ";
  ss << "max_decode_ms: " << max_decode_ms << ", ";
  ss << "cur_delay_ms: " << current_delay_ms << ", ";
  ss << "targ_delay_ms: " << target_delay_ms << ", ";
  ss << "jb_delay_ms: " << jitter_buffer_ms << ", ";
  ss << "min_playout_delay_ms: " << min_playout_delay_ms << ", ";
  ss << "discarded: " << discarded_packets << ", ";
  ss << "sync_offset_ms: " << sync_offset_ms << ", ";
  ss << "cum_loss: " << rtcp_stats.packets_lost << ", ";
  ss << "max_ext_seq: " << rtcp_stats.extended_highest_sequence_number << ", ";
  ss << "nack: " << rtcp_packet_type_counts.nack_packets << ", ";
  ss << "fir: " << rtcp_packet_type_counts.fir_packets << ", ";
  ss << "pli: " << rtcp_packet_type_counts.pli_packets;
  ss << '}';
  return ss.str();
}

VideoReceiveStream::Config::Config(const Config&) = default;
VideoReceiveStream::Config::Config(Config&&) = default;
VideoReceiveStream::Config::Config(Transport* rtcp_send_transport)
    : rtcp_send_transport(rtcp_send_transport) {}

VideoReceiveStream::Config& VideoReceiveStream::Config::operator=(Config&&) =
    default;
VideoReceiveStream::Config::Config::~Config() = default;

std::string VideoReceiveStream::Config::ToString() const {
  std::stringstream ss;
  ss << "{decoders: [";
  for (size_t i = 0; i < decoders.size(); ++i) {
    ss << decoders[i].ToString();
    if (i != decoders.size() - 1)
      ss << ", ";
  }
  ss << ']';
  ss << ", rtp: " << rtp.ToString();
  ss << ", renderer: " << (renderer ? "(renderer)" : "nullptr");
  ss << ", render_delay_ms: " << render_delay_ms;
  if (!sync_group.empty())
    ss << ", sync_group: " << sync_group;
  ss << ", pre_decode_callback: "
     << (pre_decode_callback ? "(EncodedFrameObserver)" : "nullptr");
  ss << ", target_delay_ms: " << target_delay_ms;
  ss << '}';

  return ss.str();
}

VideoReceiveStream::Config::Rtp::Rtp() = default;
VideoReceiveStream::Config::Rtp::Rtp(const Rtp&) = default;
VideoReceiveStream::Config::Rtp::~Rtp() = default;

std::string VideoReceiveStream::Config::Rtp::ToString() const {
  std::stringstream ss;
  ss << "{remote_ssrc: " << remote_ssrc;
  ss << ", local_ssrc: " << local_ssrc;
  ss << ", rtcp_mode: "
     << (rtcp_mode == RtcpMode::kCompound ? "RtcpMode::kCompound"
                                          : "RtcpMode::kReducedSize");
  ss << ", rtcp_xr: ";
  ss << "{receiver_reference_time_report: "
     << (rtcp_xr.receiver_reference_time_report ? "on" : "off");
  ss << '}';
  ss << ", remb: " << (remb ? "on" : "off");
  ss << ", transport_cc: " << (transport_cc ? "on" : "off");
  ss << ", nack: {rtp_history_ms: " << nack.rtp_history_ms << '}';
  ss << ", ulpfec_payload_type: " << ulpfec_payload_type;
  ss << ", red_type: " << red_payload_type;
  ss << ", rtx_ssrc: " << rtx_ssrc;
  ss << ", rtx_payload_types: {";
  for (auto& kv : rtx_associated_payload_types) {
    ss << kv.first << " (pt) -> " << kv.second << " (apt), ";
  }
  ss << '}';
  ss << ", extensions: [";
  for (size_t i = 0; i < extensions.size(); ++i) {
    ss << extensions[i].ToString();
    if (i != extensions.size() - 1)
      ss << ", ";
  }
  ss << ']';
  ss << '}';
  return ss.str();
}

}  // namespace webrtc
