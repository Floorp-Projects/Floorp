/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_ENGINE_VIE_RENDER_MANAGER_H_
#define WEBRTC_VIDEO_ENGINE_VIE_RENDER_MANAGER_H_

#include <list>
#include <map>

#include "webrtc/base/scoped_ptr.h"
#include "webrtc/typedefs.h"
#include "webrtc/video_engine/vie_manager_base.h"

namespace webrtc {

class CriticalSectionWrapper;
class RWLockWrapper;
class VideoRender;
class VideoRenderCallback;
class ViERenderer;

class ViERenderManager : private ViEManagerBase {
  friend class ViERenderManagerScoped;
 public:
  explicit ViERenderManager(int32_t engine_id);
  ~ViERenderManager();

  int32_t RegisterVideoRenderModule(VideoRender* render_module);
  int32_t DeRegisterVideoRenderModule(VideoRender* render_module);

  ViERenderer* AddRenderStream(const int32_t render_id,
                               void* window,
                               const uint32_t z_order,
                               const float left,
                               const float top,
                               const float right,
                               const float bottom);

  int32_t RemoveRenderStream(int32_t render_id);

 private:
  typedef std::list<VideoRender*> RenderList;
  // Returns a pointer to the render module if it exists in the render list.
  // Assumed protected.
  VideoRender* FindRenderModule(void* window);

  // Methods used by ViERenderScoped.
  ViERenderer* ViERenderPtr(int32_t render_id) const;

  rtc::scoped_ptr<CriticalSectionWrapper> list_cs_;
  int32_t engine_id_;
  // Protected by ViEManagerBase.
  typedef std::map<int32_t, ViERenderer*> RendererMap;
  RendererMap stream_to_vie_renderer_;
  RenderList render_list_;
  bool use_external_render_module_;
};

class ViERenderManagerScoped: private ViEManagerScopedBase {
 public:
  explicit ViERenderManagerScoped(const ViERenderManager& vie_render_manager);

  // Returns a pointer to the ViERender object.
  ViERenderer* Renderer(int32_t render_id) const;
};

}  // namespace webrtc

#endif  // WEBRTC_VIDEO_ENGINE_VIE_RENDER_MANAGER_H_
