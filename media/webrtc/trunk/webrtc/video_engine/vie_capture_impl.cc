/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/video_engine/vie_capture_impl.h"

#include <map>

#include "webrtc/system_wrappers/interface/trace.h"
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

class CpuOveruseObserver;

ViECapture* ViECapture::GetInterface(VideoEngine* video_engine) {
#ifdef WEBRTC_VIDEO_ENGINE_CAPTURE_API
  if (!video_engine) {
    return NULL;
  }
  VideoEngineImpl* vie_impl = static_cast<VideoEngineImpl*>(video_engine);
  ViECaptureImpl* vie_capture_impl = vie_impl;
  // Increase ref count.
  (*vie_capture_impl)++;
  return vie_capture_impl;
#else
  return NULL;
#endif
}

int ViECaptureImpl::Release() {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, shared_data_->instance_id(),
               "ViECapture::Release()");
  // Decrease ref count
  (*this)--;

  int32_t ref_count = GetCount();
  if (ref_count < 0) {
    WEBRTC_TRACE(kTraceWarning, kTraceVideo, shared_data_->instance_id(),
                 "ViECapture release too many times");
    shared_data_->SetLastError(kViEAPIDoesNotExist);
    return -1;
  }
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, shared_data_->instance_id(),
               "ViECapture reference count: %d", ref_count);
  return ref_count;
}

ViECaptureImpl::ViECaptureImpl(ViESharedData* shared_data)
    : shared_data_(shared_data) {
  WEBRTC_TRACE(kTraceMemory, kTraceVideo, shared_data_->instance_id(),
               "ViECaptureImpl::ViECaptureImpl() Ctor");
}

ViECaptureImpl::~ViECaptureImpl() {
  WEBRTC_TRACE(kTraceMemory, kTraceVideo, shared_data_->instance_id(),
               "ViECaptureImpl::~ViECaptureImpl() Dtor");
}

int ViECaptureImpl::NumberOfCaptureDevices() {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, ViEId(shared_data_->instance_id()),
               "%s", __FUNCTION__);
  return  shared_data_->input_manager()->NumberOfCaptureDevices();
}


int ViECaptureImpl::GetCaptureDevice(unsigned int list_number,
                                     char* device_nameUTF8,
                                     unsigned int device_nameUTF8Length,
                                     char* unique_idUTF8,
                                     unsigned int unique_idUTF8Length) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, ViEId(shared_data_->instance_id()),
               "%s(list_number: %d)", __FUNCTION__, list_number);
  return shared_data_->input_manager()->GetDeviceName(
      list_number,
      device_nameUTF8, device_nameUTF8Length,
      unique_idUTF8, unique_idUTF8Length);
}

int ViECaptureImpl::AllocateCaptureDevice(
  const char* unique_idUTF8,
  const unsigned int unique_idUTF8Length,
  int& capture_id) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, ViEId(shared_data_->instance_id()),
               "%s(unique_idUTF8: %s)", __FUNCTION__, unique_idUTF8);
  const int32_t result =
      shared_data_->input_manager()->CreateCaptureDevice(
          unique_idUTF8,
          static_cast<const uint32_t>(unique_idUTF8Length),
          capture_id);
  if (result != 0) {
    shared_data_->SetLastError(result);
    return -1;
  }
  return 0;
}

int ViECaptureImpl::AllocateExternalCaptureDevice(
  int& capture_id, ViEExternalCapture*& external_capture) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, ViEId(shared_data_->instance_id()),
               "%s", __FUNCTION__);
  const int32_t result =
      shared_data_->input_manager()->CreateExternalCaptureDevice(
          external_capture, capture_id);

  if (result != 0) {
    shared_data_->SetLastError(result);
    return -1;
  }
  return 0;
}

int ViECaptureImpl::AllocateCaptureDevice(
    VideoCaptureModule& capture_module, int& capture_id) {  // NOLINT
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, ViEId(shared_data_->instance_id()),
               "%s", __FUNCTION__);
  int32_t result = shared_data_->input_manager()->CreateCaptureDevice(
      &capture_module, capture_id);
  if (result != 0) {
    shared_data_->SetLastError(result);
    return -1;
  }
  return 0;
}


int ViECaptureImpl::ReleaseCaptureDevice(const int capture_id) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, ViEId(shared_data_->instance_id()),
               "%s(capture_id: %d)", __FUNCTION__, capture_id);
  {
    ViEInputManagerScoped is((*(shared_data_->input_manager())));
    ViECapturer* vie_capture = is.Capture(capture_id);
    if (!vie_capture) {
      WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(shared_data_->instance_id()),
                   "%s: Capture device %d doesn't exist", __FUNCTION__,
                   capture_id);
      shared_data_->SetLastError(kViECaptureDeviceDoesNotExist);
      return -1;
    }
  }

  // Destroy the capture device.
  return shared_data_->input_manager()->DestroyCaptureDevice(capture_id);
}

