/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/video_engine/vie_impl.h"

#include "webrtc/common.h"
#include "webrtc/system_wrappers/interface/trace.h"

#ifdef WEBRTC_ANDROID
#include "webrtc/modules/video_capture/include/video_capture_factory.h"
#include "webrtc/modules/video_render/include/video_render.h"
#endif

namespace webrtc {

enum { kModuleId = 0 };

VideoEngine* VideoEngine::Create() {
  return new VideoEngineImpl(new Config(), true /* owns_config */);
}

VideoEngine* VideoEngine::Create(const Config& config) {
  return new VideoEngineImpl(&config, false /* owns_config */);
}

bool VideoEngine::Delete(VideoEngine*& video_engine) {
  if (!video_engine) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, kModuleId,
                 "VideoEngine::Delete - No argument");
    return false;
  }
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, kModuleId,
               "VideoEngine::Delete(vie = 0x%p)", video_engine);
  VideoEngineImpl* vie_impl = static_cast<VideoEngineImpl*>(video_engine);

  // Check all reference counters.
  ViEBaseImpl* vie_base = vie_impl;
  if (vie_base->GetCount() > 0) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, kModuleId,
                 "ViEBase ref count: %d", vie_base->GetCount());
    return false;
  }
#ifdef WEBRTC_VIDEO_ENGINE_CAPTURE_API
  ViECaptureImpl* vie_capture = vie_impl;
  if (vie_capture->GetCount() > 0) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, kModuleId,
                 "ViECapture ref count: %d", vie_capture->GetCount());
    return false;
  }
#endif
#ifdef WEBRTC_VIDEO_ENGINE_CODEC_API
  ViECodecImpl* vie_codec = vie_impl;
  if (vie_codec->GetCount() > 0) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, kModuleId,
                 "ViECodec ref count: %d", vie_codec->GetCount());
    return false;
  }
#endif
#ifdef WEBRTC_VIDEO_ENGINE_ENCRYPTION_API
  ViEEncryptionImpl* vie_encryption = vie_impl;
  if (vie_encryption->GetCount() > 0) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, kModuleId,
                 "ViEEncryption ref count: %d", vie_encryption->GetCount());
    return false;
  }
#endif
#ifdef WEBRTC_VIDEO_ENGINE_EXTERNAL_CODEC_API
  ViEExternalCodecImpl* vie_external_codec = vie_impl;
  if (vie_external_codec->GetCount() > 0) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, kModuleId,
                 "ViEEncryption ref count: %d", vie_encryption->GetCount());
    return false;
  }
#endif
#ifdef WEBRTC_VIDEO_ENGINE_FILE_API
  ViEFileImpl* vie_file = vie_impl;
  if (vie_file->GetCount() > 0) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, kModuleId,
                 "ViEFile ref count: %d", vie_file->GetCount());
    return false;
  }
#endif
#ifdef WEBRTC_VIDEO_ENGINE_IMAGE_PROCESS_API
  ViEImageProcessImpl* vie_image_process = vie_impl;
  if (vie_image_process->GetCount() > 0) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, kModuleId,
                 "ViEImageProcess ref count: %d",
                 vie_image_process->GetCount());
    return false;
  }
#endif
  ViENetworkImpl* vie_network = vie_impl;
  if (vie_network->GetCount() > 0) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, kModuleId,
                 "ViENetwork ref count: %d", vie_network->GetCount());
    return false;
  }
#ifdef WEBRTC_VIDEO_ENGINE_RENDER_API
  ViERenderImpl* vie_render = vie_impl;
  if (vie_render->GetCount() > 0) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, kModuleId,
                 "ViERender ref count: %d", vie_render->GetCount());
    return false;
  }
#endif
#ifdef WEBRTC_VIDEO_ENGINE_RTP_RTCP_API
  ViERTP_RTCPImpl* vie_rtp_rtcp = vie_impl;
  if (vie_rtp_rtcp->GetCount() > 0) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, kModuleId,
                 "ViERTP_RTCP ref count: %d", vie_rtp_rtcp->GetCount());
    return false;
  }
#endif

  delete vie_impl;
  vie_impl = NULL;
  video_engine = NULL;

  WEBRTC_TRACE(kTraceInfo, kTraceVideo, kModuleId,
               "%s: instance deleted.", __FUNCTION__);
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
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, kModuleId,
               "SetTraceFileName(file_nameUTF8 = %s, add_file_counter = %d",
               file_nameUTF8, add_file_counter);
  return 0;
}

int VideoEngine::SetTraceFilter(const unsigned int filter) {
  uint32_t old_filter = 0;
  Trace::LevelFilter(old_filter);

  if (filter == kTraceNone && old_filter != kTraceNone) {
    // Do the logging before turning it off.
    WEBRTC_TRACE(kTraceApiCall, kTraceVideo, kModuleId,
                 "SetTraceFilter(filter = 0x%x)", filter);
  }

  int32_t error = Trace::SetLevelFilter(filter);
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, kModuleId,
               "SetTraceFilter(filter = 0x%x)", filter);
  if (error != 0) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, kModuleId,
                 "SetTraceFilter error: %d", error);
    return -1;
  }
  return 0;
}

int VideoEngine::SetTraceCallback(TraceCallback* callback) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, kModuleId,
               "SetTraceCallback(TraceCallback = 0x%p)", callback);
  return Trace::SetTraceCallback(callback);
}

int VideoEngine::SetAndroidObjects(void* javaVM, void* javaContext) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, kModuleId,
               "SetAndroidObjects()");

#if defined(WEBRTC_ANDROID) && !defined(WEBRTC_CHROMIUM_BUILD)
  if (SetCaptureAndroidVM(javaVM, javaContext) != 0) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, kModuleId,
                 "Could not set capture Android VM");
    return -1;
  }
  if (SetRenderAndroidVM(javaVM) != 0) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, kModuleId,
                 "Could not set render Android VM");
    return -1;
  }
  return 0;
#else
  WEBRTC_TRACE(kTraceError, kTraceVideo, kModuleId,
               "WEBRTC_ANDROID not defined for VideoEngine::SetAndroidObjects");
  return -1;
#endif
}

}  // namespace webrtc
