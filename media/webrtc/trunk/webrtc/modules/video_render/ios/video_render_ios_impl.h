/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_RENDER_IOS_VIDEO_RENDER_IOS_IMPL_H_
#define WEBRTC_MODULES_VIDEO_RENDER_IOS_VIDEO_RENDER_IOS_IMPL_H_

#include <list>
#include <map>

#include "webrtc/modules/video_render/i_video_render.h"

namespace webrtc {

class VideoRenderIosGles20;
class CriticalSectionWrapper;

class VideoRenderIosImpl : IVideoRender {
 public:
  explicit VideoRenderIosImpl(const int32_t id,
                              void* window,
                              const bool full_screen);

  ~VideoRenderIosImpl();

  // Implementation of IVideoRender.
  int32_t Init() OVERRIDE;
  int32_t ChangeUniqueId(const int32_t id) OVERRIDE;
  int32_t ChangeWindow(void* window) OVERRIDE;

  VideoRenderCallback* AddIncomingRenderStream(const uint32_t stream_id,
                                               const uint32_t z_order,
                                               const float left,
                                               const float top,
                                               const float right,
                                               const float bottom) OVERRIDE;

  int32_t DeleteIncomingRenderStream(const uint32_t stream_id) OVERRIDE;

  int32_t GetIncomingRenderStreamProperties(const uint32_t stream_id,
                                            uint32_t& z_order,
                                            float& left,
                                            float& top,
                                            float& right,
                                            float& bottom) const OVERRIDE;

  int32_t StartRender() OVERRIDE;
  int32_t StopRender() OVERRIDE;

  VideoRenderType RenderType() OVERRIDE;
  RawVideoType PerferedVideoType() OVERRIDE;
  bool FullScreen() OVERRIDE;
  int32_t GetGraphicsMemory(
      uint64_t& total_graphics_memory,
      uint64_t& available_graphics_memory) const OVERRIDE;  // NOLINT
  int32_t GetScreenResolution(
      uint32_t& screen_width,
      uint32_t& screen_height) const OVERRIDE;  // NOLINT
  uint32_t RenderFrameRate(const uint32_t stream_id);
  int32_t SetStreamCropping(const uint32_t stream_id,
                            const float left,
                            const float top,
                            const float right,
                            const float bottom) OVERRIDE;
  int32_t ConfigureRenderer(const uint32_t stream_id,
                            const unsigned int z_order,
                            const float left,
                            const float top,
                            const float right,
                            const float bottom) OVERRIDE;
  int32_t SetTransparentBackground(const bool enable) OVERRIDE;
  int32_t SetText(const uint8_t text_id,
                  const uint8_t* text,
                  const int32_t text_length,
                  const uint32_t text_color_ref,
                  const uint32_t background_color_ref,
                  const float left,
                  const float top,
                  const float right,
                  const float bottom) OVERRIDE;
  int32_t SetBitmap(const void* bit_map,
                    const uint8_t picture_id,
                    const void* color_key,
                    const float left,
                    const float top,
                    const float right,
                    const float bottom);
  int32_t FullScreenRender(void* window, const bool enable);

 private:
  int32_t id_;
  void* ptr_window_;
  bool full_screen_;

  CriticalSectionWrapper* crit_sec_;
  webrtc::scoped_ptr<VideoRenderIosGles20> ptr_ios_render_;
};
}  // namespace webrtc
#endif  // WEBRTC_MODULES_VIDEO_RENDER_IOS_VIDEO_RENDER_IOS_IMPL_H_
