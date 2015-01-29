/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/video_engine/vie_image_process_impl.h"

#include "webrtc/system_wrappers/interface/logging.h"
#include "webrtc/video_engine/include/vie_errors.h"
#include "webrtc/video_engine/vie_capturer.h"
#include "webrtc/video_engine/vie_channel.h"
#include "webrtc/video_engine/vie_channel_manager.h"
#include "webrtc/video_engine/vie_defines.h"
#include "webrtc/video_engine/vie_encoder.h"
#include "webrtc/video_engine/vie_impl.h"
#include "webrtc/video_engine/vie_input_manager.h"
#include "webrtc/video_engine/vie_shared_data.h"

namespace webrtc {

ViEImageProcess* ViEImageProcess::GetInterface(VideoEngine* video_engine) {
#ifdef WEBRTC_VIDEO_ENGINE_IMAGE_PROCESS_API
  if (!video_engine) {
    return NULL;
  }
  VideoEngineImpl* vie_impl = static_cast<VideoEngineImpl*>(video_engine);
  ViEImageProcessImpl* vie_image_process_impl = vie_impl;
  // Increase ref count.
  (*vie_image_process_impl)++;
  return vie_image_process_impl;
#else
  return NULL;
#endif
}

int ViEImageProcessImpl::Release() {
  // Decrease ref count.
  (*this)--;

  int32_t ref_count = GetCount();
  if (ref_count < 0) {
    LOG(LS_ERROR) << "ViEImageProcess release too many times";
    shared_data_->SetLastError(kViEAPIDoesNotExist);
    return -1;
  }
  return ref_count;
}

ViEImageProcessImpl::ViEImageProcessImpl(ViESharedData* shared_data)
    : shared_data_(shared_data) {}

ViEImageProcessImpl::~ViEImageProcessImpl() {}

int ViEImageProcessImpl::RegisterCaptureEffectFilter(
  const int capture_id,
  ViEEffectFilter& capture_filter) {
  LOG_F(LS_INFO) << "capture_id: " << capture_id;
  ViEInputManagerScoped is(*(shared_data_->input_manager()));
  ViECapturer* vie_capture = is.Capture(capture_id);
  if (!vie_capture) {
    shared_data_->SetLastError(kViEImageProcessInvalidCaptureId);
    return -1;
  }
  if (vie_capture->RegisterEffectFilter(&capture_filter) != 0) {
    shared_data_->SetLastError(kViEImageProcessFilterExists);
    return -1;
  }
  return 0;
}

int ViEImageProcessImpl::DeregisterCaptureEffectFilter(const int capture_id) {
  LOG_F(LS_INFO) << "capture_id: " << capture_id;

  ViEInputManagerScoped is(*(shared_data_->input_manager()));
  ViECapturer* vie_capture = is.Capture(capture_id);
  if (!vie_capture) {
    shared_data_->SetLastError(kViEImageProcessInvalidCaptureId);
    return -1;
  }
  if (vie_capture->RegisterEffectFilter(NULL) != 0) {
    shared_data_->SetLastError(kViEImageProcessFilterDoesNotExist);
    return -1;
  }
  return 0;
}

int ViEImageProcessImpl::RegisterSendEffectFilter(
    const int video_channel,
    ViEEffectFilter& send_filter) {
  LOG_F(LS_INFO) << "video_channel: " << video_channel;

  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEEncoder* vie_encoder = cs.Encoder(video_channel);
  if (vie_encoder == NULL) {
    shared_data_->SetLastError(kViEImageProcessInvalidChannelId);
    return -1;
  }

  if (vie_encoder->RegisterEffectFilter(&send_filter) != 0) {
    shared_data_->SetLastError(kViEImageProcessFilterExists);
    return -1;
  }
  return 0;
}

int ViEImageProcessImpl::DeregisterSendEffectFilter(const int video_channel) {
  LOG_F(LS_INFO) << "video_channel: " << video_channel;

  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEEncoder* vie_encoder = cs.Encoder(video_channel);
  if (vie_encoder == NULL) {
    shared_data_->SetLastError(kViEImageProcessInvalidChannelId);
    return -1;
  }
  if (vie_encoder->RegisterEffectFilter(NULL) != 0) {
    shared_data_->SetLastError(kViEImageProcessFilterDoesNotExist);
    return -1;
  }
  return 0;
}

int ViEImageProcessImpl::RegisterRenderEffectFilter(
  const int video_channel,
  ViEEffectFilter& render_filter) {
  LOG_F(LS_INFO) << "video_channel: " << video_channel;

  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    shared_data_->SetLastError(kViEImageProcessInvalidChannelId);
    return -1;
  }
  if (vie_channel->RegisterEffectFilter(&render_filter) != 0) {
    shared_data_->SetLastError(kViEImageProcessFilterExists);
    return -1;
  }
  return 0;
}

