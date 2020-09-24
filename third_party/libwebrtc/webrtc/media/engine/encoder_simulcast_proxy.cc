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

#include "media/engine/simulcast_encoder_adapter.h"
#include "modules/video_coding/include/video_error_codes.h"

namespace webrtc {
EncoderSimulcastProxy::EncoderSimulcastProxy(VideoEncoderFactory* factory,
                                             const SdpVideoFormat& format)
    : factory_(factory), video_format_(format), callback_(nullptr) {
  encoder_ = factory_->CreateVideoEncoder(format);
}

EncoderSimulcastProxy::EncoderSimulcastProxy(VideoEncoderFactory* factory)
    : EncoderSimulcastProxy(factory, SdpVideoFormat("VP8")) {}

EncoderSimulcastProxy::~EncoderSimulcastProxy() {}

int EncoderSimulcastProxy::Release() { return encoder_->Release(); }

int EncoderSimulcastProxy::InitEncode(const VideoCodec* inst,
                                      int number_of_cores,
                                      size_t max_payload_size) {
  int ret = encoder_->InitEncode(inst, number_of_cores, max_payload_size);
  if (ret == WEBRTC_VIDEO_CODEC_ERR_SIMULCAST_PARAMETERS_NOT_SUPPORTED) {
    encoder_.reset(new SimulcastEncoderAdapter(factory_));
    if (callback_) {
      encoder_->RegisterEncodeCompleteCallback(callback_);
    }
    ret = encoder_->InitEncode(inst, number_of_cores, max_payload_size);
  }
  return ret;
}

int EncoderSimulcastProxy::Encode(const VideoFrame& input_image,
                                  const CodecSpecificInfo* codec_specific_info,
                                  const std::vector<FrameType>* frame_types) {
  return encoder_->Encode(input_image, codec_specific_info, frame_types);
}

int EncoderSimulcastProxy::RegisterEncodeCompleteCallback(
    EncodedImageCallback* callback) {
  callback_ = callback;
  return encoder_->RegisterEncodeCompleteCallback(callback);
}

int EncoderSimulcastProxy::SetRateAllocation(const BitrateAllocation& bitrate,
                                             uint32_t new_framerate) {
  return encoder_->SetRateAllocation(bitrate, new_framerate);
}

int EncoderSimulcastProxy::SetChannelParameters(uint32_t packet_loss,
                                                int64_t rtt) {
  return encoder_->SetChannelParameters(packet_loss, rtt);
}

}  // namespace webrtc
