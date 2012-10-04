/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_ENGINE_VIE_RENDERER_H_
#define WEBRTC_VIDEO_ENGINE_VIE_RENDERER_H_

#include "modules/video_render/main/interface/video_render_defines.h"
#include "system_wrappers/interface/map_wrapper.h"
#include "system_wrappers/interface/scoped_ptr.h"
#include "video_engine/include/vie_render.h"
#include "video_engine/vie_frame_provider_base.h"

namespace webrtc {

class VideoRender;
class VideoRenderCallback;
class ViERenderManager;

class ViEExternalRendererImpl : public VideoRenderCallback {
 public:
  ViEExternalRendererImpl();
  virtual ~ViEExternalRendererImpl() {}

  int SetViEExternalRenderer(ExternalRenderer* external_renderer,
                             RawVideoType video_input_format);

  // Implements VideoRenderCallback.
  virtual WebRtc_Word32 RenderFrame(const WebRtc_UWord32 stream_id,
                                    VideoFrame& video_frame);

 private:
  ExternalRenderer* external_renderer_;
  RawVideoType external_renderer_format_;
  WebRtc_UWord32 external_renderer_width_;
  WebRtc_UWord32 external_renderer_height_;
  scoped_ptr<VideoFrame> converted_frame_;
};

class ViERenderer: public ViEFrameCallback {
 public:
  static ViERenderer* CreateViERenderer(const WebRtc_Word32 render_id,
                                        const WebRtc_Word32 engine_id,
                                        VideoRender& render_module,
                                        ViERenderManager& render_manager,
                                        const WebRtc_UWord32 z_order,
                                        const float left,
                                        const float top,
                                        const float right,
                                        const float bottom);
  ~ViERenderer(void);

  WebRtc_Word32 StartRender();
  WebRtc_Word32 StopRender();

  WebRtc_Word32 GetLastRenderedFrame(const WebRtc_Word32 renderID,
                                     VideoFrame& video_frame);

  WebRtc_Word32 ConfigureRenderer(const unsigned int z_order,
                                  const float left,
                                  const float top,
                                  const float right,
                                  const float bottom);

  VideoRender& RenderModule();

  WebRtc_Word32 EnableMirroring(const WebRtc_Word32 render_id,
                                const bool enable,
                                const bool mirror_xaxis,
                                const bool mirror_yaxis);

  WebRtc_Word32 SetTimeoutImage(const VideoFrame& timeout_image,
                                const WebRtc_Word32 timeout_value);
  WebRtc_Word32 SetRenderStartImage(const VideoFrame& start_image);
  WebRtc_Word32 SetExternalRenderer(const WebRtc_Word32 render_id,
                                    RawVideoType video_input_format,
                                    ExternalRenderer* external_renderer);

 private:
  ViERenderer(const WebRtc_Word32 render_id, const WebRtc_Word32 engine_id,
                VideoRender& render_module,
                ViERenderManager& render_manager);

  WebRtc_Word32 Init(const WebRtc_UWord32 z_order,
                     const float left,
                     const float top,
                     const float right,
                     const float bottom);

  // Implement ViEFrameCallback
  virtual void DeliverFrame(int id,
                            VideoFrame* video_frame,
                            int num_csrcs = 0,
                            const WebRtc_UWord32 CSRC[kRtpCsrcSize] = NULL);
  virtual void DelayChanged(int id, int frame_delay);
  virtual int GetPreferedFrameSettings(int* width,
                                       int* height,
                                       int* frame_rate);
  virtual void ProviderDestroyed(int id);

  WebRtc_UWord32 render_id_;
  VideoRender& render_module_;
  ViERenderManager& render_manager_;
  VideoRenderCallback* render_callback_;
  ViEExternalRendererImpl* incoming_external_callback_;
};

}  // namespace webrtc

#endif  // WEBRTC_VIDEO_ENGINE_VIE_RENDERER_H_
