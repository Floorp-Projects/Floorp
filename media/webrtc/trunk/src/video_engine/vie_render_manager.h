/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_ENGINE_VIE_RENDER_MANAGER_H_
#define WEBRTC_VIDEO_ENGINE_VIE_RENDER_MANAGER_H_

#include "system_wrappers/interface/list_wrapper.h"
#include "system_wrappers/interface/map_wrapper.h"
#include "system_wrappers/interface/scoped_ptr.h"
#include "typedefs.h"
#include "video_engine/vie_manager_base.h"

namespace webrtc {

class CriticalSectionWrapper;
class RWLockWrapper;
class VideoRender;
class VideoRenderCallback;
class ViERenderer;

class ViERenderManager : private ViEManagerBase {
  friend class ViERenderManagerScoped;
 public:
  explicit ViERenderManager(WebRtc_Word32 engine_id);
  ~ViERenderManager();

  WebRtc_Word32 RegisterVideoRenderModule(VideoRender& render_module);
  WebRtc_Word32 DeRegisterVideoRenderModule(VideoRender& render_module);

  ViERenderer* AddRenderStream(const WebRtc_Word32 render_id,
                               void* window,
                               const WebRtc_UWord32 z_order,
                               const float left,
                               const float top,
                               const float right,
                               const float bottom);

  WebRtc_Word32 RemoveRenderStream(WebRtc_Word32 render_id);

 private:
  // Returns a pointer to the render module if it exists in the render list.
  // Assumed protected.
  VideoRender* FindRenderModule(void* window);

  // Methods used by ViERenderScoped.
  ViERenderer* ViERenderPtr(WebRtc_Word32 render_id) const;

  scoped_ptr<CriticalSectionWrapper> list_cs_;
  WebRtc_Word32 engine_id_;
  MapWrapper stream_to_vie_renderer_;  // Protected by ViEManagerBase.
  ListWrapper render_list_;
  bool use_external_render_module_;
};

class ViERenderManagerScoped: private ViEManagerScopedBase {
 public:
  explicit ViERenderManagerScoped(const ViERenderManager& vie_render_manager);

  // Returns a pointer to the ViERender object.
  ViERenderer* Renderer(WebRtc_Word32 render_id) const;
};

}  // namespace webrtc

#endif  // WEBRTC_VIDEO_ENGINE_VIE_RENDER_MANAGER_H_
