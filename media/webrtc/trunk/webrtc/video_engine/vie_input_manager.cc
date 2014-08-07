/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/video_engine/vie_input_manager.h"

#include <assert.h>

#include "webrtc/common_types.h"
#include "webrtc/modules/video_capture/include/video_capture_factory.h"
#include "webrtc/modules/video_coding/main/interface/video_coding.h"
#include "webrtc/modules/video_coding/main/interface/video_coding_defines.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/rw_lock_wrapper.h"
#include "webrtc/system_wrappers/interface/trace.h"
#include "webrtc/video_engine/include/vie_errors.h"
#include "webrtc/video_engine/vie_capturer.h"
#include "webrtc/video_engine/vie_defines.h"
#include "webrtc/video_engine/desktop_capture_impl.h"
#include "webrtc/video_engine/browser_capture_impl.h"

namespace webrtc {

ViEInputManager::ViEInputManager(const int engine_id, const Config& config)
    : config_(config),
      engine_id_(engine_id),
      map_cs_(CriticalSectionWrapper::CreateCriticalSection()),
      device_info_cs_(CriticalSectionWrapper::CreateCriticalSection()),
      vie_frame_provider_map_(),
      capture_device_info_(NULL),
      module_process_thread_(NULL) {
  WEBRTC_TRACE(webrtc::kTraceMemory, webrtc::kTraceVideo, ViEId(engine_id_),
               "%s", __FUNCTION__);

  for (int idx = 0; idx < kViEMaxCaptureDevices; idx++) {
    free_capture_device_id_[idx] = true;
  }
}

ViEInputManager::~ViEInputManager() {
  WEBRTC_TRACE(webrtc::kTraceMemory, webrtc::kTraceVideo, ViEId(engine_id_),
               "%s", __FUNCTION__);
  for (FrameProviderMap::iterator it = vie_frame_provider_map_.begin();
       it != vie_frame_provider_map_.end();
       ++it) {
    delete it->second;
  }

  delete capture_device_info_;
}
void ViEInputManager::SetModuleProcessThread(
    ProcessThread* module_process_thread) {
  assert(!module_process_thread_);
  module_process_thread_ = module_process_thread;
}

int ViEInputManager::NumberOfCaptureDevices() {
  WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(engine_id_), "%s",
               __FUNCTION__);
  CriticalSectionScoped cs(device_info_cs_.get());
  GetDeviceInfo();
  assert(capture_device_info_);
  capture_device_info_->Refresh();
  return capture_device_info_->NumberOfDevices();
}

int ViEInputManager::GetDeviceName(uint32_t device_number,
                                   char* device_nameUTF8,
                                   uint32_t device_name_length,
                                   char* device_unique_idUTF8,
                                   uint32_t device_unique_idUTF8Length) {
  WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(engine_id_),
               "%s(device_number: %d)", __FUNCTION__, device_number);
  CriticalSectionScoped cs(device_info_cs_.get());
  GetDeviceInfo();
  assert(capture_device_info_);
  return capture_device_info_->GetDeviceName(device_number, device_nameUTF8,
                                             device_name_length,
                                             device_unique_idUTF8,
                                             device_unique_idUTF8Length);
}

int ViEInputManager::NumberOfCaptureCapabilities(
  const char* device_unique_idUTF8) {
  WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(engine_id_), "%s",
               __FUNCTION__);
  CriticalSectionScoped cs(device_info_cs_.get());
  GetDeviceInfo();
  assert(capture_device_info_);
  return capture_device_info_->NumberOfCapabilities(device_unique_idUTF8);
}

int ViEInputManager::GetCaptureCapability(
    const char* device_unique_idUTF8,
    const uint32_t device_capability_number,
    CaptureCapability& capability) {
  WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(engine_id_),
               "%s(device_unique_idUTF8: %s, device_capability_number: %d)",
               __FUNCTION__, device_unique_idUTF8, device_capability_number);
  CriticalSectionScoped cs(device_info_cs_.get());
  GetDeviceInfo();
  assert(capture_device_info_);
  VideoCaptureCapability module_capability;
  int result = capture_device_info_->GetCapability(device_unique_idUTF8,
                                                   device_capability_number,
                                                   module_capability);
  if (result != 0)
    return result;

  // Copy from module type to public type.
  capability.expectedCaptureDelay = module_capability.expectedCaptureDelay;
  capability.height = module_capability.height;
  capability.width = module_capability.width;
  capability.interlaced = module_capability.interlaced;
  capability.rawType = module_capability.rawType;
  capability.codecType = module_capability.codecType;
  capability.maxFPS = module_capability.maxFPS;
  return result;
}

