/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_ENGINE_VIE_INPUT_MANAGER_H_
#define WEBRTC_VIDEO_ENGINE_VIE_INPUT_MANAGER_H_

#include "modules/video_capture/main/interface/video_capture.h"
#include "system_wrappers/interface/map_wrapper.h"
#include "system_wrappers/interface/scoped_ptr.h"
#include "typedefs.h"  // NOLINT
#include "video_engine/include/vie_capture.h"
#include "video_engine/vie_defines.h"
#include "video_engine/vie_frame_provider_base.h"
#include "video_engine/vie_manager_base.h"

namespace webrtc {

class CriticalSectionWrapper;
class ProcessThread;
class RWLockWrapper;
class ViECapturer;
class ViEExternalCapture;
class ViEFilePlayer;
class VoiceEngine;

class ViEInputManager : private ViEManagerBase {
  friend class ViEInputManagerScoped;
 public:
  explicit ViEInputManager(int engine_id);
  ~ViEInputManager();

  void SetModuleProcessThread(ProcessThread* module_process_thread);

  // Returns number of capture devices.
  int NumberOfCaptureDevices();

  // Gets name and id for a capture device.
  int GetDeviceName(WebRtc_UWord32 device_number,
                    char* device_nameUTF8,
                    WebRtc_UWord32 device_name_length,
                    char* device_unique_idUTF8,
                    WebRtc_UWord32 device_unique_idUTF8Length);

  // Returns the number of capture capabilities for a specified device.
  int NumberOfCaptureCapabilities(const char* device_unique_idUTF8);

  // Gets a specific capability for a capture device.
  int GetCaptureCapability(const char* device_unique_idUTF8,
                           const WebRtc_UWord32 device_capability_number,
                           CaptureCapability& capability);

  // Show OS specific Capture settings.
  int DisplayCaptureSettingsDialogBox(const char* device_unique_idUTF8,
                                      const char* dialog_titleUTF8,
                                      void* parent_window,
                                      WebRtc_UWord32 positionX,
                                      WebRtc_UWord32 positionY);
  int GetOrientation(const char* device_unique_idUTF8,
                     RotateCapturedFrame& orientation);

  // Creates a capture module for the specified capture device and assigns
  // a capture device id for the device.
  // Return zero on success, ViEError on failure.
  int CreateCaptureDevice(const char* device_unique_idUTF8,
                          const WebRtc_UWord32 device_unique_idUTF8Length,
                          int& capture_id);
  int CreateCaptureDevice(VideoCaptureModule* capture_module,
                          int& capture_id);
  int CreateExternalCaptureDevice(ViEExternalCapture*& external_capture,
                                  int& capture_id);
  int DestroyCaptureDevice(int capture_id);

  int CreateFilePlayer(const char* file_nameUTF8, const bool loop,
                       const FileFormats file_format,
                       VoiceEngine* voe_ptr,
                       int& file_id);
  int DestroyFilePlayer(int file_id);

 private:
  // Gets and allocates a free capture device id. Assumed protected by caller.
  bool GetFreeCaptureId(int* freecapture_id);

  // Frees a capture id assigned in GetFreeCaptureId.
  void ReturnCaptureId(int capture_id);

  // Gets and allocates a free file id. Assumed protected by caller.
  bool GetFreeFileId(int* free_file_id);

  // Frees a file id assigned in GetFreeFileId.
  void ReturnFileId(int file_id);

  // Gets the ViEFrameProvider for this capture observer.
  ViEFrameProviderBase* ViEFrameProvider(
      const ViEFrameCallback* capture_observer) const;

  // Gets the ViEFrameProvider for this capture observer.
  ViEFrameProviderBase* ViEFrameProvider(int provider_id) const;

  // Gets the ViECapturer for the capture device id.
  ViECapturer* ViECapturePtr(int capture_id) const;

  // Gets the ViEFilePlayer for this file_id.
  ViEFilePlayer* ViEFilePlayerPtr(int file_id) const;

  int engine_id_;
  scoped_ptr<CriticalSectionWrapper> map_cs_;
  MapWrapper vie_frame_provider_map_;

  // Capture devices.
  VideoCaptureModule::DeviceInfo* capture_device_info_;
  int free_capture_device_id_[kViEMaxCaptureDevices];

  // File Players.
  int free_file_id_[kViEMaxFilePlayers];

  ProcessThread* module_process_thread_;  // Weak.
};

// Provides protected access to ViEInputManater.
class ViEInputManagerScoped: private ViEManagerScopedBase {
 public:
  explicit ViEInputManagerScoped(const ViEInputManager& vie_input_manager);

  ViECapturer* Capture(int capture_id) const;
  ViEFilePlayer* FilePlayer(int file_id) const;
  ViEFrameProviderBase* FrameProvider(int provider_id) const;
  ViEFrameProviderBase* FrameProvider(const ViEFrameCallback*
                                      capture_observer) const;
};

}  // namespace webrtc

#endif  // WEBRTC_VIDEO_ENGINE_VIE_INPUT_MANAGER_H_
