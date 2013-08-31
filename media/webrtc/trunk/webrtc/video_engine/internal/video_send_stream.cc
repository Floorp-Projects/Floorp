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

#include <vector>

#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"
#include "webrtc/video_engine/include/vie_base.h"
#include "webrtc/video_engine/include/vie_capture.h"
#include "webrtc/video_engine/include/vie_codec.h"
#include "webrtc/video_engine/include/vie_network.h"
#include "webrtc/video_engine/include/vie_rtp_rtcp.h"
#include "webrtc/video_engine/new_include/video_send_stream.h"

namespace webrtc {
namespace internal {

VideoSendStream::VideoSendStream(
    newapi::Transport* transport,
    webrtc::VideoEngine* video_engine,
    const newapi::VideoSendStream::Config& config)
    : transport_(transport), config_(config) {

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

  assert(config_.rtp.ssrcs.size() == 1);
  rtp_rtcp_->SetLocalSSRC(channel_, config_.rtp.ssrcs[0]);

  capture_ = ViECapture::GetInterface(video_engine);
  capture_->AllocateExternalCaptureDevice(capture_id_, external_capture_);
  capture_->ConnectCaptureDevice(capture_id_, channel_);

  network_ = ViENetwork::GetInterface(video_engine);
  assert(network_ != NULL);

  network_->RegisterSendTransport(channel_, *this);

  codec_ = ViECodec::GetInterface(video_engine);
  if (codec_->SetSendCodec(channel_, config_.codec) != 0) {
    abort();
  }
}

VideoSendStream::~VideoSendStream() {
  network_->DeregisterSendTransport(channel_);
  video_engine_base_->DeleteChannel(channel_);

  capture_->DisconnectCaptureDevice(channel_);
  capture_->ReleaseCaptureDevice(capture_id_);

  video_engine_base_->Release();
  capture_->Release();
  codec_->Release();
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

newapi::VideoSendStreamInput* VideoSendStream::Input() { return this; }

void VideoSendStream::StartSend() {
  if (video_engine_base_->StartSend(channel_) != 0)
    abort();
}

void VideoSendStream::StopSend() {
  if (video_engine_base_->StopSend(channel_) != 0)
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

int VideoSendStream::SendPacket(int /*channel*/,
                                const void* packet,
                                int length) {
  // TODO(pbos): Lock these methods and the destructor so it can't be processing
  //             a packet when the destructor has been called.
  assert(length >= 0);
  return transport_->SendRTP(packet, static_cast<size_t>(length)) ? 0 : -1;
}

int VideoSendStream::SendRTCPPacket(int /*channel*/,
                                    const void* packet,
                                    int length) {
  assert(length >= 0);
  return transport_->SendRTCP(packet, static_cast<size_t>(length)) ? 0 : -1;
}

}  // namespace internal
}  // namespace webrtc
