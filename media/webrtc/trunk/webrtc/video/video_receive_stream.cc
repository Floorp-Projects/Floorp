/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/video/video_receive_stream.h"

#include <stdlib.h>

#include <string>

#include "webrtc/base/checks.h"
#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"
#include "webrtc/system_wrappers/interface/clock.h"
#include "webrtc/system_wrappers/interface/logging.h"
#include "webrtc/video/receive_statistics_proxy.h"
#include "webrtc/video_encoder.h"
#include "webrtc/video_engine/include/vie_base.h"
#include "webrtc/video_engine/include/vie_capture.h"
#include "webrtc/video_engine/include/vie_codec.h"
#include "webrtc/video_engine/include/vie_external_codec.h"
#include "webrtc/video_engine/include/vie_image_process.h"
#include "webrtc/video_engine/include/vie_network.h"
#include "webrtc/video_engine/include/vie_render.h"
#include "webrtc/video_engine/include/vie_rtp_rtcp.h"
#include "webrtc/video_receive_stream.h"

namespace webrtc {
std::string VideoReceiveStream::Decoder::ToString() const {
  std::stringstream ss;
  ss << "{decoder: " << (decoder != nullptr ? "(VideoDecoder)" : "nullptr");
  ss << ", payload_type: " << payload_type;
  ss << ", payload_name: " << payload_name;
  ss << ", is_renderer: " << (is_renderer ? "yes" : "no");
  ss << ", expected_delay_ms: " << expected_delay_ms;
  ss << '}';

  return ss.str();
}

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
  ss << ", renderer: " << (renderer != nullptr ? "(renderer)" : "nullptr");
  ss << ", render_delay_ms: " << render_delay_ms;
  ss << ", audio_channel_id: " << audio_channel_id;
  ss << ", pre_decode_callback: "
     << (pre_decode_callback != nullptr ? "(EncodedFrameObserver)" : "nullptr");
  ss << ", pre_render_callback: "
     << (pre_render_callback != nullptr ? "(I420FrameCallback)" : "nullptr");
  ss << ", target_delay_ms: " << target_delay_ms;
  ss << '}';

  return ss.str();
}