int ViEInputManager::GetOrientation(const char* device_unique_idUTF8,
                                    RotateCapturedFrame& orientation) {
  WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(engine_id_),
               "%s(device_unique_idUTF8: %s,)", __FUNCTION__,
               device_unique_idUTF8);
  CriticalSectionScoped cs(device_info_cs_.get());
  GetDeviceInfo();
  assert(capture_device_info_);
  VideoCaptureRotation module_orientation;
  int result = capture_device_info_->GetOrientation(device_unique_idUTF8,
                                                    module_orientation);
  // Copy from module type to public type.
  switch (module_orientation) {
    case kCameraRotate0:
      orientation = RotateCapturedFrame_0;
      break;
    case kCameraRotate90:
      orientation = RotateCapturedFrame_90;
      break;
    case kCameraRotate180:
      orientation = RotateCapturedFrame_180;
      break;
    case kCameraRotate270:
      orientation = RotateCapturedFrame_270;
      break;
  }
  return result;
}

int ViEInputManager::DisplayCaptureSettingsDialogBox(
    const char* device_unique_idUTF8,
    const char* dialog_titleUTF8,
    void* parent_window,
    uint32_t positionX,
    uint32_t positionY) {
  CriticalSectionScoped cs(device_info_cs_.get());
  GetDeviceInfo();
  assert(capture_device_info_);
  return capture_device_info_->DisplayCaptureSettingsDialogBox(
           device_unique_idUTF8, dialog_titleUTF8, parent_window, positionX,
           positionY);
}

int ViEInputManager::CreateCaptureDevice(
    const char* device_unique_idUTF8,
    const uint32_t device_unique_idUTF8Length,
    int& capture_id) {
  WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(engine_id_),
               "%s(device_unique_id: %s)", __FUNCTION__, device_unique_idUTF8);
  CriticalSectionScoped cs(map_cs_.get());

  // Make sure the device is not already allocated.
  for (FrameProviderMap::iterator it = vie_frame_provider_map_.begin();
       it != vie_frame_provider_map_.end();
       ++it) {
    // Make sure this is a capture device.
    if (it->first >= kViECaptureIdBase && it->first <= kViECaptureIdMax) {
      ViECapturer* vie_capture = static_cast<ViECapturer*>(it->second);
      assert(vie_capture);
      // TODO(mflodman) Can we change input to avoid this cast?
      const char* device_name =
          reinterpret_cast<const char*>(vie_capture->CurrentDeviceName());
      if (strncmp(device_name, device_unique_idUTF8,
                  strlen(device_name)) == 0) {
        return kViECaptureDeviceAlreadyAllocated;
      }
    }
  }

  // Make sure the device name is valid.
  bool found_device = false;
  CriticalSectionScoped cs_devinfo(device_info_cs_.get());
  GetDeviceInfo();
  assert(capture_device_info_);
  for (uint32_t device_index = 0;
       device_index < capture_device_info_->NumberOfDevices(); ++device_index) {
    if (device_unique_idUTF8Length > kVideoCaptureUniqueNameLength) {
      // User's string length is longer than the max.
      return -1;
    }

    char found_name[kVideoCaptureDeviceNameLength] = "";
    char found_unique_name[kVideoCaptureUniqueNameLength] = "";
    capture_device_info_->GetDeviceName(device_index, found_name,
                                        kVideoCaptureDeviceNameLength,
                                        found_unique_name,
                                        kVideoCaptureUniqueNameLength);

    // TODO(mflodman) Can we change input to avoid this cast?
    const char* cast_id = reinterpret_cast<const char*>(device_unique_idUTF8);
    if (strncmp(cast_id, reinterpret_cast<const char*>(found_unique_name),
                strlen(cast_id)) == 0) {
      WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideo, ViEId(engine_id_),
                   "%s:%d Capture device was found by unique ID: %s. Returning",
                   __FUNCTION__, __LINE__, device_unique_idUTF8);
      found_device = true;
      break;
    }
  }
  if (!found_device) {
    WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideo, ViEId(engine_id_),
                 "%s:%d Capture device NOT found by unique ID: %s. Returning",
                 __FUNCTION__, __LINE__, device_unique_idUTF8);
    return kViECaptureDeviceDoesNotExist;
  }

  int newcapture_id = 0;
  if (GetFreeCaptureId(&newcapture_id) == false) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(engine_id_),
                 "%s: Maximum supported number of capture devices already in "
                 "use", __FUNCTION__);
    return kViECaptureDeviceMaxNoDevicesAllocated;
  }
  ViECapturer* vie_capture = ViECapturer::CreateViECapture(
      newcapture_id, engine_id_, config_, device_unique_idUTF8,
      device_unique_idUTF8Length, *module_process_thread_);
  if (!vie_capture) {
  ReturnCaptureId(newcapture_id);
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(engine_id_),
                 "%s: Could not create capture module for %s", __FUNCTION__,
                 device_unique_idUTF8);
    return kViECaptureDeviceUnknownError;
  }

  vie_frame_provider_map_[newcapture_id] = vie_capture;
  capture_id = newcapture_id;
  WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(engine_id_),
               "%s(device_unique_id: %s, capture_id: %d)", __FUNCTION__,
               device_unique_idUTF8, capture_id);
  return 0;
}