int ViECaptureImpl::ConnectCaptureDevice(const int capture_id,
                                         const int video_channel) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo,
               ViEId(shared_data_->instance_id(), video_channel),
               "%s(capture_id: %d, video_channel: %d)", __FUNCTION__,
               capture_id, video_channel);

  ViEInputManagerScoped is(*(shared_data_->input_manager()));
  ViECapturer* vie_capture = is.Capture(capture_id);
  if (!vie_capture) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), video_channel),
                 "%s: Capture device %d doesn't exist", __FUNCTION__,
                 capture_id);
    shared_data_->SetLastError(kViECaptureDeviceDoesNotExist);
    return -1;
  }

  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEEncoder* vie_encoder = cs.Encoder(video_channel);
  if (!vie_encoder) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), video_channel),
                 "%s: Channel %d doesn't exist", __FUNCTION__,
                 video_channel);
    shared_data_->SetLastError(kViECaptureDeviceInvalidChannelId);
    return -1;
  }
  if (vie_encoder->Owner() != video_channel) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), video_channel),
                 "Can't connect capture device to a receive only channel.");
    shared_data_->SetLastError(kViECaptureDeviceInvalidChannelId);
    return -1;
  }
  //  Check if the encoder already has a connected frame provider
  if (is.FrameProvider(vie_encoder) != NULL) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), video_channel),
                 "%s: Channel %d already connected to a capture device.",
                 __FUNCTION__, video_channel);
    shared_data_->SetLastError(kViECaptureDeviceAlreadyConnected);
    return -1;
  }
  if (vie_capture->RegisterFrameCallback(video_channel, vie_encoder) != 0) {
    shared_data_->SetLastError(kViECaptureDeviceUnknownError);
    return -1;
  }
  std::map<int, CpuOveruseObserver*>::iterator it =
      shared_data_->overuse_observers()->find(video_channel);
  if (it != shared_data_->overuse_observers()->end()) {
    vie_capture->RegisterCpuOveruseObserver(it->second);
  }
  return 0;
}


int ViECaptureImpl::DisconnectCaptureDevice(const int video_channel) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo,
               ViEId(shared_data_->instance_id(), video_channel),
               "%s(video_channel: %d)", __FUNCTION__, video_channel);

  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEEncoder* vie_encoder = cs.Encoder(video_channel);
  if (!vie_encoder) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id()),
                 "%s: Channel %d doesn't exist", __FUNCTION__,
                 video_channel);
    shared_data_->SetLastError(kViECaptureDeviceInvalidChannelId);
    return -1;
  }

  ViEInputManagerScoped is(*(shared_data_->input_manager()));
  ViEFrameProviderBase* frame_provider = is.FrameProvider(vie_encoder);
  if (!frame_provider) {
    WEBRTC_TRACE(kTraceWarning, kTraceVideo,
                 ViEId(shared_data_->instance_id()),
                 "%s: No capture device connected to channel %d",
                 __FUNCTION__, video_channel);
    shared_data_->SetLastError(kViECaptureDeviceNotConnected);
    return -1;
  }
  if (frame_provider->Id() < kViECaptureIdBase ||
      frame_provider->Id() > kViECaptureIdMax) {
    WEBRTC_TRACE(kTraceWarning, kTraceVideo,
                 ViEId(shared_data_->instance_id()),
                 "%s: No capture device connected to channel %d",
                 __FUNCTION__, video_channel);
    shared_data_->SetLastError(kViECaptureDeviceNotConnected);
    return -1;
  }

  ViECapturer* vie_capture = is.Capture(frame_provider->Id());
  assert(vie_capture);
  vie_capture->RegisterCpuOveruseObserver(NULL);
  if (frame_provider->DeregisterFrameCallback(vie_encoder) != 0) {
    shared_data_->SetLastError(kViECaptureDeviceUnknownError);
    return -1;
  }

  return 0;
}

int ViECaptureImpl::StartCapture(const int capture_id,
                                 const CaptureCapability& capture_capability) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, ViEId(shared_data_->instance_id()),
               "%s(capture_id: %d)", __FUNCTION__, capture_id);

  ViEInputManagerScoped is(*(shared_data_->input_manager()));
  ViECapturer* vie_capture = is.Capture(capture_id);
  if (!vie_capture) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), capture_id),
                 "%s: Capture device %d doesn't exist", __FUNCTION__,
                 capture_id);
    shared_data_->SetLastError(kViECaptureDeviceDoesNotExist);
    return -1;
  }
  if (vie_capture->Started()) {
    shared_data_->SetLastError(kViECaptureDeviceAlreadyStarted);
    return -1;
  }
  if (vie_capture->Start(capture_capability) != 0) {
    shared_data_->SetLastError(kViECaptureDeviceUnknownError);
    return -1;
  }
  return 0;
}

