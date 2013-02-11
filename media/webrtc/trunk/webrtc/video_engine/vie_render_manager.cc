/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video_engine/vie_render_manager.h"

#include "engine_configurations.h"  // NOLINT
#include "webrtc/modules/video_render/include/video_render.h"
#include "webrtc/modules/video_render/include/video_render_defines.h"
#include "system_wrappers/interface/critical_section_wrapper.h"
#include "system_wrappers/interface/rw_lock_wrapper.h"
#include "system_wrappers/interface/trace.h"
#include "video_engine/vie_defines.h"
#include "video_engine/vie_renderer.h"

namespace webrtc {

ViERenderManagerScoped::ViERenderManagerScoped(
    const ViERenderManager& vie_render_manager)
    : ViEManagerScopedBase(vie_render_manager) {
}

ViERenderer* ViERenderManagerScoped::Renderer(WebRtc_Word32 render_id) const {
  return static_cast<const ViERenderManager*>(vie_manager_)->ViERenderPtr(
           render_id);
}

ViERenderManager::ViERenderManager(WebRtc_Word32 engine_id)
    : list_cs_(CriticalSectionWrapper::CreateCriticalSection()),
      engine_id_(engine_id),
      use_external_render_module_(false) {
  WEBRTC_TRACE(webrtc::kTraceMemory, webrtc::kTraceVideo, ViEId(engine_id),
               "ViERenderManager::ViERenderManager(engine_id: %d) - "
               "Constructor", engine_id);
}

ViERenderManager::~ViERenderManager() {
  WEBRTC_TRACE(webrtc::kTraceMemory, webrtc::kTraceVideo, ViEId(engine_id_),
               "ViERenderManager Destructor, engine_id: %d", engine_id_);

  while (stream_to_vie_renderer_.Size() != 0) {
    MapItem* item = stream_to_vie_renderer_.First();
    assert(item);
    const WebRtc_Word32 render_id = item->GetId();
    // The renderer is delete in RemoveRenderStream.
    item = NULL;
    RemoveRenderStream(render_id);
  }
}

WebRtc_Word32 ViERenderManager::RegisterVideoRenderModule(
    VideoRender* render_module) {
  // See if there is already a render module registered for the window that
  // the registrant render module is associated with.
  VideoRender* current_module = FindRenderModule(render_module->Window());
  if (current_module) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(engine_id_),
                 "A module is already registered for this window (window=%p, "
                 "current module=%p, registrant module=%p.",
                 render_module->Window(), current_module, render_module);
    return -1;
  }

  // Register module.
  render_list_.PushBack(static_cast<void*>(render_module));
  use_external_render_module_ = true;
  return 0;
}

WebRtc_Word32 ViERenderManager::DeRegisterVideoRenderModule(
    VideoRender* render_module) {
  // Check if there are streams in the module.
  WebRtc_UWord32 n_streams = render_module->GetNumIncomingRenderStreams();
  if (n_streams != 0) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(engine_id_),
                 "There are still %d streams in this module, cannot "
                 "de-register", n_streams);
    return -1;
  }

  // Erase the render module from the map.
  ListItem* list_item = NULL;
  bool found = false;
  for (list_item = render_list_.First(); list_item != NULL;
       list_item = render_list_.Next(list_item)) {
    if (render_module == static_cast<VideoRender*>(list_item->GetItem())) {
      // We've found our renderer.
      render_list_.Erase(list_item);
      found = true;
      break;
    }
  }
  if (!found) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(engine_id_),
                 "Module not registered");
    return -1;
  }
  return 0;
}

ViERenderer* ViERenderManager::AddRenderStream(const WebRtc_Word32 render_id,
                                               void* window,
                                               const WebRtc_UWord32 z_order,
                                               const float left,
                                               const float top,
                                               const float right,
                                               const float bottom) {
  CriticalSectionScoped cs(list_cs_.get());

  if (stream_to_vie_renderer_.Find(render_id) != NULL) {
    // This stream is already added to a renderer, not allowed!
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(engine_id_),
                 "Render stream already exists");
    return NULL;
  }

  // Get the render module for this window.
  VideoRender* render_module = FindRenderModule(window);
  if (render_module == NULL) {
    // No render module for this window, create a new one.
    render_module = VideoRender::CreateVideoRender(ViEModuleId(engine_id_, -1),
                                                  window, false);
    if (!render_module) {
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(engine_id_),
                   "Could not create new render module");
      return NULL;
    }
    render_list_.PushBack(static_cast<void*>(render_module));
  }

  ViERenderer* vie_renderer = ViERenderer::CreateViERenderer(render_id,
                                                             engine_id_,
                                                             *render_module,
                                                             *this, z_order,
                                                             left, top, right,
                                                             bottom);
  if (!vie_renderer) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                 ViEId(engine_id_, render_id),
                 "Could not create new render stream");
    return NULL;
  }
  stream_to_vie_renderer_.Insert(render_id, vie_renderer);
  return vie_renderer;
}

WebRtc_Word32 ViERenderManager::RemoveRenderStream(
    const WebRtc_Word32 render_id) {
  // We need exclusive right to the items in the render manager to delete a
  // stream.
  ViEManagerWriteScoped scope(this);

  CriticalSectionScoped cs(list_cs_.get());
  MapItem* map_item = stream_to_vie_renderer_.Find(render_id);
  if (!map_item) {
    // No such stream
    WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceVideo, ViEId(engine_id_),
                 "No renderer for this stream found, channel_id");
    return 0;
  }

  ViERenderer* vie_renderer = static_cast<ViERenderer*>(map_item->GetItem());
  assert(vie_renderer);

  // Get the render module pointer for this vie_render object.
  VideoRender& renderer = vie_renderer->RenderModule();

  // Delete the vie_render.
  // This deletes the stream in the render module.
  delete vie_renderer;

  // Remove from the stream map.
  stream_to_vie_renderer_.Erase(map_item);

  // Check if there are other streams in the module.
  if (!use_external_render_module_ &&
      renderer.GetNumIncomingRenderStreams() == 0) {
    // Erase the render module from the map.
    ListItem* list_item = NULL;
    for (list_item = render_list_.First(); list_item != NULL;
         list_item = render_list_.Next(list_item)) {
      if (&renderer == static_cast<VideoRender*>(list_item->GetItem())) {
        // We've found our renderer.
        render_list_.Erase(list_item);
        break;
      }
    }
    // Destroy the module.
    VideoRender::DestroyVideoRender(&renderer);
  }
  return 0;
}

VideoRender* ViERenderManager::FindRenderModule(void* window) {
  VideoRender* renderer = NULL;
  ListItem* list_item = NULL;
  for (list_item = render_list_.First(); list_item != NULL;
       list_item = render_list_.Next(list_item)) {
    renderer = static_cast<VideoRender*>(list_item->GetItem());
    if (renderer == NULL) {
      break;
    }
    if (renderer->Window() == window) {
      // We've found the render module.
      break;
    }
    renderer = NULL;
  }
  return renderer;
}

ViERenderer* ViERenderManager::ViERenderPtr(WebRtc_Word32 render_id) const {
  ViERenderer* renderer = NULL;
  MapItem* map_item = stream_to_vie_renderer_.Find(render_id);
  if (!map_item) {
    // No such stream in any renderer.
    return NULL;
  }
  renderer = static_cast<ViERenderer*>(map_item->GetItem());

  return renderer;
}

}  // namespace webrtc