int ViEInputManager::CreateCaptureDevice(VideoCaptureModule* capture_module,
                                         int& capture_id) {
  WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(engine_id_), "%s",
               __FUNCTION__);

  CriticalSectionScoped cs(map_cs_.get());
  int newcapture_id = 0;
  if (!GetFreeCaptureId(&newcapture_id)) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(engine_id_),
                 "%s: Maximum supported number of capture devices already in "
                 "use", __FUNCTION__);
    return kViECaptureDeviceMaxNoDevicesAllocated;
  }

  ViECapturer* vie_capture = ViECapturer::CreateViECapture(
      newcapture_id, engine_id_, config_,
      capture_module, *module_process_thread_);
  if (!vie_capture) {
  ReturnCaptureId(newcapture_id);
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(engine_id_),
                 "%s: Could attach capture module.", __FUNCTION__);
    return kViECaptureDeviceUnknownError;
  }
  vie_frame_provider_map_[newcapture_id] = vie_capture;
  capture_id = newcapture_id;
  WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(engine_id_),
               "%s, capture_id: %d", __FUNCTION__, capture_id);
  return 0;
}

int ViEInputManager::DestroyCaptureDevice(const int capture_id) {
  WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(engine_id_),
               "%s(capture_id: %d)", __FUNCTION__, capture_id);
  ViECapturer* vie_capture = NULL;
  {
    // We need exclusive access to the object to delete it.
    // Take this write lock first since the read lock is taken before map_cs_.
    ViEManagerWriteScoped wl(this);
    CriticalSectionScoped cs(map_cs_.get());

    vie_capture = ViECapturePtr(capture_id);
    if (!vie_capture) {
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(engine_id_),
                   "%s(capture_id: %d) - No such capture device id",
                   __FUNCTION__, capture_id);
      return -1;
    }
    uint32_t num_callbacks =
        vie_capture->NumberOfRegisteredFrameCallbacks();
    if (num_callbacks > 0) {
      WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceVideo,
                   ViEId(engine_id_), "%s(capture_id: %d) - %u registered "
                   "callbacks when destroying capture device",
                   __FUNCTION__, capture_id, num_callbacks);
    }
    vie_frame_provider_map_.erase(capture_id);
    ReturnCaptureId(capture_id);
    // Leave cs before deleting the capture object. This is because deleting the
    // object might cause deletions of renderers so we prefer to not have a lock
    // at that time.
  }
  delete vie_capture;
  return 0;
}

int ViEInputManager::CreateExternalCaptureDevice(
    ViEExternalCapture*& external_capture,
    int& capture_id) {
  WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(engine_id_), "%s",
               __FUNCTION__);
  CriticalSectionScoped cs(map_cs_.get());

  int newcapture_id = 0;
  if (GetFreeCaptureId(&newcapture_id) == false) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(engine_id_),
                 "%s: Maximum supported number of capture devices already in "
                 "use", __FUNCTION__);
    return kViECaptureDeviceMaxNoDevicesAllocated;
  }

  ViECapturer* vie_capture = ViECapturer::CreateViECapture(
      newcapture_id, engine_id_, config_, NULL, 0, *module_process_thread_);
  if (!vie_capture) {
    ReturnCaptureId(newcapture_id);
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(engine_id_),
                 "%s: Could not create capture module for external capture.",
                 __FUNCTION__);
    return kViECaptureDeviceUnknownError;
  }

  vie_frame_provider_map_[newcapture_id] = vie_capture;
  capture_id = newcapture_id;
  external_capture = vie_capture;
  WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(engine_id_),
               "%s, capture_id: %d)", __FUNCTION__, capture_id);
  return 0;
}

