/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_RENDER_IOS_VIDEO_RENDER_IOS_GLES20_H_
#define WEBRTC_MODULES_VIDEO_RENDER_IOS_VIDEO_RENDER_IOS_GLES20_H_

#include <list>
#include <map>

#include "webrtc/base/platform_thread.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/modules/video_render/ios/video_render_ios_channel.h"
#include "webrtc/modules/video_render/ios/video_render_ios_view.h"

namespace webrtc {

class CriticalSectionWrapper;
class EventTimerWrapper;

class VideoRenderIosGles20 {
 public:
  VideoRenderIosGles20(VideoRenderIosView* view,
                       bool full_screen,
                       int render_id);
  virtual ~VideoRenderIosGles20();

  int Init();
  VideoRenderIosChannel* CreateEaglChannel(int channel,
                                           int z_order,
                                           float left,
                                           float top,
                                           float right,
                                           float bottom);
  int DeleteEaglChannel(int channel);
  bool HasChannel(int channel);
  bool ScreenUpdateProcess();
  int GetWindowRect(Rect& rect);  // NOLINT

  int GetScreenResolution(uint& screen_width, uint& screen_height);  // NOLINT
  int SetStreamCropping(const uint stream_id,
                        const float left,
                        const float top,
                        const float right,
                        const float bottom);

  int ChangeWindow(void* new_window);
  int StartRender();
  int StopRender();

 protected:
  static bool ScreenUpdateThreadProc(void* obj);

 private:
  bool RenderOffScreenBuffers();
  int SwapAndDisplayBuffers();

 private:
  rtc::scoped_ptr<CriticalSectionWrapper> gles_crit_sec_;
  EventTimerWrapper* screen_update_event_;
  // TODO(pbos): Remove scoped_ptr and use member directly.
  rtc::scoped_ptr<rtc::PlatformThread> screen_update_thread_;

  VideoRenderIosView* view_;
  Rect window_rect_;
  int window_width_;
  int window_height_;
  bool is_full_screen_;
  GLint backing_width_;
  GLint backing_height_;
  GLuint view_renderbuffer_;
  GLuint view_framebuffer_;
  GLuint depth_renderbuffer_;
  std::map<int, VideoRenderIosChannel*> agl_channels_;
  std::multimap<int, int> z_order_to_channel_;
  EAGLContext* gles_context_;
  bool is_rendering_;
};
}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_RENDER_IOS_VIDEO_RENDER_IOS_GLES20_H_