int ViECaptureImpl::StopCapture(const int capture_id) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, ViEId(shared_data_->instance_id()),
               "%s(capture_id: %d)", __FUNCTION__, capture_id);

  ViEInputManagerScoped is(*(shared_data_->input_manager()));
  ViECapturer* vie_capture = is.Capture(capture_id);
  if (!vie_capture) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), capture_id),
                 "%s: Capture device %d doesn't exist", __FUNCTION__,
                 capture_id);
    shared_data_->SetLastError(kViECaptureDeviceDoesNotExist);
    return -1;
  }
  if (!vie_capture->Started()) {
    shared_data_->SetLastError(kViECaptureDeviceNotStarted);
    return -1;
  }
  if (vie_capture->Stop() != 0) {
    shared_data_->SetLastError(kViECaptureDeviceUnknownError);
    return -1;
  }

  return 0;
}

int ViECaptureImpl::SetRotateCapturedFrames(
    const int capture_id,
    const RotateCapturedFrame rotation) {
  int i_rotation = -1;
  switch (rotation) {
    case RotateCapturedFrame_0:
      i_rotation = 0;
      break;
    case RotateCapturedFrame_90:
      i_rotation = 90;
      break;
    case RotateCapturedFrame_180:
      i_rotation = 180;
      break;
    case RotateCapturedFrame_270:
      i_rotation = 270;
      break;
  }
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, ViEId(shared_data_->instance_id()),
               "%s(rotation: %d)", __FUNCTION__, i_rotation);

  ViEInputManagerScoped is(*(shared_data_->input_manager()));
  ViECapturer* vie_capture = is.Capture(capture_id);
  if (!vie_capture) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), capture_id),
                 "%s: Capture device %d doesn't exist", __FUNCTION__,
                 capture_id);
    shared_data_->SetLastError(kViECaptureDeviceDoesNotExist);
    return -1;
  }
  if (vie_capture->SetRotateCapturedFrames(rotation) != 0) {
    shared_data_->SetLastError(kViECaptureDeviceUnknownError);
    return -1;
  }
  return 0;
}

int ViECaptureImpl::SetCaptureDelay(const int capture_id,
                                    const unsigned int capture_delay_ms) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, ViEId(shared_data_->instance_id()),
               "%s(capture_id: %d, capture_delay_ms %u)", __FUNCTION__,
               capture_id, capture_delay_ms);

  ViEInputManagerScoped is(*(shared_data_->input_manager()));
  ViECapturer* vie_capture = is.Capture(capture_id);
  if (!vie_capture) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), capture_id),
                 "%s: Capture device %d doesn't exist", __FUNCTION__,
                 capture_id);
    shared_data_->SetLastError(kViECaptureDeviceDoesNotExist);
    return -1;
  }

  if (vie_capture->SetCaptureDelay(capture_delay_ms) != 0) {
    shared_data_->SetLastError(kViECaptureDeviceUnknownError);
    return -1;
  }
  return 0;
}

int ViECaptureImpl::NumberOfCapabilities(
    const char* unique_idUTF8,
    const unsigned int unique_idUTF8Length) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, ViEId(shared_data_->instance_id()),
               "%s(capture_device_name: %s)", __FUNCTION__, unique_idUTF8);

#if defined(WEBRTC_MAC)
  // TODO(mflodman) Move to capture module!
  // QTKit framework handles all capabilities and capture settings
  // automatically (mandatory).
  // Thus this function cannot be supported on the Mac platform.
  shared_data_->SetLastError(kViECaptureDeviceMacQtkitNotSupported);
  WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(shared_data_->instance_id()),
               "%s This API is not supported on Mac OS", __FUNCTION__,
               shared_data_->instance_id());
  return -1;
#endif
  return shared_data_->input_manager()->NumberOfCaptureCapabilities(
      unique_idUTF8);
}


int ViECaptureImpl::GetCaptureCapability(const char* unique_idUTF8,
                                         const unsigned int unique_idUTF8Length,
                                         const unsigned int capability_number,
                                         CaptureCapability& capability) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, ViEId(shared_data_->instance_id()),
               "%s(capture_device_name: %s)", __FUNCTION__, unique_idUTF8);

