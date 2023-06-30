/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "media/engine/encoder_simulcast_proxy.h"

#include "api/video_codecs/video_encoder.h"
#include "media/engine/simulcast_encoder_adapter.h"
#include "modules/video_coding/include/video_error_codes.h"

namespace webrtc {

EncoderSimulcastProxy::EncoderSimulcastProxy(VideoEncoderFactory* factory,
                                             const SdpVideoFormat& format)
    : factory_(factory), video_format_(format), callback_(nullptr) {
  encoder_ = factory_->CreateVideoEncoder(format);
}

EncoderSimulcastProxy::~EncoderSimulcastProxy() = default;

int EncoderSimulcastProxy::Release() {
  return encoder_->Release();
}

void EncoderSimulcastProxy::SetFecControllerOverride(
    FecControllerOverride* fec_controller_override) {
  encoder_->SetFecControllerOverride(fec_controller_override);
}

// TODO(eladalon): s/inst/codec_settings/g.
int EncoderSimulcastProxy::InitEncode(const VideoCodec* inst,
                                      const VideoEncoder::Settings& settings) {
  int ret;
  if (inst->numberOfSimulcastStreams <= 1 ||
      encoder_->GetEncoderInfo().supports_simulcast) {
    // A simulcast capable encoder may still return
    // WEBRTC_VIDEO_CODEC_ERR_SIMULCAST_PARAMETERS_NOT_SUPPORTED if the current
    // configuration is not supported.
    ret = encoder_->InitEncode(inst, settings);
  } else {
    // If the encoder does not support simulcast, fallback to
    // SimulcastEncoderAdapter without trying to InitEncode().
    // TODO(https://crbug.com/webrtc/14884): Delete EncoderSimulcastProxy and
    // always use the simulcast adapter instead; it has a passthrough mode so
    // this proxy is an unnecessary layer.
    ret = WEBRTC_VIDEO_CODEC_ERR_SIMULCAST_PARAMETERS_NOT_SUPPORTED;
  }
  if (ret == WEBRTC_VIDEO_CODEC_ERR_SIMULCAST_PARAMETERS_NOT_SUPPORTED) {
    encoder_.reset(new SimulcastEncoderAdapter(factory_, video_format_));
    if (callback_) {
      encoder_->RegisterEncodeCompleteCallback(callback_);
    }
    ret = encoder_->InitEncode(inst, settings);
  }
  return ret;
}

int EncoderSimulcastProxy::Encode(
    const VideoFrame& input_image,
    const std::vector<VideoFrameType>* frame_types) {
  return encoder_->Encode(input_image, frame_types);
}

int EncoderSimulcastProxy::RegisterEncodeCompleteCallback(
    EncodedImageCallback* callback) {
  callback_ = callback;
  return encoder_->RegisterEncodeCompleteCallback(callback);
}

void EncoderSimulcastProxy::SetRates(const RateControlParameters& parameters) {
  encoder_->SetRates(parameters);
}

void EncoderSimulcastProxy::OnPacketLossRateUpdate(float packet_loss_rate) {
  encoder_->OnPacketLossRateUpdate(packet_loss_rate);
}

void EncoderSimulcastProxy::OnRttUpdate(int64_t rtt_ms) {
  encoder_->OnRttUpdate(rtt_ms);
}

void EncoderSimulcastProxy::OnLossNotification(
    const LossNotification& loss_notification) {
  encoder_->OnLossNotification(loss_notification);
}

VideoEncoder::EncoderInfo EncoderSimulcastProxy::GetEncoderInfo() const {
  return encoder_->GetEncoderInfo();
}

}  // namespace webrtc