std::string VideoReceiveStream::Config::Rtp::ToString() const {
  std::stringstream ss;
  ss << "{remote_ssrc: " << remote_ssrc;
  ss << ", local_ssrc: " << local_ssrc;
  ss << ", rtcp_mode: " << (rtcp_mode == newapi::kRtcpCompound
                                ? "kRtcpCompound"
                                : "kRtcpReducedSize");
  ss << ", rtcp_xr: ";
  ss << "{receiver_reference_time_report: "
     << (rtcp_xr.receiver_reference_time_report ? "on" : "off");
  ss << '}';
  ss << ", remb: " << (remb ? "on" : "off");
  ss << ", nack: {rtp_history_ms: " << nack.rtp_history_ms << '}';
  ss << ", fec: " << fec.ToString();
  ss << ", rtx: {";
  for (auto& kv : rtx) {
    ss << kv.first << " -> ";
    ss << "{ssrc: " << kv.second.ssrc;
    ss << ", payload_type: " << kv.second.payload_type;
    ss << '}';
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

namespace internal {
namespace {

VideoCodec CreateDecoderVideoCodec(const VideoReceiveStream::Decoder& decoder) {
  VideoCodec codec;
  memset(&codec, 0, sizeof(codec));

  codec.plType = decoder.payload_type;
  strcpy(codec.plName, decoder.payload_name.c_str());
  if (decoder.payload_name == "VP8") {
    codec.codecType = kVideoCodecVP8;
  } else if (decoder.payload_name == "H264") {
    codec.codecType = kVideoCodecH264;
  } else {
    codec.codecType = kVideoCodecGeneric;
  }

  if (codec.codecType == kVideoCodecVP8) {
    codec.codecSpecific.VP8 = VideoEncoder::GetDefaultVp8Settings();
  } else if (codec.codecType == kVideoCodecH264) {
    codec.codecSpecific.H264 = VideoEncoder::GetDefaultH264Settings();
  }

  codec.width = 320;
  codec.height = 180;
  codec.startBitrate = codec.minBitrate = codec.maxBitrate =
      Call::Config::kDefaultStartBitrateBps / 1000;

  return codec;
}
}  // namespace

VideoReceiveStream::VideoReceiveStream(webrtc::VideoEngine* video_engine,
                                       const VideoReceiveStream::Config& config,
                                       newapi::Transport* transport,
                                       webrtc::VoiceEngine* voice_engine,
                                       int base_channel)
    : transport_adapter_(transport),
      encoded_frame_proxy_(config.pre_decode_callback),
      config_(config),
      clock_(Clock::GetRealTimeClock()),
      channel_(-1) {
  video_engine_base_ = ViEBase::GetInterface(video_engine);
  video_engine_base_->CreateReceiveChannel(channel_, base_channel);
  DCHECK(channel_ != -1);

  rtp_rtcp_ = ViERTP_RTCP::GetInterface(video_engine);
  DCHECK(rtp_rtcp_ != nullptr);

  // TODO(pbos): This is not fine grained enough...
  rtp_rtcp_->SetNACKStatus(channel_, config_.rtp.nack.rtp_history_ms > 0);
  rtp_rtcp_->SetKeyFrameRequestMethod(channel_, kViEKeyFrameRequestPliRtcp);
  SetRtcpMode(config_.rtp.rtcp_mode);

  DCHECK(config_.rtp.remote_ssrc != 0);
  // TODO(pbos): What's an appropriate local_ssrc for receive-only streams?
  DCHECK(config_.rtp.local_ssrc != 0);
  DCHECK(config_.rtp.remote_ssrc != config_.rtp.local_ssrc);

  rtp_rtcp_->SetLocalSSRC(channel_, config_.rtp.local_ssrc);
  // TODO(pbos): Support multiple RTX, per video payload.
  Config::Rtp::RtxMap::const_iterator it = config_.rtp.rtx.begin();
  if (it != config_.rtp.rtx.end()) {
    DCHECK(it->second.ssrc != 0);
    DCHECK(it->second.payload_type != 0);

    rtp_rtcp_->SetRemoteSSRCType(channel_, kViEStreamTypeRtx, it->second.ssrc);
    rtp_rtcp_->SetRtxReceivePayloadType(channel_, it->second.payload_type);
  }

  rtp_rtcp_->SetRembStatus(channel_, false, config_.rtp.remb);

  for (size_t i = 0; i < config_.rtp.extensions.size(); ++i) {
    const std::string& extension = config_.rtp.extensions[i].name;
    int id = config_.rtp.extensions[i].id;
    // One-byte-extension local identifiers are in the range 1-14 inclusive.
    DCHECK_GE(id, 1);
    DCHECK_LE(id, 14);
    if (extension == RtpExtension::kTOffset) {
      CHECK_EQ(0,
               rtp_rtcp_->SetReceiveTimestampOffsetStatus(channel_, true, id));
    } else if (extension == RtpExtension::kAbsSendTime) {
      CHECK_EQ(0,
               rtp_rtcp_->SetReceiveAbsoluteSendTimeStatus(channel_, true, id));
    } else if (extension == RtpExtension::kVideoRotation) {
      CHECK_EQ(0, rtp_rtcp_->SetReceiveVideoRotationStatus(channel_, true, id));
    } else {
      RTC_NOTREACHED() << "Unsupported RTP extension.";
    }
  }

  network_ = ViENetwork::GetInterface(video_engine);
  DCHECK(network_ != nullptr);

  network_->RegisterSendTransport(channel_, transport_adapter_);

  codec_ = ViECodec::GetInterface(video_engine);

  if (config_.rtp.fec.ulpfec_payload_type != -1) {
    // ULPFEC without RED doesn't make sense.
    DCHECK(config_.rtp.fec.red_payload_type != -1);
    VideoCodec codec;
    memset(&codec, 0, sizeof(codec));
    codec.codecType = kVideoCodecULPFEC;
    strcpy(codec.plName, "ulpfec");
    codec.plType = config_.rtp.fec.ulpfec_payload_type;
    CHECK_EQ(0, codec_->SetReceiveCodec(channel_, codec));
  }
  if (config_.rtp.fec.red_payload_type != -1) {
    VideoCodec codec;
    memset(&codec, 0, sizeof(codec));
    codec.codecType = kVideoCodecRED;
    strcpy(codec.plName, "red");
    codec.plType = config_.rtp.fec.red_payload_type;
    CHECK_EQ(0, codec_->SetReceiveCodec(channel_, codec));
  }

  stats_proxy_.reset(
      new ReceiveStatisticsProxy(config_.rtp.remote_ssrc, clock_));

  CHECK_EQ(0, rtp_rtcp_->RegisterReceiveChannelRtcpStatisticsCallback(
                  channel_, stats_proxy_.get()));
  CHECK_EQ(0, rtp_rtcp_->RegisterReceiveChannelRtpStatisticsCallback(
                  channel_, stats_proxy_.get()));
  CHECK_EQ(0, rtp_rtcp_->RegisterRtcpPacketTypeCounterObserver(
                  channel_, stats_proxy_.get()));
  CHECK_EQ(0, codec_->RegisterDecoderObserver(channel_, *stats_proxy_));

  video_engine_base_->RegisterReceiveStatisticsProxy(channel_,
                                                     stats_proxy_.get());

  external_codec_ = ViEExternalCodec::GetInterface(video_engine);
  DCHECK(!config_.decoders.empty());
  for (size_t i = 0; i < config_.decoders.size(); ++i) {
    const Decoder& decoder = config_.decoders[i];
    CHECK_EQ(0, external_codec_->RegisterExternalReceiveCodec(
                    channel_, decoder.payload_type, decoder.decoder,
                    decoder.is_renderer, decoder.expected_delay_ms));

    VideoCodec codec = CreateDecoderVideoCodec(decoder);

    CHECK_EQ(0, codec_->SetReceiveCodec(channel_, codec));
  }

  render_ = ViERender::GetInterface(video_engine);
  DCHECK(render_ != nullptr);

  render_->AddRenderer(channel_, kVideoI420, this);

  if (voice_engine && config_.audio_channel_id != -1) {
    video_engine_base_->SetVoiceEngine(voice_engine);
    video_engine_base_->ConnectAudioChannel(channel_, config_.audio_channel_id);
  }

  image_process_ = ViEImageProcess::GetInterface(video_engine);
  if (config.pre_decode_callback) {
    image_process_->RegisterPreDecodeImageCallback(channel_,
                                                   &encoded_frame_proxy_);
  }
  image_process_->RegisterPreRenderCallback(channel_, this);

  if (config.rtp.rtcp_xr.receiver_reference_time_report) {
    rtp_rtcp_->SetRtcpXrRrtrStatus(channel_, true);
  }
}

VideoReceiveStream::~VideoReceiveStream() {
  image_process_->DeRegisterPreRenderCallback(channel_);
  image_process_->DeRegisterPreDecodeCallback(channel_);

  render_->RemoveRenderer(channel_);

  for (size_t i = 0; i < config_.decoders.size(); ++i) {
    external_codec_->DeRegisterExternalReceiveCodec(
        channel_, config_.decoders[i].payload_type);
  }

  network_->DeregisterSendTransport(channel_);

  video_engine_base_->SetVoiceEngine(nullptr);
  image_process_->Release();
  external_codec_->Release();
  codec_->DeregisterDecoderObserver(channel_);
  rtp_rtcp_->DeregisterReceiveChannelRtpStatisticsCallback(channel_,
                                                           stats_proxy_.get());
  rtp_rtcp_->DeregisterReceiveChannelRtcpStatisticsCallback(channel_,
                                                            stats_proxy_.get());
  rtp_rtcp_->RegisterRtcpPacketTypeCounterObserver(channel_, nullptr);
  codec_->Release();
  network_->Release();
  render_->Release();
  rtp_rtcp_->Release();
  video_engine_base_->DeleteChannel(channel_);
  video_engine_base_->Release();
}

void VideoReceiveStream::Start() {
  transport_adapter_.Enable();
  CHECK_EQ(0, render_->StartRender(channel_));
  CHECK_EQ(0, video_engine_base_->StartReceive(channel_));
}

void VideoReceiveStream::Stop() {
  CHECK_EQ(0, render_->StopRender(channel_));
  CHECK_EQ(0, video_engine_base_->StopReceive(channel_));
  transport_adapter_.Disable();
}

VideoReceiveStream::Stats VideoReceiveStream::GetStats() const {
  return stats_proxy_->GetStats();
}

bool VideoReceiveStream::DeliverRtcp(const uint8_t* packet, size_t length) {
  return network_->ReceivedRTCPPacket(channel_, packet, length) == 0;
}

bool VideoReceiveStream::DeliverRtp(const uint8_t* packet, size_t length) {
  return network_->ReceivedRTPPacket(channel_, packet, length, PacketTime()) ==
      0;
}

void VideoReceiveStream::FrameCallback(I420VideoFrame* video_frame) {
  stats_proxy_->OnDecodedFrame();

  if (config_.pre_render_callback)
    config_.pre_render_callback->FrameCallback(video_frame);
}

int VideoReceiveStream::FrameSizeChange(unsigned int width,
                                        unsigned int height,
                                        unsigned int number_of_streams) {
  return 0;
}

int VideoReceiveStream::DeliverFrame(unsigned char* buffer,
                                     size_t buffer_size,
                                     uint32_t timestamp,
                                     int64_t ntp_time_ms,
                                     int64_t render_time_ms,
                                     void* handle) {
  CHECK(false) << "Renderer should be configured as kVideoI420 and never "
                  "receive callbacks on DeliverFrame.";
  return 0;
}

int VideoReceiveStream::DeliverI420Frame(const I420VideoFrame& video_frame) {
  if (config_.renderer != nullptr)
    config_.renderer->RenderFrame(
        video_frame,
        video_frame.render_time_ms() - clock_->TimeInMilliseconds());

  stats_proxy_->OnRenderedFrame();

  return 0;
}

bool VideoReceiveStream::IsTextureSupported() {
  if (config_.renderer == nullptr)
    return false;
  return config_.renderer->IsTextureSupported();
}

void VideoReceiveStream::SignalNetworkState(Call::NetworkState state) {
  if (state == Call::kNetworkUp)
    SetRtcpMode(config_.rtp.rtcp_mode);
  network_->SetNetworkTransmissionState(channel_, state == Call::kNetworkUp);
  if (state == Call::kNetworkDown)
    rtp_rtcp_->SetRTCPStatus(channel_, kRtcpNone);
}

void VideoReceiveStream::SetRtcpMode(newapi::RtcpMode mode) {
  switch (mode) {
    case newapi::kRtcpCompound:
      rtp_rtcp_->SetRTCPStatus(channel_, kRtcpCompound_RFC4585);
      break;
    case newapi::kRtcpReducedSize:
      rtp_rtcp_->SetRTCPStatus(channel_, kRtcpNonCompound_RFC5506);
      break;
  }
}
}  // namespace internal
}  // namespace webrtc
