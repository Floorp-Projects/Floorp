/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video_engine/vie_image_process_impl.h"

#include "system_wrappers/interface/trace.h"
#include "video_engine/include/vie_errors.h"
#include "video_engine/vie_capturer.h"
#include "video_engine/vie_channel.h"
#include "video_engine/vie_channel_manager.h"
#include "video_engine/vie_defines.h"
#include "video_engine/vie_encoder.h"
#include "video_engine/vie_impl.h"
#include "video_engine/vie_input_manager.h"
#include "video_engine/vie_shared_data.h"

namespace webrtc {

ViEImageProcess* ViEImageProcess::GetInterface(VideoEngine* video_engine) {
#ifdef WEBRTC_VIDEO_ENGINE_IMAGE_PROCESS_API
  if (!video_engine) {
    return NULL;
  }
  VideoEngineImpl* vie_impl = reinterpret_cast<VideoEngineImpl*>(video_engine);
  ViEImageProcessImpl* vie_image_process_impl = vie_impl;
  // Increase ref count.
  (*vie_image_process_impl)++;
  return vie_image_process_impl;
#else
  return NULL;
#endif
}

int ViEImageProcessImpl::Release() {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, shared_data_->instance_id(),
               "ViEImageProcess::Release()");
  // Decrease ref count.
  (*this)--;

  int32_t ref_count = GetCount();
  if (ref_count < 0) {
    WEBRTC_TRACE(kTraceWarning, kTraceVideo, shared_data_->instance_id(),
                 "ViEImageProcess release too many times");
    shared_data_->SetLastError(kViEAPIDoesNotExist);
    return -1;
  }
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, shared_data_->instance_id(),
               "ViEImageProcess reference count: %d", ref_count);
  return ref_count;
}

ViEImageProcessImpl::ViEImageProcessImpl(ViESharedData* shared_data)
    : shared_data_(shared_data) {
  WEBRTC_TRACE(kTraceMemory, kTraceVideo, shared_data_->instance_id(),
               "ViEImageProcessImpl::ViEImageProcessImpl() Ctor");
}

ViEImageProcessImpl::~ViEImageProcessImpl() {
  WEBRTC_TRACE(kTraceMemory, kTraceVideo, shared_data_->instance_id(),
               "ViEImageProcessImpl::~ViEImageProcessImpl() Dtor");
}

int ViEImageProcessImpl::RegisterCaptureEffectFilter(
  const int capture_id,
  ViEEffectFilter& capture_filter) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, ViEId(shared_data_->instance_id()),
               "%s(capture_id: %d)", __FUNCTION__, capture_id);
  if (!shared_data_->Initialized()) {
    shared_data_->SetLastError(kViENotInitialized);
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(shared_data_->instance_id()),
                 "%s - ViE instance %d not initialized", __FUNCTION__,
                 shared_data_->instance_id());
    return -1;
  }

  ViEInputManagerScoped is(*(shared_data_->input_manager()));
  ViECapturer* vie_capture = is.Capture(capture_id);
  if (!vie_capture) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(shared_data_->instance_id()),
                 "%s: Capture device %d doesn't exist", __FUNCTION__,
                 capture_id);
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
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, ViEId(shared_data_->instance_id()),
               "%s(capture_id: %d)", __FUNCTION__, capture_id);

  ViEInputManagerScoped is(*(shared_data_->input_manager()));
  ViECapturer* vie_capture = is.Capture(capture_id);
  if (!vie_capture) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(shared_data_->instance_id()),
                 "%s: Capture device %d doesn't exist", __FUNCTION__,
                 capture_id);
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
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, ViEId(shared_data_->instance_id()),
               "%s(video_channel: %d)", __FUNCTION__, video_channel);

  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEEncoder* vie_encoder = cs.Encoder(video_channel);
  if (vie_encoder == NULL) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(shared_data_->instance_id()),
                 "%s: Channel %d doesn't exist", __FUNCTION__, video_channel);
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
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, ViEId(shared_data_->instance_id()),
               "%s(video_channel: %d)", __FUNCTION__, video_channel);

  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEEncoder* vie_encoder = cs.Encoder(video_channel);
  if (vie_encoder == NULL) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(shared_data_->instance_id()),
                 "%s: Channel %d doesn't exist", __FUNCTION__, video_channel);
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
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, ViEId(shared_data_->instance_id()),
               "%s(video_channel: %d)", __FUNCTION__, video_channel);

  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(shared_data_->instance_id()),
                 "%s: Channel %d doesn't exist", __FUNCTION__, video_channel);
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
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, ViEId(shared_data_->instance_id()),
               "%s(video_channel: %d)", __FUNCTION__, video_channel);

  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(shared_data_->instance_id()),
                 "%s: Channel %d doesn't exist", __FUNCTION__, video_channel);
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
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, ViEId(shared_data_->instance_id()),
               "%s(capture_id: %d, enable: %d)", __FUNCTION__, capture_id,
               enable);

  ViEInputManagerScoped is(*(shared_data_->input_manager()));
  ViECapturer* vie_capture = is.Capture(capture_id);
  if (!vie_capture) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(shared_data_->instance_id()),
                 "%s: Capture device %d doesn't exist", __FUNCTION__,
                 capture_id);
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

int ViEImageProcessImpl::EnableDenoising(const int capture_id,
                                         const bool enable) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, ViEId(shared_data_->instance_id()),
               "%s(capture_id: %d, enable: %d)", __FUNCTION__, capture_id,
               enable);

  ViEInputManagerScoped is(*(shared_data_->input_manager()));
  ViECapturer* vie_capture = is.Capture(capture_id);
  if (!vie_capture) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(shared_data_->instance_id()),
                 "%s: Capture device %d doesn't exist", __FUNCTION__,
                 capture_id);
    shared_data_->SetLastError(kViEImageProcessInvalidCaptureId);
    return -1;
  }

  if (vie_capture->EnableDenoising(enable) != 0) {
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
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, ViEId(shared_data_->instance_id()),
               "%s(video_channel: %d, enable: %d)", __FUNCTION__, video_channel,
               enable);

  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(shared_data_->instance_id()),
                 "%s: Channel %d doesn't exist", __FUNCTION__, video_channel);
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

}  // namespace webrtc
