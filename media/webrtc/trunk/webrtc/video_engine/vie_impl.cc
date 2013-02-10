/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video_engine/vie_impl.h"

#if (defined(WIN32_) || defined(WIN64_))
#include <Windows.h>  // For LoadLibrary.
#include <tchar.h>    // For T_.
#endif

#include "system_wrappers/interface/trace.h"

#ifdef WEBRTC_ANDROID
#include "webrtc/modules/video_capture/include/video_capture_factory.h"
#include "webrtc/modules/video_render/include/video_render.h"
#endif

// Global counter to get an id for each new ViE instance.
static WebRtc_Word32 g_vie_active_instance_counter = 0;

namespace webrtc {

// extern "C" ensures that GetProcAddress() can find the function address.
extern "C" {
  VideoEngine* GetVideoEngine() {
    VideoEngineImpl* self = new VideoEngineImpl();
    if (!self) {
      return NULL;
    }
    g_vie_active_instance_counter++;
    VideoEngine* vie = reinterpret_cast<VideoEngine*>(self);
    return vie;
  }
}

VideoEngine* VideoEngine::Create() {
#if (defined(WIN32_) || defined(WIN64_))
  // Load a debug dll, if there is one.
  HMODULE hmod_ = LoadLibrary(TEXT("VideoEngineTestingDLL.dll"));
  if (hmod_) {
    typedef VideoEngine* (*PFNGetVideoEngineLib)(void);
    PFNGetVideoEngineLib pfn =
      (PFNGetVideoEngineLib)GetProcAddress(hmod_, "GetVideoEngine");
    if (pfn) {
      VideoEngine* self = pfn();
      return self;
    } else {
      assert(false && "Failed to open test dll VideoEngineTestingDLL.dll");
      return NULL;
    }
  }
#endif

  return GetVideoEngine();
}

bool VideoEngine::Delete(VideoEngine*& video_engine) {
  if (!video_engine) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, g_vie_active_instance_counter,
                 "VideoEngine::Delete - No argument");
    return false;
  }
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, g_vie_active_instance_counter,
               "VideoEngine::Delete(vie = 0x%p)", video_engine);
  VideoEngineImpl* vie_impl = reinterpret_cast<VideoEngineImpl*>(video_engine);

  // Check all reference counters.
  ViEBaseImpl* vie_base = vie_impl;
  if (vie_base->GetCount() > 0) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, g_vie_active_instance_counter,
                 "ViEBase ref count: %d", vie_base->GetCount());
    return false;
  }
#ifdef WEBRTC_VIDEO_ENGINE_CAPTURE_API
  ViECaptureImpl* vie_capture = vie_impl;
  if (vie_capture->GetCount() > 0) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, g_vie_active_instance_counter,
                 "ViECapture ref count: %d", vie_capture->GetCount());
    return false;
  }
#endif
#ifdef WEBRTC_VIDEO_ENGINE_CODEC_API
  ViECodecImpl* vie_codec = vie_impl;
  if (vie_codec->GetCount() > 0) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, g_vie_active_instance_counter,
                 "ViECodec ref count: %d", vie_codec->GetCount());
    return false;
  }
#endif
#ifdef WEBRTC_VIDEO_ENGINE_ENCRYPTION_API
  ViEEncryptionImpl* vie_encryption = vie_impl;
  if (vie_encryption->GetCount() > 0) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, g_vie_active_instance_counter,
                 "ViEEncryption ref count: %d", vie_encryption->GetCount());
    return false;
  }
#endif
#ifdef WEBRTC_VIDEO_ENGINE_EXTERNAL_CODEC_API
  ViEExternalCodecImpl* vie_external_codec = vie_impl;
  if (vie_external_codec->GetCount() > 0) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, g_vie_active_instance_counter,
                 "ViEEncryption ref count: %d", vie_encryption->GetCount());
    return false;
  }
#endif
#ifdef WEBRTC_VIDEO_ENGINE_FILE_API
  ViEFileImpl* vie_file = vie_impl;
  if (vie_file->GetCount() > 0) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, g_vie_active_instance_counter,
                 "ViEFile ref count: %d", vie_file->GetCount());
    return false;
  }
