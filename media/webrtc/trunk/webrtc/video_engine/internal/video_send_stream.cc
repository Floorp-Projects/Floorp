/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/video_engine/internal/video_send_stream.h"

#include <string.h>

#include <vector>

#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"
#include "webrtc/video_engine/include/vie_base.h"
#include "webrtc/video_engine/include/vie_capture.h"
#include "webrtc/video_engine/include/vie_codec.h"
#include "webrtc/video_engine/include/vie_external_codec.h"
#include "webrtc/video_engine/include/vie_network.h"
#include "webrtc/video_engine/include/vie_rtp_rtcp.h"
#include "webrtc/video_engine/new_include/video_send_stream.h"

namespace webrtc {
namespace internal {

// Super simple and temporary overuse logic. This will move to the application
// as soon as the new API allows changing send codec on the fly.
class ResolutionAdaptor : public webrtc::CpuOveruseObserver {
 public:
  ResolutionAdaptor(ViECodec* codec, int channel, size_t width, size_t height)
      : codec_(codec),
        channel_(channel),
        max_width_(width),
        max_height_(height) {}

  virtual ~ResolutionAdaptor() {}

  virtual void OveruseDetected() OVERRIDE {
    VideoCodec codec;
    if (codec_->GetSendCodec(channel_, codec) != 0)
      return;

    if (codec.width / 2 < min_width || codec.height / 2 < min_height)
      return;

    codec.width /= 2;
    codec.height /= 2;
    codec_->SetSendCodec(channel_, codec);
  }

  virtual void NormalUsage() OVERRIDE {
    VideoCodec codec;
    if (codec_->GetSendCodec(channel_, codec) != 0)
      return;

    if (codec.width * 2u > max_width_ || codec.height * 2u > max_height_)
      return;

    codec.width *= 2;
    codec.height *= 2;
    codec_->SetSendCodec(channel_, codec);
  }

 private:
  // Temporary and arbitrary chosen minimum resolution.
  static const size_t min_width = 160;
  static const size_t min_height = 120;

  ViECodec* codec_;
  const int channel_;

