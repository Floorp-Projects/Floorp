/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/video_engine/vie_external_codec_impl.h"

#include "webrtc/engine_configurations.h"
#include "webrtc/system_wrappers/interface/logging.h"
#include "webrtc/video_engine/include/vie_errors.h"
#include "webrtc/video_engine/vie_channel.h"
#include "webrtc/video_engine/vie_channel_manager.h"
#include "webrtc/video_engine/vie_encoder.h"
#include "webrtc/video_engine/vie_impl.h"
#include "webrtc/video_engine/vie_shared_data.h"

namespace webrtc {

ViEExternalCodec* ViEExternalCodec::GetInterface(VideoEngine* video_engine) {
#ifdef WEBRTC_VIDEO_ENGINE_EXTERNAL_CODEC_API
  if (video_engine == NULL) {
    return NULL;
  }
  VideoEngineImpl* vie_impl = static_cast<VideoEngineImpl*>(video_engine);
  ViEExternalCodecImpl* vie_external_codec_impl = vie_impl;
  // Increase ref count.
  (*vie_external_codec_impl)++;
  return vie_external_codec_impl;
#else
  return NULL;
#endif
}

int ViEExternalCodecImpl::Release() {
  // Decrease ref count.
  (*this)--;

  int32_t ref_count = GetCount();
  if (ref_count < 0) {
    LOG(LS_WARNING) << "ViEExternalCodec released too many times.";
    shared_data_->SetLastError(kViEAPIDoesNotExist);
    return -1;
  }
  return ref_count;
}

ViEExternalCodecImpl::ViEExternalCodecImpl(ViESharedData* shared_data)
    : shared_data_(shared_data) {
}

ViEExternalCodecImpl::~ViEExternalCodecImpl() {
}

int ViEExternalCodecImpl::RegisterExternalSendCodec(const int video_channel,
                                                    const unsigned char pl_type,
                                                    VideoEncoder* encoder,
                                                    bool internal_source) {
  assert(encoder != NULL);
  LOG(LS_INFO) << "Register external encoder for channel " << video_channel
               << ", pl_type " << static_cast<int>(pl_type)
               << ", internal_source " << internal_source;

  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEEncoder* vie_encoder = cs.Encoder(video_channel);
  if (!vie_encoder) {
    shared_data_->SetLastError(kViECodecInvalidArgument);
    return -1;
  }
  if (vie_encoder->RegisterExternalEncoder(encoder, pl_type,
                                           internal_source) != 0) {
    shared_data_->SetLastError(kViECodecUnknownError);
    return -1;
  }
  return 0;
}

int ViEExternalCodecImpl::DeRegisterExternalSendCodec(
  const int video_channel, const unsigned char pl_type) {
  LOG(LS_INFO) << "Deregister external encoder for channel " << video_channel;

  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEEncoder* vie_encoder = cs.Encoder(video_channel);
  if (!vie_encoder) {
    shared_data_->SetLastError(kViECodecInvalidArgument);
    return -1;
  }

  if (vie_encoder->DeRegisterExternalEncoder(pl_type) != 0) {
    shared_data_->SetLastError(kViECodecUnknownError);
    return -1;
  }
  return 0;
}

int ViEExternalCodecImpl::RegisterExternalReceiveCodec(
    const int video_channel,
    const unsigned char pl_type,
    VideoDecoder* decoder,
    bool decoder_render,
    int render_delay) {
  LOG(LS_INFO) << "Register external decoder for channel " << video_channel
               << ", pl_type " << static_cast<int>(pl_type)
               << ", decoder_render " << decoder_render
               << ", render_delay " << render_delay;
  assert(decoder != NULL);

  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    shared_data_->SetLastError(kViECodecInvalidArgument);
    return -1;
  }

  if (vie_channel->RegisterExternalDecoder(pl_type, decoder, decoder_render,
                                           render_delay) != 0) {
    shared_data_->SetLastError(kViECodecUnknownError);
    return -1;
  }
  return 0;
}

int ViEExternalCodecImpl::DeRegisterExternalReceiveCodec(
    const int video_channel, const unsigned char pl_type) {
  LOG(LS_INFO) << "DeRegisterExternalReceiveCodec for channel " << video_channel
               << ", pl_type " << static_cast<int>(pl_type);

  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    shared_data_->SetLastError(kViECodecInvalidArgument);
    return -1;
  }
  if (vie_channel->DeRegisterExternalDecoder(pl_type) != 0) {
    shared_data_->SetLastError(kViECodecUnknownError);
    return -1;
  }
  return 0;
}

}  // namespace webrtc