#endif
#ifdef WEBRTC_VIDEO_ENGINE_IMAGE_PROCESS_API
  ViEImageProcessImpl* vie_image_process = vie_impl;
  if (vie_image_process->GetCount() > 0) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, g_vie_active_instance_counter,
                 "ViEImageProcess ref count: %d",
                 vie_image_process->GetCount());
    return false;
  }
#endif
#ifdef WEBRTC_VIDEO_ENGINE_NETWORK_API
  ViENetworkImpl* vie_network = vie_impl;
  if (vie_network->GetCount() > 0) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, g_vie_active_instance_counter,
                 "ViENetwork ref count: %d", vie_network->GetCount());
    return false;
  }
#endif
#ifdef WEBRTC_VIDEO_ENGINE_RENDER_API
  ViERenderImpl* vie_render = vie_impl;
  if (vie_render->GetCount() > 0) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, g_vie_active_instance_counter,
                 "ViERender ref count: %d", vie_render->GetCount());
    return false;
  }
#endif
#ifdef WEBRTC_VIDEO_ENGINE_RTP_RTCP_API
  ViERTP_RTCPImpl* vie_rtp_rtcp = vie_impl;
  if (vie_rtp_rtcp->GetCount() > 0) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, g_vie_active_instance_counter,
                 "ViERTP_RTCP ref count: %d", vie_rtp_rtcp->GetCount());
    return false;
  }
#endif

  delete vie_impl;
  vie_impl = NULL;
  video_engine = NULL;

  // Decrease the number of instances.
  g_vie_active_instance_counter--;

  WEBRTC_TRACE(kTraceInfo, kTraceVideo, g_vie_active_instance_counter,
               "%s: instance deleted. Remaining instances: %d", __FUNCTION__,
               g_vie_active_instance_counter);
  return true;
}

int VideoEngine::SetTraceFile(const char* file_nameUTF8,
                              const bool add_file_counter) {
  if (!file_nameUTF8) {
    return -1;
  }
  if (Trace::SetTraceFile(file_nameUTF8, add_file_counter) == -1) {
    return -1;
  }
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, g_vie_active_instance_counter,
               "SetTraceFileName(file_nameUTF8 = %s, add_file_counter = %d",
               file_nameUTF8, add_file_counter);
  return 0;
}

int VideoEngine::SetTraceFilter(const unsigned int filter) {
  WebRtc_UWord32 old_filter = 0;
  Trace::LevelFilter(old_filter);

  if (filter == kTraceNone && old_filter != kTraceNone) {
    // Do the logging before turning it off.
    WEBRTC_TRACE(kTraceApiCall, kTraceVideo, g_vie_active_instance_counter,
                 "SetTraceFilter(filter = 0x%x)", filter);
  }

  WebRtc_Word32 error = Trace::SetLevelFilter(filter);
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, g_vie_active_instance_counter,
               "SetTraceFilter(filter = 0x%x)", filter);
  if (error != 0) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, g_vie_active_instance_counter,
                 "SetTraceFilter error: %d", error);
    return -1;
  }
  return 0;
}

int VideoEngine::SetTraceCallback(TraceCallback* callback) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, g_vie_active_instance_counter,
               "SetTraceCallback(TraceCallback = 0x%p)", callback);
  return Trace::SetTraceCallback(callback);
}

int VideoEngine::SetAndroidObjects(void* javaVM, void* javaContext) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, g_vie_active_instance_counter,
               "SetAndroidObjects()");

#if defined(WEBRTC_ANDROID) && !defined(WEBRTC_CHROMIUM_BUILD)
  if (SetCaptureAndroidVM(javaVM, javaContext) != 0) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, g_vie_active_instance_counter,
                 "Could not set capture Android VM");
    return -1;
  }
#ifdef WEBRTC_INCLUDE_INTERNAL_VIDEO_RENDER
  if (SetRenderAndroidVM(javaVM) != 0) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, g_vie_active_instance_counter,
                 "Could not set render Android VM");
    return -1;
  }
#endif
  return 0;
#else
  WEBRTC_TRACE(kTraceError, kTraceVideo, g_vie_active_instance_counter,
               "WEBRTC_ANDROID not defined for VideoEngine::SetAndroidObjects");
  return -1;
#endif
}

}  // namespace webrtc