  const size_t max_width_;
  const size_t max_height_;
};

VideoSendStream::VideoSendStream(newapi::Transport* transport,
                                 bool overuse_detection,
                                 webrtc::VideoEngine* video_engine,
                                 const VideoSendStream::Config& config)
    : transport_adapter_(transport), config_(config), external_codec_(NULL) {

  if (config_.codec.numberOfSimulcastStreams > 0) {
    assert(config_.rtp.ssrcs.size() == config_.codec.numberOfSimulcastStreams);
  } else {
    assert(config_.rtp.ssrcs.size() == 1);
  }

  video_engine_base_ = ViEBase::GetInterface(video_engine);
  video_engine_base_->CreateChannel(channel_);
  assert(channel_ != -1);

  rtp_rtcp_ = ViERTP_RTCP::GetInterface(video_engine);
  assert(rtp_rtcp_ != NULL);

  if (config_.rtp.ssrcs.size() == 1) {
    rtp_rtcp_->SetLocalSSRC(channel_, config_.rtp.ssrcs[0]);
  } else {
    for (size_t i = 0; i < config_.rtp.ssrcs.size(); ++i) {
      rtp_rtcp_->SetLocalSSRC(channel_, config_.rtp.ssrcs[i],
                              kViEStreamTypeNormal, i);
    }
  }
  rtp_rtcp_->SetTransmissionSmoothingStatus(channel_, config_.pacing);
  if (!config_.rtp.rtx.ssrcs.empty()) {
    assert(config_.rtp.rtx.ssrcs.size() == config_.rtp.ssrcs.size());
    for (size_t i = 0; i < config_.rtp.rtx.ssrcs.size(); ++i) {
      rtp_rtcp_->SetLocalSSRC(
          channel_, config_.rtp.rtx.ssrcs[i], kViEStreamTypeRtx, i);
    }

    if (config_.rtp.rtx.rtx_payload_type != 0) {
      rtp_rtcp_->SetRtxSendPayloadType(channel_,
                                       config_.rtp.rtx.rtx_payload_type);
    }
  }

  for (size_t i = 0; i < config_.rtp.extensions.size(); ++i) {
    const std::string& extension = config_.rtp.extensions[i].name;
    int id = config_.rtp.extensions[i].id;
    if (extension == "toffset") {
      if (rtp_rtcp_->SetSendTimestampOffsetStatus(channel_, true, id) != 0)
        abort();
    } else if (extension == "abs-send-time") {
      if (rtp_rtcp_->SetSendAbsoluteSendTimeStatus(channel_, true, id) != 0)
        abort();
    } else {
      abort();  // Unsupported extension.
    }
  }

  // Enable NACK, FEC or both.
  if (config_.rtp.fec.red_payload_type != -1) {
    assert(config_.rtp.fec.ulpfec_payload_type != -1);
    if (config_.rtp.nack.rtp_history_ms > 0) {
      rtp_rtcp_->SetHybridNACKFECStatus(
          channel_,
          true,
          static_cast<unsigned char>(config_.rtp.fec.red_payload_type),
          static_cast<unsigned char>(config_.rtp.fec.ulpfec_payload_type));
    } else {
      rtp_rtcp_->SetFECStatus(
          channel_,
          true,
          static_cast<unsigned char>(config_.rtp.fec.red_payload_type),
          static_cast<unsigned char>(config_.rtp.fec.ulpfec_payload_type));
    }
  } else {
    rtp_rtcp_->SetNACKStatus(channel_, config_.rtp.nack.rtp_history_ms > 0);
  }

  char rtcp_cname[ViERTP_RTCP::KMaxRTCPCNameLength];
  assert(config_.rtp.c_name.length() < ViERTP_RTCP::KMaxRTCPCNameLength);
  strncpy(rtcp_cname, config_.rtp.c_name.c_str(), sizeof(rtcp_cname) - 1);
  rtcp_cname[sizeof(rtcp_cname) - 1] = '\0';

  rtp_rtcp_->SetRTCPCName(channel_, rtcp_cname);

  capture_ = ViECapture::GetInterface(video_engine);
  capture_->AllocateExternalCaptureDevice(capture_id_, external_capture_);
  capture_->ConnectCaptureDevice(capture_id_, channel_);

  network_ = ViENetwork::GetInterface(video_engine);
  assert(network_ != NULL);

  network_->RegisterSendTransport(channel_, transport_adapter_);

  if (config.encoder) {
    external_codec_ = ViEExternalCodec::GetInterface(video_engine);
    if (external_codec_->RegisterExternalSendCodec(
        channel_, config.codec.plType, config.encoder,
        config.internal_source) != 0) {
      abort();
    }
  }

  codec_ = ViECodec::GetInterface(video_engine);
  if (codec_->SetSendCodec(channel_, config_.codec) != 0) {
    abort();
  }

  if (overuse_detection) {
    overuse_observer_.reset(
        new ResolutionAdaptor(codec_, channel_, config_.codec.width,
                              config_.codec.height));
    video_engine_base_->RegisterCpuOveruseObserver(channel_,
                                                   overuse_observer_.get());
  }
}

VideoSendStream::~VideoSendStream() {
  network_->DeregisterSendTransport(channel_);
  video_engine_base_->DeleteChannel(channel_);

  capture_->DisconnectCaptureDevice(channel_);
  capture_->ReleaseCaptureDevice(capture_id_);

  if (external_codec_) {
    external_codec_->DeRegisterExternalSendCodec(channel_,
                                                 config_.codec.plType);
  }

  video_engine_base_->Release();
  capture_->Release();
  codec_->Release();
  if (external_codec_)
    external_codec_->Release();
  network_->Release();
  rtp_rtcp_->Release();
}

void VideoSendStream::PutFrame(const I420VideoFrame& frame,
                               uint32_t time_since_capture_ms) {
  // TODO(pbos): frame_copy should happen after the VideoProcessingModule has
  //             resized the frame.
  I420VideoFrame frame_copy;
  frame_copy.CopyFrame(frame);

  if (config_.pre_encode_callback != NULL) {
    config_.pre_encode_callback->FrameCallback(&frame_copy);
  }

  ViEVideoFrameI420 vf;

  // TODO(pbos): This represents a memcpy step and is only required because
  //             external_capture_ only takes ViEVideoFrameI420s.
  vf.y_plane = frame_copy.buffer(kYPlane);
  vf.u_plane = frame_copy.buffer(kUPlane);
  vf.v_plane = frame_copy.buffer(kVPlane);
  vf.y_pitch = frame.stride(kYPlane);
  vf.u_pitch = frame.stride(kUPlane);
  vf.v_pitch = frame.stride(kVPlane);
  vf.width = frame.width();
  vf.height = frame.height();

  external_capture_->IncomingFrameI420(vf, frame.render_time_ms());

  if (config_.local_renderer != NULL) {
    config_.local_renderer->RenderFrame(frame, 0);
  }
}

VideoSendStreamInput* VideoSendStream::Input() { return this; }

void VideoSendStream::StartSend() {
  if (video_engine_base_->StartSend(channel_) != 0)
    abort();
  if (video_engine_base_->StartReceive(channel_) != 0)
    abort();
}

void VideoSendStream::StopSend() {
  if (video_engine_base_->StopSend(channel_) != 0)
    abort();
  if (video_engine_base_->StopReceive(channel_) != 0)
    abort();
}

bool VideoSendStream::SetTargetBitrate(
    int min_bitrate,
    int max_bitrate,
    const std::vector<SimulcastStream>& streams) {
  return false;
}

void VideoSendStream::GetSendCodec(VideoCodec* send_codec) {
  *send_codec = config_.codec;
}

bool VideoSendStream::DeliverRtcp(const uint8_t* packet, size_t length) {
  return network_->ReceivedRTCPPacket(
             channel_, packet, static_cast<int>(length)) == 0;
}

}  // namespace internal
}  // namespace webrtc
