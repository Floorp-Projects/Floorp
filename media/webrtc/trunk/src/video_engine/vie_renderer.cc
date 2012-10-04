/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video_engine/vie_renderer.h"

#include "common_video/libyuv/include/webrtc_libyuv.h"
#include "modules/video_render/main/interface/video_render.h"
#include "modules/video_render/main/interface/video_render_defines.h"
#include "video_engine/vie_render_manager.h"

namespace webrtc {

ViERenderer* ViERenderer::CreateViERenderer(const WebRtc_Word32 render_id,
                                            const WebRtc_Word32 engine_id,
                                            VideoRender& render_module,
                                            ViERenderManager& render_manager,
                                            const WebRtc_UWord32 z_order,
                                            const float left,
                                            const float top,
                                            const float right,
                                            const float bottom) {
  ViERenderer* self = new ViERenderer(render_id, engine_id, render_module,
                                      render_manager);
  if (!self || self->Init(z_order, left, top, right, bottom) != 0) {
    delete self;
    self = NULL;
  }
  return self;
}

ViERenderer::~ViERenderer(void) {
  if (render_callback_)
    render_module_.DeleteIncomingRenderStream(render_id_);

  if (incoming_external_callback_)
    delete incoming_external_callback_;
}

WebRtc_Word32 ViERenderer::StartRender() {
  return render_module_.StartRender(render_id_);
}
WebRtc_Word32 ViERenderer::StopRender() {
  return render_module_.StopRender(render_id_);
}

WebRtc_Word32 ViERenderer::GetLastRenderedFrame(const WebRtc_Word32 renderID,
                                                VideoFrame& video_frame) {
  return render_module_.GetLastRenderedFrame(renderID, video_frame);
}

WebRtc_Word32 ViERenderer::ConfigureRenderer(const unsigned int z_order,
                                             const float left,
                                             const float top,
                                             const float right,
                                             const float bottom) {
  return render_module_.ConfigureRenderer(render_id_, z_order, left, top, right,
                                          bottom);
}

VideoRender& ViERenderer::RenderModule() {
  return render_module_;
}

WebRtc_Word32 ViERenderer::EnableMirroring(const WebRtc_Word32 render_id,
                                           const bool enable,
                                           const bool mirror_xaxis,
                                           const bool mirror_yaxis) {
  return render_module_.MirrorRenderStream(render_id, enable, mirror_xaxis,
                                           mirror_yaxis);
}

WebRtc_Word32 ViERenderer::SetTimeoutImage(const VideoFrame& timeout_image,
                                           const WebRtc_Word32 timeout_value) {
  return render_module_.SetTimeoutImage(render_id_, timeout_image,
                                        timeout_value);
}

WebRtc_Word32  ViERenderer::SetRenderStartImage(const VideoFrame& start_image) {
  return render_module_.SetStartImage(render_id_, start_image);
}

WebRtc_Word32 ViERenderer::SetExternalRenderer(
    const WebRtc_Word32 render_id,
    RawVideoType video_input_format,
    ExternalRenderer* external_renderer) {
  if (!incoming_external_callback_)
    return -1;

  incoming_external_callback_->SetViEExternalRenderer(external_renderer,
                                                      video_input_format);
  return render_module_.AddExternalRenderCallback(render_id,
                                                  incoming_external_callback_);
}

ViERenderer::ViERenderer(const WebRtc_Word32 render_id,
                         const WebRtc_Word32 engine_id,
                         VideoRender& render_module,
                         ViERenderManager& render_manager)
    : render_id_(render_id),
      render_module_(render_module),
      render_manager_(render_manager),
      render_callback_(NULL),
      incoming_external_callback_(new ViEExternalRendererImpl()) {
}

WebRtc_Word32 ViERenderer::Init(const WebRtc_UWord32 z_order,
                                const float left,
                                const float top,
                                const float right,
                                const float bottom) {
  render_callback_ =
      static_cast<VideoRenderCallback*>(render_module_.AddIncomingRenderStream(
          render_id_, z_order, left, top, right, bottom));
  if (!render_callback_) {
    // Logging done.
    return -1;
  }
  return 0;
}

void ViERenderer::DeliverFrame(int id,
                               VideoFrame* video_frame,
                               int num_csrcs,
                               const WebRtc_UWord32 CSRC[kRtpCsrcSize]) {
  render_callback_->RenderFrame(render_id_, *video_frame);
}

void ViERenderer::DelayChanged(int id, int frame_delay) {}

int ViERenderer::GetPreferedFrameSettings(int* width,
                                          int* height,
                                          int* frame_rate) {
    return -1;
}

void ViERenderer::ProviderDestroyed(int id) {
  // Remove the render stream since the provider is destroyed.
  render_manager_.RemoveRenderStream(render_id_);
}

ViEExternalRendererImpl::ViEExternalRendererImpl()
    : external_renderer_(NULL),
      external_renderer_format_(kVideoUnknown),
      external_renderer_width_(0),
      external_renderer_height_(0),
      converted_frame_(new VideoFrame()) {
}

int ViEExternalRendererImpl::SetViEExternalRenderer(
    ExternalRenderer* external_renderer,
    RawVideoType video_input_format) {
  external_renderer_ = external_renderer;
  external_renderer_format_ = video_input_format;
  return 0;
}

WebRtc_Word32 ViEExternalRendererImpl::RenderFrame(
    const WebRtc_UWord32 stream_id,
    VideoFrame&   video_frame) {
  VideoFrame* out_frame = converted_frame_.get();

  // Convert to requested format.
  VideoType type =
      RawVideoTypeToCommonVideoVideoType(external_renderer_format_);
  int buffer_size = CalcBufferSize(type, video_frame.Width(),
                                   video_frame.Height());
  if (buffer_size <= 0) {
    // Unsupported video format.
    assert(false);
    return -1;
  }
  converted_frame_->VerifyAndAllocate(buffer_size);

  switch (external_renderer_format_) {
    case kVideoI420:
      out_frame = &video_frame;
      break;
    case kVideoYV12:
    case kVideoYUY2:
    case kVideoUYVY:
    case kVideoARGB:
    case kVideoRGB24:
    case kVideoRGB565:
    case kVideoARGB4444:
    case kVideoARGB1555 :
      {
        ConvertFromI420(video_frame.Buffer(), video_frame.Width(), type, 0,
                        video_frame.Width(), video_frame.Height(),
                        converted_frame_->Buffer());
      }
      break;
    case kVideoIYUV:
      // no conversion available
      break;
    default:
      assert(false);
      out_frame = NULL;
      break;
  }

  if (external_renderer_width_ != video_frame.Width() ||
      external_renderer_height_ != video_frame.Height()) {
    external_renderer_width_ = video_frame.Width();
    external_renderer_height_ = video_frame.Height();
    external_renderer_->FrameSizeChange(external_renderer_width_,
                                        external_renderer_height_, stream_id);
  }

  if (out_frame) {
    external_renderer_->DeliverFrame(out_frame->Buffer(),
                                     out_frame->Length(),
                                     video_frame.TimeStamp(),
                                     video_frame.RenderTimeMs());
  }
  return 0;
}

}  // namespace webrtc