int ViEImageProcessImpl::DeregisterRenderEffectFilter(const int video_channel) {
  LOG_F(LS_INFO) << "video_channel: " << video_channel;

  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    shared_data_->SetLastError(kViEImageProcessInvalidChannelId);
    return -1;
  }

  if (vie_channel->RegisterEffectFilter(NULL) != 0) {
    shared_data_->SetLastError(kViEImageProcessFilterDoesNotExist);
    return -1;
  }
  return 0;
}

int ViEImageProcessImpl::EnableDeflickering(const int capture_id,
                                            const bool enable) {
  LOG_F(LS_INFO) << "capture_id: " << capture_id
                 << " enable: " << (enable ? "on" : "off");

  ViEInputManagerScoped is(*(shared_data_->input_manager()));
  ViECapturer* vie_capture = is.Capture(capture_id);
  if (!vie_capture) {
    shared_data_->SetLastError(kViEImageProcessInvalidChannelId);
    return -1;
  }

  if (vie_capture->EnableDeflickering(enable) != 0) {
    if (enable) {
      shared_data_->SetLastError(kViEImageProcessAlreadyEnabled);
    } else {
      shared_data_->SetLastError(kViEImageProcessAlreadyDisabled);
    }
    return -1;
  }
  return 0;
}

int ViEImageProcessImpl::EnableColorEnhancement(const int video_channel,
                                                const bool enable) {
  LOG_F(LS_INFO) << "video_channel: " << video_channel
                 << " enable: " << (enable ? "on" : "off");

  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    shared_data_->SetLastError(kViEImageProcessInvalidChannelId);
    return -1;
  }
  if (vie_channel->EnableColorEnhancement(enable) != 0) {
    if (enable) {
      shared_data_->SetLastError(kViEImageProcessAlreadyEnabled);
    } else {
      shared_data_->SetLastError(kViEImageProcessAlreadyDisabled);
    }
    return -1;
  }
  return 0;
}

void ViEImageProcessImpl::RegisterPreEncodeCallback(
    int video_channel,
    I420FrameCallback* pre_encode_callback) {
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEEncoder* vie_encoder = cs.Encoder(video_channel);
  vie_encoder->RegisterPreEncodeCallback(pre_encode_callback);
}

void ViEImageProcessImpl::DeRegisterPreEncodeCallback(int video_channel) {
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEEncoder* vie_encoder = cs.Encoder(video_channel);
  assert(vie_encoder != NULL);
  vie_encoder->DeRegisterPreEncodeCallback();
}

void ViEImageProcessImpl::RegisterPostEncodeImageCallback(
    int video_channel,
    EncodedImageCallback* post_encode_callback) {
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEEncoder* vie_encoder = cs.Encoder(video_channel);
  assert(vie_encoder != NULL);
  vie_encoder->RegisterPostEncodeImageCallback(post_encode_callback);
}

void ViEImageProcessImpl::DeRegisterPostEncodeCallback(int video_channel) {
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEEncoder* vie_encoder = cs.Encoder(video_channel);
  assert(vie_encoder != NULL);
  vie_encoder->DeRegisterPostEncodeImageCallback();
}

void ViEImageProcessImpl::RegisterPreDecodeImageCallback(
    int video_channel,
    EncodedImageCallback* pre_decode_callback) {
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* channel = cs.Channel(video_channel);
  channel->RegisterPreDecodeImageCallback(pre_decode_callback);
}

void ViEImageProcessImpl::DeRegisterPreDecodeCallback(int video_channel) {
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* channel = cs.Channel(video_channel);
  channel->RegisterPreDecodeImageCallback(NULL);
}

void ViEImageProcessImpl::RegisterPreRenderCallback(
    int video_channel,
    I420FrameCallback* pre_render_callback) {
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  assert(vie_channel != NULL);
  vie_channel->RegisterPreRenderCallback(pre_render_callback);
}

void ViEImageProcessImpl::DeRegisterPreRenderCallback(int video_channel) {
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  assert(vie_channel != NULL);
  vie_channel->RegisterPreRenderCallback(NULL);
}

}  // namespace webrtc
