/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/video_engine/internal/video_receive_stream.h"

#include <assert.h>
#include <stdlib.h>

#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"
#include "webrtc/system_wrappers/interface/clock.h"
#include "webrtc/video_engine/include/vie_base.h"
#include "webrtc/video_engine/include/vie_capture.h"
#include "webrtc/video_engine/include/vie_codec.h"
#include "webrtc/video_engine/include/vie_external_codec.h"
#include "webrtc/video_engine/include/vie_network.h"
#include "webrtc/video_engine/include/vie_render.h"
#include "webrtc/video_engine/include/vie_rtp_rtcp.h"
#include "webrtc/video_engine/new_include/video_receive_stream.h"

namespace webrtc {
namespace internal {

VideoReceiveStream::VideoReceiveStream(webrtc::VideoEngine* video_engine,
                                       const VideoReceiveStream::Config& config,
                                       newapi::Transport* transport)
    : transport_adapter_(transport), config_(config), channel_(-1) {
  video_engine_base_ = ViEBase::GetInterface(video_engine);
  // TODO(mflodman): Use the other CreateChannel method.
  video_engine_base_->CreateChannel(channel_);
  assert(channel_ != -1);

  rtp_rtcp_ = ViERTP_RTCP::GetInterface(video_engine);
  assert(rtp_rtcp_ != NULL);

  // TODO(pbos): This is not fine grained enough...
  rtp_rtcp_->SetNACKStatus(channel_, config_.rtp.nack.rtp_history_ms > 0);
  rtp_rtcp_->SetKeyFrameRequestMethod(channel_, kViEKeyFrameRequestPliRtcp);

  assert(config_.rtp.ssrc != 0);

  network_ = ViENetwork::GetInterface(video_engine);
  assert(network_ != NULL);

  network_->RegisterSendTransport(channel_, transport_adapter_);

  codec_ = ViECodec::GetInterface(video_engine);

  for (size_t i = 0; i < config_.codecs.size(); ++i) {
    if (codec_->SetReceiveCodec(channel_, config_.codecs[i]) != 0) {
      // TODO(pbos): Abort gracefully, this can be a runtime error.
      //             Factor out to an Init() method.
      abort();
    }
  }

  external_codec_ = ViEExternalCodec::GetInterface(video_engine);
  for (size_t i = 0; i < config_.external_decoders.size(); ++i) {
    ExternalVideoDecoder* decoder = &config_.external_decoders[i];
    if (external_codec_->RegisterExternalReceiveCodec(
            channel_,
            decoder->payload_type,
            decoder->decoder,
            decoder->renderer,
            decoder->expected_delay_ms) !=
        0) {
      // TODO(pbos): Abort gracefully? Can this be a runtime error?
      abort();
    }
  }

  render_ = webrtc::ViERender::GetInterface(video_engine);
  assert(render_ != NULL);

  if (render_->AddRenderer(channel_, kVideoI420, this) != 0) {
    abort();
  }

  clock_ = Clock::GetRealTimeClock();
}

VideoReceiveStream::~VideoReceiveStream() {
  for (size_t i = 0; i < config_.external_decoders.size(); ++i) {
    external_codec_->DeRegisterExternalReceiveCodec(
        channel_, config_.external_decoders[i].payload_type);
  }

  network_->DeregisterSendTransport(channel_);

  video_engine_base_->Release();
  external_codec_->Release();
  codec_->Release();
  network_->Release();
  render_->Release();
  rtp_rtcp_->Release();
}

void VideoReceiveStream::StartReceive() {
  if (render_->StartRender(channel_)) {
    abort();
  }
  if (video_engine_base_->StartReceive(channel_) != 0) {
    abort();
  }
}

void VideoReceiveStream::StopReceive() {
  if (render_->StopRender(channel_)) {
    abort();
  }
  if (video_engine_base_->StopReceive(channel_) != 0) {
    abort();
  }
}

void VideoReceiveStream::GetCurrentReceiveCodec(VideoCodec* receive_codec) {
  // TODO(pbos): Implement
}

bool VideoReceiveStream::DeliverRtcp(const uint8_t* packet, size_t length) {
  return network_->ReceivedRTCPPacket(channel_, packet, length) == 0;
}

bool VideoReceiveStream::DeliverRtp(const uint8_t* packet, size_t length) {
  return network_->ReceivedRTPPacket(channel_, packet, length) == 0;
}

int VideoReceiveStream::FrameSizeChange(unsigned int width,
                                        unsigned int height,
                                        unsigned int /*number_of_streams*/) {
  width_ = width;
  height_ = height;
  return 0;
}

int VideoReceiveStream::DeliverFrame(uint8_t* frame,
                                     int buffer_size,
                                     uint32_t timestamp,
                                     int64_t render_time,
                                     void* /*handle*/) {
  if (config_.renderer == NULL) {
    return 0;
  }

  I420VideoFrame video_frame;
  video_frame.CreateEmptyFrame(width_, height_, width_, height_, height_);
  ConvertToI420(kI420,
                frame,
                0,
                0,
                width_,
                height_,
                buffer_size,
                webrtc::kRotateNone,
                &video_frame);
  video_frame.set_timestamp(timestamp);
  video_frame.set_render_time_ms(render_time);

  if (config_.post_decode_callback != NULL) {
    config_.post_decode_callback->FrameCallback(&video_frame);
  }

  if (config_.renderer != NULL) {
    // TODO(pbos): Add timing to RenderFrame call
    config_.renderer->RenderFrame(video_frame,
                                  render_time - clock_->TimeInMilliseconds());
  }

  return 0;
}

bool VideoReceiveStream::IsTextureSupported() { return false; }

}  // internal
}  // webrtc
