/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/video/video_send_stream.h"

#include <algorithm>
#include <sstream>
#include <string>
#include <vector>

#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/trace_event.h"
#include "webrtc/call/congestion_controller.h"
#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"
#include "webrtc/modules/bitrate_controller/include/bitrate_controller.h"
#include "webrtc/modules/pacing/packet_router.h"
#include "webrtc/video/call_stats.h"
#include "webrtc/video/encoder_state_feedback.h"
#include "webrtc/video/payload_router.h"
#include "webrtc/video/video_capture_input.h"
#include "webrtc/video/vie_channel.h"
#include "webrtc/video/vie_encoder.h"
#include "webrtc/video_send_stream.h"

namespace webrtc {

class PacedSender;
class RtcpIntraFrameObserver;
class TransportFeedbackObserver;

std::string
VideoSendStream::Config::EncoderSettings::ToString() const {
  std::stringstream ss;
  ss << "{payload_name: " << payload_name;
  ss << ", payload_type: " << payload_type;
  ss << ", encoder: " << (encoder != nullptr ? "(VideoEncoder)" : "nullptr");
  ss << '}';
  return ss.str();
}

std::string VideoSendStream::Config::Rtp::Rtx::ToString()
    const {
  std::stringstream ss;
  ss << "{ssrcs: [";
  for (size_t i = 0; i < ssrcs.size(); ++i) {
    ss << ssrcs[i];
    if (i != ssrcs.size() - 1)
      ss << ", ";
  }
  ss << ']';

  ss << ", payload_type: " << payload_type;
  ss << '}';
  return ss.str();
}

std::string VideoSendStream::Config::Rtp::ToString() const {
  std::stringstream ss;
  ss << "{ssrcs: [";
  for (size_t i = 0; i < ssrcs.size(); ++i) {
    ss << ssrcs[i];
    if (i != ssrcs.size() - 1)
      ss << ", ";
  }
  ss << ']';
  ss << ", max_packet_size: " << max_packet_size;
  ss << ", extensions: [";
  for (size_t i = 0; i < extensions.size(); ++i) {
    ss << extensions[i].ToString();
    if (i != extensions.size() - 1)
      ss << ", ";
  }
  ss << ']';

  ss << ", nack: {rtp_history_ms: " << nack.rtp_history_ms << '}';
  ss << ", fec: " << fec.ToString();
  ss << ", rtx: " << rtx.ToString();
  ss << ", c_name: " << c_name;
  ss << '}';
  return ss.str();
}

std::string VideoSendStream::Config::ToString() const {
  std::stringstream ss;
  ss << "{encoder_settings: " << encoder_settings.ToString();
  ss << ", rtp: " << rtp.ToString();
  ss << ", pre_encode_callback: "
     << (pre_encode_callback != nullptr ? "(I420FrameCallback)" : "nullptr");
  ss << ", post_encode_callback: " << (post_encode_callback != nullptr
                                           ? "(EncodedFrameObserver)"
                                           : "nullptr");
  ss << ", local_renderer: " << (local_renderer != nullptr ? "(VideoRenderer)"
                                                         : "nullptr");
  ss << ", render_delay_ms: " << render_delay_ms;
  ss << ", target_delay_ms: " << target_delay_ms;
  ss << ", suspend_below_min_bitrate: " << (suspend_below_min_bitrate ? "on"
                                                                      : "off");
  ss << '}';
  return ss.str();
}

namespace internal {
VideoSendStream::VideoSendStream(
    int num_cpu_cores,
    ProcessThread* module_process_thread,
    CallStats* call_stats,
    CongestionController* congestion_controller,
    BitrateAllocator* bitrate_allocator,
    const VideoSendStream::Config& config,
    const VideoEncoderConfig& encoder_config,
    const std::map<uint32_t, RtpState>& suspended_ssrcs)
    : stats_proxy_(Clock::GetRealTimeClock(),
                   config,
                   encoder_config.content_type),
      transport_adapter_(config.send_transport),
      encoded_frame_proxy_(config.post_encode_callback),
      config_(config),
      suspended_ssrcs_(suspended_ssrcs),
      module_process_thread_(module_process_thread),
      call_stats_(call_stats),
      congestion_controller_(congestion_controller),
      encoder_feedback_(new EncoderStateFeedback()),
      use_config_bitrate_(true) {
  LOG(LS_INFO) << "VideoSendStream: " << config_.ToString();
  RTC_DCHECK(!config_.rtp.ssrcs.empty());

  // Set up Call-wide sequence numbers, if configured for this send stream.
  TransportFeedbackObserver* transport_feedback_observer = nullptr;
  for (const RtpExtension& extension : config.rtp.extensions) {
    if (extension.name == RtpExtension::kTransportSequenceNumber) {
      transport_feedback_observer =
          congestion_controller_->GetTransportFeedbackObserver();
      break;
    }
  }

  const std::vector<uint32_t>& ssrcs = config.rtp.ssrcs;

  vie_encoder_.reset(new ViEEncoder(
      num_cpu_cores, module_process_thread_, &stats_proxy_,
      config.pre_encode_callback, congestion_controller_->pacer(),
      bitrate_allocator));
  RTC_CHECK(vie_encoder_->Init());

  vie_channel_.reset(new ViEChannel(
      num_cpu_cores, config.send_transport, module_process_thread_,
      encoder_feedback_->GetRtcpIntraFrameObserver(),
      congestion_controller_->GetBitrateController()->
          CreateRtcpBandwidthObserver(),
      transport_feedback_observer,
      congestion_controller_->GetRemoteBitrateEstimator(false),
      call_stats_->rtcp_rtt_stats(), congestion_controller_->pacer(),
      congestion_controller_->packet_router(), ssrcs.size(), true));
  RTC_CHECK(vie_channel_->Init() == 0);

  call_stats_->RegisterStatsObserver(vie_channel_->GetStatsObserver());

  vie_encoder_->StartThreadsAndSetSharedMembers(
      vie_channel_->send_payload_router(),
      vie_channel_->vcm_protection_callback());

  std::vector<uint32_t> first_ssrc(1, ssrcs[0]);
  vie_encoder_->SetSsrcs(first_ssrc);

  for (size_t i = 0; i < config_.rtp.extensions.size(); ++i) {
    const std::string& extension = config_.rtp.extensions[i].name;
    int id = config_.rtp.extensions[i].id;
    // One-byte-extension local identifiers are in the range 1-14 inclusive.
    RTC_DCHECK_GE(id, 1);
    RTC_DCHECK_LE(id, 14);
    if (extension == RtpExtension::kTOffset) {
      RTC_CHECK_EQ(0, vie_channel_->SetSendTimestampOffsetStatus(true, id));
    } else if (extension == RtpExtension::kAbsSendTime) {
      RTC_CHECK_EQ(0, vie_channel_->SetSendAbsoluteSendTimeStatus(true, id));
    } else if (extension == RtpExtension::kVideoRotation) {
      RTC_CHECK_EQ(0, vie_channel_->SetSendVideoRotationStatus(true, id));
    } else if (extension == RtpExtension::kTransportSequenceNumber) {
      RTC_CHECK_EQ(0, vie_channel_->SetSendTransportSequenceNumber(true, id));
    } else {
      RTC_NOTREACHED() << "Registering unsupported RTP extension.";
    }
  }

  congestion_controller_->SetChannelRembStatus(true, false,
                                               vie_channel_->rtp_rtcp());

  // Enable NACK, FEC or both.
  const bool enable_protection_nack = config_.rtp.nack.rtp_history_ms > 0;
  const bool enable_protection_fec = config_.rtp.fec.red_payload_type != -1;
  // TODO(changbin): Should set RTX for RED mapping in RTP sender in future.
  vie_channel_->SetProtectionMode(enable_protection_nack, enable_protection_fec,
                                  config_.rtp.fec.red_payload_type,
                                  config_.rtp.fec.ulpfec_payload_type);
  vie_encoder_->SetProtectionMethod(enable_protection_nack,
                                    enable_protection_fec);

  ConfigureSsrcs();

  vie_channel_->SetRTCPCName(config_.rtp.c_name.c_str());

  input_.reset(new internal::VideoCaptureInput(
      module_process_thread_, vie_encoder_.get(), config_.local_renderer,
      &stats_proxy_, this, config_.encoding_time_observer));

  // 28 to match packet overhead in ModuleRtpRtcpImpl.
  RTC_DCHECK_LE(config_.rtp.max_packet_size, static_cast<size_t>(0xFFFF - 28));
  vie_channel_->SetMTU(static_cast<uint16_t>(config_.rtp.max_packet_size + 28));

  RTC_DCHECK(config.encoder_settings.encoder != nullptr);
  RTC_DCHECK_GE(config.encoder_settings.payload_type, 0);
  RTC_DCHECK_LE(config.encoder_settings.payload_type, 127);
  RTC_CHECK_EQ(0, vie_encoder_->RegisterExternalEncoder(
                      config.encoder_settings.encoder,
                      config.encoder_settings.payload_type,
                      config.encoder_settings.internal_source));

  RTC_CHECK(ReconfigureVideoEncoder(encoder_config));

  vie_channel_->RegisterSendSideDelayObserver(&stats_proxy_);

  if (config_.post_encode_callback)
    vie_encoder_->RegisterPostEncodeImageCallback(&encoded_frame_proxy_);

  if (config_.suspend_below_min_bitrate)
    vie_encoder_->SuspendBelowMinBitrate();

  congestion_controller_->AddEncoder(vie_encoder_.get());
  encoder_feedback_->AddEncoder(ssrcs, vie_encoder_.get());

  vie_channel_->RegisterSendChannelRtcpStatisticsCallback(&stats_proxy_);
  vie_channel_->RegisterSendChannelRtpStatisticsCallback(&stats_proxy_);
  vie_channel_->RegisterRtcpPacketTypeCounterObserver(&stats_proxy_);
  vie_channel_->RegisterSendBitrateObserver(&stats_proxy_);
  vie_channel_->RegisterSendFrameCountObserver(&stats_proxy_);
}

VideoSendStream::~VideoSendStream() {
  LOG(LS_INFO) << "~VideoSendStream: " << config_.ToString();
  vie_channel_->RegisterSendFrameCountObserver(nullptr);
  vie_channel_->RegisterSendBitrateObserver(nullptr);
  vie_channel_->RegisterRtcpPacketTypeCounterObserver(nullptr);
  vie_channel_->RegisterSendChannelRtpStatisticsCallback(nullptr);
  vie_channel_->RegisterSendChannelRtcpStatisticsCallback(nullptr);

  // Remove capture input (thread) so that it's not running after the current
  // channel is deleted.
  input_.reset();

  vie_encoder_->DeRegisterExternalEncoder(
      config_.encoder_settings.payload_type);

  call_stats_->DeregisterStatsObserver(vie_channel_->GetStatsObserver());
  congestion_controller_->SetChannelRembStatus(false, false,
                                               vie_channel_->rtp_rtcp());

  // Remove the feedback, stop all encoding threads and processing. This must be
  // done before deleting the channel.
  congestion_controller_->RemoveEncoder(vie_encoder_.get());
  encoder_feedback_->RemoveEncoder(vie_encoder_.get());
  vie_encoder_->StopThreadsAndRemoveSharedMembers();

  uint32_t remote_ssrc = vie_channel_->GetRemoteSSRC();
  congestion_controller_->GetRemoteBitrateEstimator(false)->RemoveStream(
      remote_ssrc);
}

VideoCaptureInput* VideoSendStream::Input() {
  return input_.get();
}

void VideoSendStream::Start() {
  transport_adapter_.Enable();
  vie_encoder_->Pause();
  if (vie_channel_->StartSend() == 0) {
    // Was not already started, trigger a keyframe.
    vie_encoder_->SendKeyFrame();
  }
  vie_encoder_->Restart();
  vie_channel_->StartReceive();
}

void VideoSendStream::Stop() {
  // TODO(pbos): Make sure the encoder stops here.
  vie_channel_->StopSend();
  vie_channel_->StopReceive();
  transport_adapter_.Disable();
}

bool VideoSendStream::ReconfigureVideoEncoder(
    const VideoEncoderConfig& config) {
  TRACE_EVENT0("webrtc", "VideoSendStream::(Re)configureVideoEncoder");
  LOG(LS_INFO) << "(Re)configureVideoEncoder: " << config.ToString();
  const std::vector<VideoStream>& streams = config.streams;
  RTC_DCHECK(!streams.empty());
  RTC_DCHECK_GE(config_.rtp.ssrcs.size(), streams.size());

  VideoCodec video_codec;
  memset(&video_codec, 0, sizeof(video_codec));
  if (config_.encoder_settings.payload_name == "VP8") {
    video_codec.codecType = kVideoCodecVP8;
  } else if (config_.encoder_settings.payload_name == "VP9") {
    video_codec.codecType = kVideoCodecVP9;
  } else if (config_.encoder_settings.payload_name == "H264") {
    video_codec.codecType = kVideoCodecH264;
  } else {
    video_codec.codecType = kVideoCodecGeneric;
  }

  switch (config.content_type) {
    case VideoEncoderConfig::ContentType::kRealtimeVideo:
      video_codec.mode = kRealtimeVideo;
      break;
    case VideoEncoderConfig::ContentType::kScreen:
      video_codec.mode = kScreensharing;
      if (config.streams.size() == 1 &&
          config.streams[0].temporal_layer_thresholds_bps.size() == 1) {
        video_codec.targetBitrate =
            config.streams[0].temporal_layer_thresholds_bps[0] / 1000;
      }
      break;
  }

  if (video_codec.codecType == kVideoCodecVP8) {
    video_codec.codecSpecific.VP8 = VideoEncoder::GetDefaultVp8Settings();
  } else if (video_codec.codecType == kVideoCodecVP9) {
    video_codec.codecSpecific.VP9 = VideoEncoder::GetDefaultVp9Settings();
  } else if (video_codec.codecType == kVideoCodecH264) {
    video_codec.codecSpecific.H264 = VideoEncoder::GetDefaultH264Settings();
  }

  if (video_codec.codecType == kVideoCodecVP8) {
    if (config.encoder_specific_settings != nullptr) {
      video_codec.codecSpecific.VP8 = *reinterpret_cast<const VideoCodecVP8*>(
                                          config.encoder_specific_settings);
    }
    video_codec.codecSpecific.VP8.numberOfTemporalLayers =
        static_cast<unsigned char>(
            streams.back().temporal_layer_thresholds_bps.size() + 1);
  } else if (video_codec.codecType == kVideoCodecVP9) {
    if (config.encoder_specific_settings != nullptr) {
      video_codec.codecSpecific.VP9 = *reinterpret_cast<const VideoCodecVP9*>(
                                          config.encoder_specific_settings);
      if (video_codec.mode == kScreensharing) {
        video_codec.codecSpecific.VP9.flexibleMode = true;
        // For now VP9 screensharing use 1 temporal and 2 spatial layers.
        RTC_DCHECK_EQ(video_codec.codecSpecific.VP9.numberOfTemporalLayers, 1);
        RTC_DCHECK_EQ(video_codec.codecSpecific.VP9.numberOfSpatialLayers, 2);
      }
    }
    video_codec.codecSpecific.VP9.numberOfTemporalLayers =
        static_cast<unsigned char>(
            streams.back().temporal_layer_thresholds_bps.size() + 1);
  } else if (video_codec.codecType == kVideoCodecH264) {
    if (config.encoder_specific_settings != nullptr) {
      video_codec.codecSpecific.H264 = *reinterpret_cast<const VideoCodecH264*>(
                                           config.encoder_specific_settings);
    }
  } else {
    // TODO(pbos): Support encoder_settings codec-agnostically.
    RTC_DCHECK(config.encoder_specific_settings == nullptr)
        << "Encoder-specific settings for codec type not wired up.";
  }

  strncpy(video_codec.plName,
          config_.encoder_settings.payload_name.c_str(),
          kPayloadNameSize - 1);
  video_codec.plName[kPayloadNameSize - 1] = '\0';
  video_codec.plType = config_.encoder_settings.payload_type;
  video_codec.numberOfSimulcastStreams =
      static_cast<unsigned char>(streams.size());
  video_codec.minBitrate = streams[0].min_bitrate_bps / 1000;
  RTC_DCHECK_LE(streams.size(), static_cast<size_t>(kMaxSimulcastStreams));
  if (video_codec.codecType == kVideoCodecVP9) {
    // If the vector is empty, bitrates will be configured automatically.
    RTC_DCHECK(config.spatial_layers.empty() ||
               config.spatial_layers.size() ==
                   video_codec.codecSpecific.VP9.numberOfSpatialLayers);
    RTC_DCHECK_LE(video_codec.codecSpecific.VP9.numberOfSpatialLayers,
                  kMaxSimulcastStreams);
    for (size_t i = 0; i < config.spatial_layers.size(); ++i)
      video_codec.spatialLayers[i] = config.spatial_layers[i];
  }
  for (size_t i = 0; i < streams.size(); ++i) {
    SimulcastStream* sim_stream = &video_codec.simulcastStream[i];
    RTC_DCHECK_GT(streams[i].width, 0u);
    RTC_DCHECK_GT(streams[i].height, 0u);
    RTC_DCHECK_GT(streams[i].max_framerate, 0);
    // Different framerates not supported per stream at the moment.
    RTC_DCHECK_EQ(streams[i].max_framerate, streams[0].max_framerate);
    RTC_DCHECK_GE(streams[i].min_bitrate_bps, 0);
    RTC_DCHECK_GE(streams[i].target_bitrate_bps, streams[i].min_bitrate_bps);
    RTC_DCHECK_GE(streams[i].max_bitrate_bps, streams[i].target_bitrate_bps);
    RTC_DCHECK_GE(streams[i].max_qp, 0);

    sim_stream->width = static_cast<uint16_t>(streams[i].width);
    sim_stream->height = static_cast<uint16_t>(streams[i].height);
    sim_stream->minBitrate = streams[i].min_bitrate_bps / 1000;
    sim_stream->targetBitrate = streams[i].target_bitrate_bps / 1000;
    sim_stream->maxBitrate = streams[i].max_bitrate_bps / 1000;
    sim_stream->qpMax = streams[i].max_qp;
    sim_stream->numberOfTemporalLayers = static_cast<unsigned char>(
        streams[i].temporal_layer_thresholds_bps.size() + 1);

    video_codec.width = std::max(video_codec.width,
                                 static_cast<uint16_t>(streams[i].width));
    video_codec.height = std::max(
        video_codec.height, static_cast<uint16_t>(streams[i].height));
    video_codec.minBitrate =
        std::min(static_cast<uint16_t>(video_codec.minBitrate),
                 static_cast<uint16_t>(streams[i].min_bitrate_bps / 1000));
    video_codec.maxBitrate += streams[i].max_bitrate_bps / 1000;
    video_codec.qpMax = std::max(video_codec.qpMax,
                                 static_cast<unsigned int>(streams[i].max_qp));
  }

  // Set to zero to not update the bitrate controller from ViEEncoder, as
  // the bitrate controller is already set from Call.
  video_codec.startBitrate = 0;

  RTC_DCHECK_GT(streams[0].max_framerate, 0);
  video_codec.maxFramerate = streams[0].max_framerate;

  if (!SetSendCodec(video_codec))
    return false;

  // Clear stats for disabled layers.
  for (size_t i = video_codec.numberOfSimulcastStreams;
       i < config_.rtp.ssrcs.size(); ++i) {
    stats_proxy_.OnInactiveSsrc(config_.rtp.ssrcs[i]);
  }

  stats_proxy_.SetContentType(config.content_type);

  RTC_DCHECK_GE(config.min_transmit_bitrate_bps, 0);
  vie_encoder_->SetMinTransmitBitrate(config.min_transmit_bitrate_bps / 1000);

  encoder_config_ = config;
  use_config_bitrate_ = false;
  return true;
}

bool VideoSendStream::DeliverRtcp(const uint8_t* packet, size_t length) {
  return vie_channel_->ReceivedRTCPPacket(packet, length) == 0;
}

VideoSendStream::Stats VideoSendStream::GetStats() {
  return stats_proxy_.GetStats();
}

void VideoSendStream::OveruseDetected() {
  if (config_.overuse_callback)
    config_.overuse_callback->OnLoadUpdate(LoadObserver::kOveruse);
}

void VideoSendStream::NormalUsage() {
  if (config_.overuse_callback)
    config_.overuse_callback->OnLoadUpdate(LoadObserver::kUnderuse);
}

void VideoSendStream::ConfigureSsrcs() {
  vie_channel_->SetSSRC(config_.rtp.ssrcs.front(), kViEStreamTypeNormal, 0);
  for (size_t i = 0; i < config_.rtp.ssrcs.size(); ++i) {
    uint32_t ssrc = config_.rtp.ssrcs[i];
    vie_channel_->SetSSRC(ssrc, kViEStreamTypeNormal,
                          static_cast<unsigned char>(i));
    RtpStateMap::iterator it = suspended_ssrcs_.find(ssrc);
    if (it != suspended_ssrcs_.end())
      vie_channel_->SetRtpStateForSsrc(ssrc, it->second);
  }

  if (config_.rtp.rtx.ssrcs.empty()) {
    return;
  }

  // Set up RTX.
  RTC_DCHECK_EQ(config_.rtp.rtx.ssrcs.size(), config_.rtp.ssrcs.size());
  for (size_t i = 0; i < config_.rtp.rtx.ssrcs.size(); ++i) {
    uint32_t ssrc = config_.rtp.rtx.ssrcs[i];
    vie_channel_->SetSSRC(config_.rtp.rtx.ssrcs[i], kViEStreamTypeRtx,
                          static_cast<unsigned char>(i));
    RtpStateMap::iterator it = suspended_ssrcs_.find(ssrc);
    if (it != suspended_ssrcs_.end())
      vie_channel_->SetRtpStateForSsrc(ssrc, it->second);
  }

  RTC_DCHECK_GE(config_.rtp.rtx.payload_type, 0);
  vie_channel_->SetRtxSendPayloadType(config_.rtp.rtx.payload_type,
                                      config_.encoder_settings.payload_type);
}

std::map<uint32_t, RtpState> VideoSendStream::GetRtpStates() const {
  std::map<uint32_t, RtpState> rtp_states;
  for (size_t i = 0; i < config_.rtp.ssrcs.size(); ++i) {
    uint32_t ssrc = config_.rtp.ssrcs[i];
    rtp_states[ssrc] = vie_channel_->GetRtpStateForSsrc(ssrc);
  }

  for (size_t i = 0; i < config_.rtp.rtx.ssrcs.size(); ++i) {
    uint32_t ssrc = config_.rtp.rtx.ssrcs[i];
    rtp_states[ssrc] = vie_channel_->GetRtpStateForSsrc(ssrc);
  }

  return rtp_states;
}

void VideoSendStream::SignalNetworkState(NetworkState state) {
  // When network goes up, enable RTCP status before setting transmission state.
  // When it goes down, disable RTCP afterwards. This ensures that any packets
  // sent due to the network state changed will not be dropped.
  if (state == kNetworkUp)
    vie_channel_->SetRTCPMode(config_.rtp.rtcp_mode);
  vie_encoder_->SetNetworkTransmissionState(state == kNetworkUp);
  if (state == kNetworkDown)
    vie_channel_->SetRTCPMode(RtcpMode::kOff);
}

int64_t VideoSendStream::GetRtt() const {
  webrtc::RtcpStatistics rtcp_stats;
  uint16_t frac_lost;
  uint32_t cumulative_lost;
  uint32_t extended_max_sequence_number;
  uint32_t jitter;
  int64_t rtt_ms;
  if (vie_channel_->GetSendRtcpStatistics(&frac_lost, &cumulative_lost,
                                          &extended_max_sequence_number,
                                          &jitter, &rtt_ms) == 0) {
    return rtt_ms;
  }
  return -1;
}

int VideoSendStream::GetPaddingNeededBps() const {
  return vie_encoder_->GetPaddingNeededBps();
}

bool VideoSendStream::SetSendCodec(VideoCodec video_codec) {
  static const int kEncoderMinBitrate = 30;
  if (video_codec.maxBitrate == 0) {
    // Unset max bitrate -> cap to one bit per pixel.
    video_codec.maxBitrate =
        (video_codec.width * video_codec.height * video_codec.maxFramerate) /
        1000;
  }

  if (video_codec.minBitrate < kEncoderMinBitrate)
    video_codec.minBitrate = kEncoderMinBitrate;
  if (video_codec.maxBitrate < kEncoderMinBitrate)
    video_codec.maxBitrate = kEncoderMinBitrate;

  // Stop the media flow while reconfiguring.
  vie_encoder_->Pause();

  if (vie_encoder_->SetEncoder(video_codec) != 0) {
    LOG(LS_ERROR) << "Failed to set encoder.";
    return false;
  }

  if (vie_channel_->SetSendCodec(video_codec, false) != 0) {
    LOG(LS_ERROR) << "Failed to set send codec.";
    return false;
  }

  // Not all configured SSRCs have to be utilized (simulcast senders don't have
  // to send on all SSRCs at once etc.)
  std::vector<uint32_t> used_ssrcs = config_.rtp.ssrcs;
  used_ssrcs.resize(static_cast<size_t>(video_codec.numberOfSimulcastStreams));
  vie_encoder_->SetSsrcs(used_ssrcs);

  // Restart the media flow
  vie_encoder_->Restart();

  return true;
}

}  // namespace internal
}  // namespace webrtc
