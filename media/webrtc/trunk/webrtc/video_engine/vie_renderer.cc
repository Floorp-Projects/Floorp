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
#include "webrtc/modules/video_render/include/video_render.h"
#include "webrtc/modules/video_render/include/video_render_defines.h"
#include "video_engine/vie_render_manager.h"

namespace webrtc {

ViERenderer* ViERenderer::CreateViERenderer(const int32_t render_id,
                                            const int32_t engine_id,
                                            VideoRender& render_module,
                                            ViERenderManager& render_manager,
                                            const uint32_t z_order,
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

int32_t ViERenderer::StartRender() {
  return render_module_.StartRender(render_id_);
}
int32_t ViERenderer::StopRender() {
  return render_module_.StopRender(render_id_);
}

int32_t ViERenderer::GetLastRenderedFrame(const int32_t renderID,
                                          I420VideoFrame& video_frame) {
  return render_module_.GetLastRenderedFrame(renderID, video_frame);
}

int ViERenderer::SetExpectedRenderDelay(int render_delay) {
  return render_module_.SetExpectedRenderDelay(render_id_, render_delay);
}

int32_t ViERenderer::ConfigureRenderer(const unsigned int z_order,
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

int32_t ViERenderer::EnableMirroring(const int32_t render_id,
                                     const bool enable,
                                     const bool mirror_xaxis,
                                     const bool mirror_yaxis) {
  return render_module_.MirrorRenderStream(render_id, enable, mirror_xaxis,
                                           mirror_yaxis);
}

int32_t ViERenderer::SetTimeoutImage(const I420VideoFrame& timeout_image,
                                     const int32_t timeout_value) {
  return render_module_.SetTimeoutImage(render_id_, timeout_image,
                                        timeout_value);
}

int32_t  ViERenderer::SetRenderStartImage(
    const I420VideoFrame& start_image) {
  return render_module_.SetStartImage(render_id_, start_image);
}

int32_t ViERenderer::SetExternalRenderer(
    const int32_t render_id,
    RawVideoType video_input_format,
    ExternalRenderer* external_renderer) {
  if (!incoming_external_callback_)
    return -1;

  incoming_external_callback_->SetViEExternalRenderer(external_renderer,
                                                      video_input_format);
  return render_module_.AddExternalRenderCallback(render_id,
                                                  incoming_external_callback_);
}

ViERenderer::ViERenderer(const int32_t render_id,
                         const int32_t engine_id,
                         VideoRender& render_module,
                         ViERenderManager& render_manager)
    : render_id_(render_id),
      render_module_(render_module),
      render_manager_(render_manager),
      render_callback_(NULL),
      incoming_external_callback_(new ViEExternalRendererImpl()) {
}

int32_t ViERenderer::Init(const uint32_t z_order,
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
                               I420VideoFrame* video_frame,
                               int num_csrcs,
                               const uint32_t CSRC[kRtpCsrcSize]) {
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

int32_t ViEExternalRendererImpl::RenderFrame(
    const uint32_t stream_id,
    I420VideoFrame&   video_frame) {
  VideoFrame* out_frame = converted_frame_.get();

  // Convert to requested format.
  VideoType type =
      RawVideoTypeToCommonVideoVideoType(external_renderer_format_);
  int buffer_size = CalcBufferSize(type, video_frame.width(),
                                   video_frame.height());
  if (buffer_size <= 0) {
    // Unsupported video format.
    assert(false);
    return -1;
  }
  converted_frame_->VerifyAndAllocate(buffer_size);

  switch (external_renderer_format_) {
    case kVideoI420: {
      // TODO(mikhal): need to copy the buffer as is.
      // can the output here be a I420 frame?
      int length = ExtractBuffer(video_frame, out_frame->Size(),
                                 out_frame->Buffer());
      if (length < 0)
        return -1;
      out_frame->SetLength(length);
      break;
    }
    case kVideoYV12:
    case kVideoYUY2:
    case kVideoUYVY:
    case kVideoARGB:
    case kVideoRGB24:
    case kVideoRGB565:
    case kVideoARGB4444:
    case kVideoARGB1555 :
      {
        if (ConvertFromI420(video_frame, type, 0,
                            converted_frame_->Buffer()) < 0)
          return -1;
        converted_frame_->SetLength(buffer_size);
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

  if (external_renderer_width_ != video_frame.width() ||
      external_renderer_height_ != video_frame.height()) {
    external_renderer_width_ = video_frame.width();
    external_renderer_height_ = video_frame.height();
    external_renderer_->FrameSizeChange(external_renderer_width_,
                                        external_renderer_height_, stream_id);
  }

  if (out_frame) {
    external_renderer_->DeliverFrame(out_frame->Buffer(),
                                     out_frame->Length(),
                                     video_frame.timestamp(),
                                     video_frame.render_time_ms());
  }
  return 0;
}

}  // namespace webrtc
