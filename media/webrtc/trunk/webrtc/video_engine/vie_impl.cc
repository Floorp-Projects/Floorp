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
#include "webrtc/system_wrappers/interface/logging.h"
#include "webrtc/system_wrappers/interface/trace.h"

namespace webrtc {

enum { kModuleId = 0 };

VideoEngine* VideoEngine::Create() {
  return new VideoEngineImpl(new Config(), true /* owns_config */);
}

VideoEngine* VideoEngine::Create(const Config& config) {
  return new VideoEngineImpl(&config, false /* owns_config */);
}

bool VideoEngine::Delete(VideoEngine*& video_engine) {
  if (!video_engine)
    return false;

  LOG_F(LS_INFO);
  VideoEngineImpl* vie_impl = static_cast<VideoEngineImpl*>(video_engine);

  // Check all reference counters.
  ViEBaseImpl* vie_base = vie_impl;
  if (vie_base->GetCount() > 0) {
    LOG(LS_ERROR) << "ViEBase ref count > 0: " << vie_base->GetCount();
    return false;
  }
#ifdef WEBRTC_VIDEO_ENGINE_CAPTURE_API
  ViECaptureImpl* vie_capture = vie_impl;
  if (vie_capture->GetCount() > 0) {
    LOG(LS_ERROR) << "ViECapture ref count > 0: " << vie_capture->GetCount();
    return false;
  }
#endif
#ifdef WEBRTC_VIDEO_ENGINE_CODEC_API
  ViECodecImpl* vie_codec = vie_impl;
  if (vie_codec->GetCount() > 0) {
    LOG(LS_ERROR) << "ViECodec ref count > 0: " << vie_codec->GetCount();
    return false;
  }
#endif
#ifdef WEBRTC_VIDEO_ENGINE_EXTERNAL_CODEC_API
  ViEExternalCodecImpl* vie_external_codec = vie_impl;
  if (vie_external_codec->GetCount() > 0) {
    LOG(LS_ERROR) << "ViEExternalCodec ref count > 0: "
                  << vie_external_codec->GetCount();
    return false;
  }
#endif
#ifdef WEBRTC_VIDEO_ENGINE_FILE_API
  ViEFileImpl* vie_file = vie_impl;
  if (vie_file->GetCount() > 0) {
    LOG(LS_ERROR) << "ViEFile ref count > 0: " << vie_file->GetCount();
    return false;
  }
#endif
#ifdef WEBRTC_VIDEO_ENGINE_IMAGE_PROCESS_API
  ViEImageProcessImpl* vie_image_process = vie_impl;
  if (vie_image_process->GetCount() > 0) {
    LOG(LS_ERROR) << "ViEImageProcess ref count > 0: "
                  << vie_image_process->GetCount();
    return false;
  }
#endif
  ViENetworkImpl* vie_network = vie_impl;
  if (vie_network->GetCount() > 0) {
    LOG(LS_ERROR) << "ViENetwork ref count > 0: " << vie_network->GetCount();
    return false;
  }
#ifdef WEBRTC_VIDEO_ENGINE_RENDER_API
  ViERenderImpl* vie_render = vie_impl;
  if (vie_render->GetCount() > 0) {
    LOG(LS_ERROR) << "ViERender ref count > 0: " << vie_render->GetCount();
    return false;
  }
#endif
#ifdef WEBRTC_VIDEO_ENGINE_RTP_RTCP_API
  ViERTP_RTCPImpl* vie_rtp_rtcp = vie_impl;
  if (vie_rtp_rtcp->GetCount() > 0) {
    LOG(LS_ERROR) << "ViERTP_RTCP ref count > 0: " << vie_rtp_rtcp->GetCount();
    return false;
  }
#endif

  delete vie_impl;
  vie_impl = NULL;
  video_engine = NULL;

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
  LOG_F(LS_INFO) << "filename: " << file_nameUTF8
                 << " add_file_counter: " << (add_file_counter ? "yes" : "no");
  return 0;
}

int VideoEngine::SetTraceFilter(const unsigned int filter) {
  uint32_t old_filter = Trace::level_filter();

  if (filter == kTraceNone && old_filter != kTraceNone) {
    // Do the logging before turning it off.
    LOG_F(LS_INFO) << "filter: " << filter;
  }

  Trace::set_level_filter(filter);
  LOG_F(LS_INFO) << "filter: " << filter;
  return 0;
}

int VideoEngine::SetTraceCallback(TraceCallback* callback) {
  LOG_F(LS_INFO);
  return Trace::SetTraceCallback(callback);
}

}  // namespace webrtc
