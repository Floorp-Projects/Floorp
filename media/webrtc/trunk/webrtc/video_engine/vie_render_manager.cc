/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/video_engine/vie_render_manager.h"

#include "webrtc/engine_configurations.h"
#include "webrtc/modules/video_render/include/video_render.h"
#include "webrtc/modules/video_render/include/video_render_defines.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/rw_lock_wrapper.h"
#include "webrtc/system_wrappers/interface/logging.h"
#include "webrtc/video_engine/vie_defines.h"
#include "webrtc/video_engine/vie_renderer.h"

namespace webrtc {

ViERenderManagerScoped::ViERenderManagerScoped(
    const ViERenderManager& vie_render_manager)
    : ViEManagerScopedBase(vie_render_manager) {
}

ViERenderer* ViERenderManagerScoped::Renderer(int32_t render_id) const {
  return static_cast<const ViERenderManager*>(vie_manager_)->ViERenderPtr(
           render_id);
}

ViERenderManager::ViERenderManager(int32_t engine_id)
    : list_cs_(CriticalSectionWrapper::CreateCriticalSection()),
      engine_id_(engine_id),
      use_external_render_module_(false) {
}

ViERenderManager::~ViERenderManager() {
  for (RendererMap::iterator it = stream_to_vie_renderer_.begin();
       it != stream_to_vie_renderer_.end();
       ++it) {
    // The renderer is deleted in RemoveRenderStream.
    RemoveRenderStream(it->first);
  }
}

int32_t ViERenderManager::RegisterVideoRenderModule(
    VideoRender* render_module) {
  // See if there is already a render module registered for the window that
  // the registrant render module is associated with.
  VideoRender* current_module = FindRenderModule(render_module->Window());
  if (current_module) {
    LOG_F(LS_ERROR) << "A render module is already registered for this window.";
    return -1;
  }

  // Register module.
  render_list_.push_back(render_module);
  use_external_render_module_ = true;
  return 0;
}

int32_t ViERenderManager::DeRegisterVideoRenderModule(
    VideoRender* render_module) {
  // Check if there are streams in the module.
  uint32_t n_streams = render_module->GetNumIncomingRenderStreams();
  if (n_streams != 0) {
    LOG(LS_ERROR) << "There are still " << n_streams
                  << "in this module, cannot de-register.";
    return -1;
  }

  for (RenderList::iterator iter = render_list_.begin();
       iter != render_list_.end(); ++iter) {
    if (render_module == *iter) {
      // We've found our renderer. Erase the render module from the map.
      render_list_.erase(iter);
      return 0;
    }
  }

  LOG(LS_ERROR) << "Module not registered.";
  return -1;
}

ViERenderer* ViERenderManager::AddRenderStream(const int32_t render_id,
                                               void* window,
                                               const uint32_t z_order,
                                               const float left,
                                               const float top,
                                               const float right,
                                               const float bottom) {
  CriticalSectionScoped cs(list_cs_.get());

  if (stream_to_vie_renderer_.find(render_id) !=
      stream_to_vie_renderer_.end()) {
    LOG(LS_ERROR) << "Render stream already exists";
    return NULL;
  }

  // Get the render module for this window.
  VideoRender* render_module = FindRenderModule(window);
  if (render_module == NULL) {
    // No render module for this window, create a new one.
    render_module = VideoRender::CreateVideoRender(ViEModuleId(engine_id_, -1),
                                                  window, false);
    if (!render_module)
      return NULL;

    render_list_.push_back(render_module);
  }

  ViERenderer* vie_renderer = ViERenderer::CreateViERenderer(render_id,
                                                             engine_id_,
                                                             *render_module,
                                                             *this, z_order,
                                                             left, top, right,
                                                             bottom);
  if (!vie_renderer)
    return NULL;

  stream_to_vie_renderer_[render_id] = vie_renderer;
  return vie_renderer;
}

int32_t ViERenderManager::RemoveRenderStream(
    const int32_t render_id) {
  // We need exclusive right to the items in the render manager to delete a
  // stream.
  ViEManagerWriteScoped scope(this);
  CriticalSectionScoped cs(list_cs_.get());
  RendererMap::iterator it = stream_to_vie_renderer_.find(render_id);
  if (it == stream_to_vie_renderer_.end()) {
    LOG(LS_ERROR) << "No renderer found for render_id: " << render_id;
    return 0;
  }

  // Get the render module pointer for this vie_render object.
  VideoRender& renderer = it->second->RenderModule();

  // Delete the vie_render.
  // This deletes the stream in the render module.
  delete it->second;

  // Remove from the stream map.
  stream_to_vie_renderer_.erase(it);

  // Check if there are other streams in the module.
  if (!use_external_render_module_ &&
      renderer.GetNumIncomingRenderStreams() == 0) {
    // Erase the render module from the map.
    for (RenderList::iterator iter = render_list_.begin();
         iter != render_list_.end(); ++iter) {
      if (&renderer == *iter) {
        // We've found our renderer.
        render_list_.erase(iter);
        break;
      }
    }
    // Destroy the module.
    VideoRender::DestroyVideoRender(&renderer);
  }
  return 0;
}

VideoRender* ViERenderManager::FindRenderModule(void* window) {
  for (RenderList::iterator iter = render_list_.begin();
       iter != render_list_.end(); ++iter) {
    if ((*iter)->Window() == window) {
      // We've found the render module.
      return *iter;
    }
  }
  return NULL;
}

ViERenderer* ViERenderManager::ViERenderPtr(int32_t render_id) const {
  RendererMap::const_iterator it = stream_to_vie_renderer_.find(render_id);
  if (it == stream_to_vie_renderer_.end())
    return NULL;

  return it->second;
}

}  // namespace webrtc