bool ViEInputManager::GetFreeCaptureId(int* freecapture_id) {
  WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(engine_id_), "%s",
               __FUNCTION__);
  for (int id = 0; id < kViEMaxCaptureDevices; id++) {
    if (free_capture_device_id_[id]) {
      // We found a free capture device id.
      free_capture_device_id_[id] = false;
      *freecapture_id = id + kViECaptureIdBase;
      WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(engine_id_),
                   "%s: new id: %d", __FUNCTION__, *freecapture_id);
      return true;
    }
  }
  return false;
}

void ViEInputManager::ReturnCaptureId(int capture_id) {
  WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(engine_id_),
               "%s(%d)", __FUNCTION__, capture_id);
  CriticalSectionScoped cs(map_cs_.get());
  if (capture_id >= kViECaptureIdBase &&
      capture_id < kViEMaxCaptureDevices + kViECaptureIdBase) {
    free_capture_device_id_[capture_id - kViECaptureIdBase] = true;
  }
  return;
}

ViEFrameProviderBase* ViEInputManager::ViEFrameProvider(
    const ViEFrameCallback* capture_observer) const {
  assert(capture_observer);
  CriticalSectionScoped cs(map_cs_.get());

  for (FrameProviderMap::const_iterator it = vie_frame_provider_map_.begin();
       it != vie_frame_provider_map_.end();
       ++it) {
    if (it->second->IsFrameCallbackRegistered(capture_observer))
      return it->second;
  }

  // No capture device set for this channel.
  return NULL;
}

ViEFrameProviderBase* ViEInputManager::ViEFrameProvider(int provider_id) const {
  CriticalSectionScoped cs(map_cs_.get());

  FrameProviderMap::const_iterator it =
      vie_frame_provider_map_.find(provider_id);
  if (it == vie_frame_provider_map_.end())
    return NULL;
  return it->second;
}

ViECapturer* ViEInputManager::ViECapturePtr(int capture_id) const {
  if (!(capture_id >= kViECaptureIdBase &&
        capture_id <= kViECaptureIdBase + kViEMaxCaptureDevices))
    return NULL;

  return static_cast<ViECapturer*>(ViEFrameProvider(capture_id));
}

// Create different DeviceInfo by _config;
VideoCaptureModule::DeviceInfo* ViEInputManager::GetDeviceInfo() {
  CaptureDeviceType type = config_.Get<CaptureDeviceInfo>().type;

  if (capture_device_info_ == NULL) {
    switch (type) {
      case CaptureDeviceType::Screen:
      case CaptureDeviceType::Application:
      case CaptureDeviceType::Window:
#if !defined(ANDROID)
        capture_device_info_ = DesktopCaptureImpl::CreateDeviceInfo(ViEModuleId(engine_id_),
                                                                    type);
#endif
        break;
      case CaptureDeviceType::Browser:
        capture_device_info_ = BrowserDeviceInfoImpl::CreateDeviceInfo();
        break;
      case CaptureDeviceType::Camera:
        capture_device_info_ = VideoCaptureFactory::CreateDeviceInfo(ViEModuleId(engine_id_));
        break;
      default:
        // Don't try to build anything for unknown/unsupported types
        break;
    }
  }
  return capture_device_info_;
}
ViEInputManagerScoped::ViEInputManagerScoped(
    const ViEInputManager& vie_input_manager)
    : ViEManagerScopedBase(vie_input_manager) {
}

ViECapturer* ViEInputManagerScoped::Capture(int capture_id) const {
  return static_cast<const ViEInputManager*>(vie_manager_)->ViECapturePtr(
      capture_id);
}

ViEFrameProviderBase* ViEInputManagerScoped::FrameProvider(
    const ViEFrameCallback* capture_observer) const {
  return static_cast<const ViEInputManager*>(vie_manager_)->ViEFrameProvider(
      capture_observer);
}

ViEFrameProviderBase* ViEInputManagerScoped::FrameProvider(
    int provider_id) const {
  return static_cast<const ViEInputManager*>(vie_manager_)->ViEFrameProvider(
      provider_id);
}

}  // namespace webrtc
