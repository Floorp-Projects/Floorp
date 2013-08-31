/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/video_engine/new_include/video_engine.h"

#include <cassert>

#include "webrtc/video_engine/include/vie_base.h"
#include "webrtc/video_engine/internal/video_call.h"
#include "webrtc/video_engine/vie_defines.h"

namespace webrtc {
namespace internal {

class VideoEngine : public newapi::VideoEngine {
 public:
  explicit VideoEngine(const newapi::VideoEngineConfig& engine_config)
      : config_(engine_config) {
    video_engine_ = webrtc::VideoEngine::Create();
    assert(video_engine_ != NULL);

    ViEBase* video_engine_base = ViEBase::GetInterface(video_engine_);
    assert(video_engine_base != NULL);
    if (video_engine_base->Init() != 0) {
      abort();
    }
    video_engine_base->Release();
  }

  virtual ~VideoEngine() { webrtc::VideoEngine::Delete(video_engine_); }

  virtual newapi::VideoCall* CreateCall(newapi::Transport* transport) OVERRIDE {
    return new VideoCall(video_engine_, transport);
  }

 private:
  newapi::VideoEngineConfig config_;
  webrtc::VideoEngine* video_engine_;

  DISALLOW_COPY_AND_ASSIGN(VideoEngine);
};
}  // internal

namespace newapi {

VideoEngine* VideoEngine::Create(const VideoEngineConfig& engine_config) {
  return new internal::VideoEngine(engine_config);
}

const char* Version() { return WEBRTC_SVNREVISION " (" BUILDINFO ")"; }
}  // newapi
}  // webrtc
