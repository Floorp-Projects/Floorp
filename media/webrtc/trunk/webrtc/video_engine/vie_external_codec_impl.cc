/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video_engine/vie_external_codec_impl.h"

#include "engine_configurations.h"  // NOLINT
#include "system_wrappers/interface/trace.h"
#include "video_engine/include/vie_errors.h"
#include "video_engine/vie_channel.h"
#include "video_engine/vie_channel_manager.h"
#include "video_engine/vie_encoder.h"
#include "video_engine/vie_impl.h"
#include "video_engine/vie_shared_data.h"

namespace webrtc {

ViEExternalCodec* ViEExternalCodec::GetInterface(VideoEngine* video_engine) {
#ifdef WEBRTC_VIDEO_ENGINE_EXTERNAL_CODEC_API
  if (video_engine == NULL) {
    return NULL;
  }
  VideoEngineImpl* vie_impl = reinterpret_cast<VideoEngineImpl*>(video_engine);
  ViEExternalCodecImpl* vie_external_codec_impl = vie_impl;
  // Increase ref count.
  (*vie_external_codec_impl)++;
  return vie_external_codec_impl;
#else
  return NULL;
#endif
}

int ViEExternalCodecImpl::Release() {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, shared_data_->instance_id(),
               "ViEExternalCodec::Release()");
  // Decrease ref count.
  (*this)--;

  int32_t ref_count = GetCount();
  if (ref_count < 0) {
    WEBRTC_TRACE(kTraceWarning, kTraceVideo, shared_data_->instance_id(),
                 "ViEExternalCodec release too many times");
    shared_data_->SetLastError(kViEAPIDoesNotExist);
    return -1;
  }
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, shared_data_->instance_id(),
               "ViEExternalCodec reference count: %d", ref_count);
  return ref_count;
}

ViEExternalCodecImpl::ViEExternalCodecImpl(ViESharedData* shared_data)
    : shared_data_(shared_data) {
  WEBRTC_TRACE(kTraceMemory, kTraceVideo, shared_data_->instance_id(),
               "ViEExternalCodecImpl::ViEExternalCodecImpl() Ctor");
}

ViEExternalCodecImpl::~ViEExternalCodecImpl() {
  WEBRTC_TRACE(kTraceMemory, kTraceVideo, shared_data_->instance_id(),
               "ViEExternalCodecImpl::~ViEExternalCodecImpl() Dtor");
}

int ViEExternalCodecImpl::RegisterExternalSendCodec(const int video_channel,
                                                    const unsigned char pl_type,
                                                    VideoEncoder* encoder,
                                                    bool internal_source) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, ViEId(shared_data_->instance_id()),
               "%s channel %d pl_type %d encoder 0x%x", __FUNCTION__,
               video_channel, pl_type, encoder);

  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEEncoder* vie_encoder = cs.Encoder(video_channel);
  if (!vie_encoder) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), video_channel),
                 "%s: Invalid argument video_channel %u. Does it exist?",
                 __FUNCTION__, video_channel);
    shared_data_->SetLastError(kViECodecInvalidArgument);
    return -1;
  }
  if (!encoder) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), video_channel),
                 "%s: Invalid argument Encoder 0x%x.", __FUNCTION__, encoder);
    shared_data_->SetLastError(kViECodecInvalidArgument);
    return -1;
  }

  if (vie_encoder->RegisterExternalEncoder(encoder, pl_type, internal_source)
          != 0) {
    shared_data_->SetLastError(kViECodecUnknownError);
    return -1;
  }
  return 0;
}

int ViEExternalCodecImpl::DeRegisterExternalSendCodec(
  const int video_channel, const unsigned char pl_type) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, ViEId(shared_data_->instance_id()),
               "%s channel %d pl_type %d", __FUNCTION__, video_channel,
               pl_type);

  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEEncoder* vie_encoder = cs.Encoder(video_channel);
  if (!vie_encoder) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), video_channel),
                 "%s: Invalid argument video_channel %u. Does it exist?",
                 __FUNCTION__, video_channel);
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
    const unsigned int pl_type,
    VideoDecoder* decoder,
    bool decoder_render,
    int render_delay) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, ViEId(shared_data_->instance_id()),
               "%s channel %d pl_type %d decoder 0x%x, decoder_render %d, "
               "renderDelay %d", __FUNCTION__, video_channel, pl_type, decoder,
               decoder_render, render_delay);

  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), video_channel),
                 "%s: Invalid argument video_channel %u. Does it exist?",
                 __FUNCTION__, video_channel);
    shared_data_->SetLastError(kViECodecInvalidArgument);
    return -1;
  }
  if (!decoder) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), video_channel),
                 "%s: Invalid argument decoder 0x%x.", __FUNCTION__, decoder);
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
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, ViEId(shared_data_->instance_id()),
               "%s channel %d pl_type %u", __FUNCTION__, video_channel,
               pl_type);

  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), video_channel),
                 "%s: Invalid argument video_channel %u. Does it exist?",
                 __FUNCTION__, video_channel);
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