#if defined(WEBRTC_MAC)
  // TODO(mflodman) Move to capture module!
  // QTKit framework handles all capabilities and capture settings
  // automatically (mandatory).
  // Thus this function cannot be supported on the Mac platform.
  shared_data_->SetLastError(kViECaptureDeviceMacQtkitNotSupported);
  WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(shared_data_->instance_id()),
               "%s This API is not supported on Mac OS", __FUNCTION__,
               shared_data_->instance_id());
  return -1;
#endif
  if (shared_data_->input_manager()->GetCaptureCapability(
          unique_idUTF8, capability_number, capability) != 0) {
    shared_data_->SetLastError(kViECaptureDeviceUnknownError);
    return -1;
  }
  return 0;
}

int ViECaptureImpl::ShowCaptureSettingsDialogBox(
    const char* unique_idUTF8,
    const unsigned int unique_idUTF8Length,
    const char* dialog_title,
    void* parent_window,
    const unsigned int x,
    const unsigned int y) {
#if defined(WEBRTC_MAC)
  // TODO(mflodman) Move to capture module
  // QTKit framework handles all capabilities and capture settings
  // automatically (mandatory).
  // Thus this function cannot be supported on the Mac platform.
  shared_data_->SetLastError(kViECaptureDeviceMacQtkitNotSupported);
  WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(shared_data_->instance_id()),
               "%s This API is not supported on Mac OS", __FUNCTION__,
               shared_data_->instance_id());
  return -1;
#endif
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, ViEId(shared_data_->instance_id()),
               "%s capture_id (capture_device_name: %s)", __FUNCTION__,
               unique_idUTF8);

  return shared_data_->input_manager()->DisplayCaptureSettingsDialogBox(
           unique_idUTF8, dialog_title,
           parent_window, x, y);
}

int ViECaptureImpl::GetOrientation(const char* unique_idUTF8,
                                   RotateCapturedFrame& orientation) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, ViEId(shared_data_->instance_id()),
               "%s (capture_device_name: %s)", __FUNCTION__, unique_idUTF8);
  if (shared_data_->input_manager()->GetOrientation(
      unique_idUTF8,
      orientation) != 0) {
    shared_data_->SetLastError(kViECaptureDeviceUnknownError);
    return -1;
  }
  return 0;
}


int ViECaptureImpl::EnableBrightnessAlarm(const int capture_id,
                                          const bool enable) {
  ViEInputManagerScoped is(*(shared_data_->input_manager()));
  ViECapturer* vie_capture = is.Capture(capture_id);
  if (!vie_capture) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), capture_id),
                 "%s: Capture device %d doesn't exist", __FUNCTION__,
                 capture_id);
    shared_data_->SetLastError(kViECaptureDeviceDoesNotExist);
    return -1;
  }
  if (vie_capture->EnableBrightnessAlarm(enable) != 0) {
    shared_data_->SetLastError(kViECaptureDeviceUnknownError);
    return -1;
  }
  return 0;
}

int ViECaptureImpl::RegisterObserver(const int capture_id,
                                     ViECaptureObserver& observer) {
  ViEInputManagerScoped is(*(shared_data_->input_manager()));
  ViECapturer* vie_capture = is.Capture(capture_id);
  if (!vie_capture) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), capture_id),
                 "%s: Capture device %d doesn't exist", __FUNCTION__,
                 capture_id);
    shared_data_->SetLastError(kViECaptureDeviceDoesNotExist);
    return -1;
  }
  if (vie_capture->IsObserverRegistered()) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), capture_id),
                 "%s: Observer already registered", __FUNCTION__);
    shared_data_->SetLastError(kViECaptureObserverAlreadyRegistered);
    return -1;
  }
  if (vie_capture->RegisterObserver(&observer) != 0) {
    shared_data_->SetLastError(kViECaptureDeviceUnknownError);
    return -1;
  }
  return 0;
}

int ViECaptureImpl::DeregisterObserver(const int capture_id) {
  ViEInputManagerScoped is(*(shared_data_->input_manager()));
  ViECapturer* vie_capture = is.Capture(capture_id);
  if (!vie_capture) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), capture_id),
                 "%s: Capture device %d doesn't exist", __FUNCTION__,
                 capture_id);
    shared_data_->SetLastError(kViECaptureDeviceDoesNotExist);
    return -1;
  }
  if (!vie_capture->IsObserverRegistered()) {
    shared_data_->SetLastError(kViECaptureDeviceObserverNotRegistered);
    return -1;
  }

  if (vie_capture->DeRegisterObserver() != 0) {
    shared_data_->SetLastError(kViECaptureDeviceUnknownError);
    return -1;
  }
  return 0;
}

}  // namespace webrtc
