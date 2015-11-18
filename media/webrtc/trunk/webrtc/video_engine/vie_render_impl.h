/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_ENGINE_VIE_RENDER_IMPL_H_
#define WEBRTC_VIDEO_ENGINE_VIE_RENDER_IMPL_H_

#include "webrtc/modules/video_render/include/video_render_defines.h"
#include "webrtc/typedefs.h"
#include "webrtc/video_engine/include/vie_render.h"
#include "webrtc/video_engine/vie_ref_count.h"

namespace webrtc {

class ViESharedData;

class ViERenderImpl
    : public ViERender,
      public ViERefCount {
 public:
  // Implements ViERender
  virtual int Release();
  virtual int RegisterVideoRenderModule(VideoRender& render_module);  // NOLINT
  virtual int DeRegisterVideoRenderModule(
      VideoRender& render_module);  // NOLINT
  virtual int AddRenderer(const int render_id, void* window,
                          const unsigned int z_order, const float left,
                          const float top, const float right,
                          const float bottom);
  virtual int RemoveRenderer(const int render_id);
  virtual int StartRender(const int render_id);
  virtual int StopRender(const int render_id);
  virtual int SetExpectedRenderDelay(int render_id, int render_delay);
  virtual int ConfigureRender(int render_id, const unsigned int z_order,
                              const float left, const float top,
                              const float right, const float bottom);
  virtual int AddRenderer(const int render_id, RawVideoType video_input_format,
                          ExternalRenderer* renderer);

 protected:
  explicit ViERenderImpl(ViESharedData* shared_data);
  virtual ~ViERenderImpl();

 private:
  ViESharedData* shared_data_;
};

}  // namespace webrtc

#endif  // WEBRTC_VIDEO_ENGINE_VIE_RENDER_IMPL_H_
