/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#include "webrtc/modules/video_render/ios/video_render_ios_gles20.h"
#include "webrtc/system_wrappers/include/critical_section_wrapper.h"
#include "webrtc/system_wrappers/include/event_wrapper.h"

using namespace webrtc;

VideoRenderIosGles20::VideoRenderIosGles20(VideoRenderIosView* view,
                                           bool full_screen,
                                           int render_id)
    : gles_crit_sec_(CriticalSectionWrapper::CreateCriticalSection()),
      screen_update_event_(0),
      view_(view),
      window_rect_(),
      window_width_(0),
      window_height_(0),
      is_full_screen_(full_screen),
      agl_channels_(),
      z_order_to_channel_(),
      gles_context_([view context]),
      is_rendering_(true) {
  screen_update_thread_.reset(new rtc::PlatformThread(
      ScreenUpdateThreadProc, this, "ScreenUpdateGles20"));
  screen_update_event_ = EventTimerWrapper::Create();
  GetWindowRect(window_rect_);
}

VideoRenderIosGles20::~VideoRenderIosGles20() {
  // Signal event to exit thread, then delete it
  rtc::PlatformThread* thread_wrapper = screen_update_thread_.release();

  if (thread_wrapper) {
    screen_update_event_->Set();
    screen_update_event_->StopTimer();

    thread_wrapper->Stop();
    delete thread_wrapper;
    delete screen_update_event_;
    screen_update_event_ = NULL;
    is_rendering_ = FALSE;
  }

  // Delete all channels
  std::map<int, VideoRenderIosChannel*>::iterator it = agl_channels_.begin();
  while (it != agl_channels_.end()) {
    delete it->second;
    agl_channels_.erase(it);
    it = agl_channels_.begin();
  }
  agl_channels_.clear();

  // Clean the zOrder map
  std::multimap<int, int>::iterator z_it = z_order_to_channel_.begin();
  while (z_it != z_order_to_channel_.end()) {
    z_order_to_channel_.erase(z_it);
    z_it = z_order_to_channel_.begin();
  }
  z_order_to_channel_.clear();
}

int VideoRenderIosGles20::Init() {
  CriticalSectionScoped cs(gles_crit_sec_.get());

  if (!view_) {
    view_ = [[VideoRenderIosView alloc] init];
  }

  if (![view_ createContext]) {
    return -1;
  }

  screen_update_thread_->Start();
  screen_update_thread_->SetPriority(rtc::kRealtimePriority);

  // Start the event triggering the render process
  unsigned int monitor_freq = 60;
  screen_update_event_->StartTimer(true, 1000 / monitor_freq);

  window_width_ = window_rect_.right - window_rect_.left;
  window_height_ = window_rect_.bottom - window_rect_.top;

  return 0;
}

VideoRenderIosChannel* VideoRenderIosGles20::CreateEaglChannel(int channel,
                                                               int z_order,
                                                               float left,
                                                               float top,
                                                               float right,
                                                               float bottom) {
  CriticalSectionScoped cs(gles_crit_sec_.get());

  if (HasChannel(channel)) {
    return NULL;
  }

  VideoRenderIosChannel* new_eagl_channel = new VideoRenderIosChannel(view_);

  if (new_eagl_channel->SetStreamSettings(z_order, left, top, right, bottom) ==
      -1) {
    return NULL;
  }

  agl_channels_[channel] = new_eagl_channel;
  z_order_to_channel_.insert(std::pair<int, int>(z_order, channel));

  return new_eagl_channel;
}

int VideoRenderIosGles20::DeleteEaglChannel(int channel) {
  CriticalSectionScoped cs(gles_crit_sec_.get());

  std::map<int, VideoRenderIosChannel*>::iterator it;
  it = agl_channels_.find(channel);
  if (it != agl_channels_.end()) {
    delete it->second;
    agl_channels_.erase(it);
  } else {
    return -1;
  }

  std::multimap<int, int>::iterator z_it = z_order_to_channel_.begin();
  while (z_it != z_order_to_channel_.end()) {
    if (z_it->second == channel) {
      z_order_to_channel_.erase(z_it);
      break;
    }
    z_it++;
  }

  return 0;
}

bool VideoRenderIosGles20::HasChannel(int channel) {
  CriticalSectionScoped cs(gles_crit_sec_.get());

  std::map<int, VideoRenderIosChannel*>::iterator it =
      agl_channels_.find(channel);

  if (it != agl_channels_.end()) {
    return true;
  }

  return false;
}

// Rendering process
bool VideoRenderIosGles20::ScreenUpdateThreadProc(void* obj) {
  return static_cast<VideoRenderIosGles20*>(obj)->ScreenUpdateProcess();
}

bool VideoRenderIosGles20::ScreenUpdateProcess() {
  screen_update_event_->Wait(100);

  CriticalSectionScoped cs(gles_crit_sec_.get());

  if (!is_rendering_) {
    return false;
  }

  if (!screen_update_thread_) {
    return false;
  }

  if (GetWindowRect(window_rect_) == -1) {
    return true;
  }

  if (window_width_ != (window_rect_.right - window_rect_.left) ||
      window_height_ != (window_rect_.bottom - window_rect_.top)) {
    window_width_ = window_rect_.right - window_rect_.left;
    window_height_ = window_rect_.bottom - window_rect_.top;
  }

  // Check if there are any updated buffers
  bool updated = false;

  std::map<int, VideoRenderIosChannel*>::iterator it = agl_channels_.begin();
  while (it != agl_channels_.end()) {
    VideoRenderIosChannel* agl_channel = it->second;

    updated = agl_channel->IsUpdated();
    if (updated) {
      break;
    }
    it++;
  }

  if (updated) {
    // At least one buffer has been updated, we need to repaint the texture
    // Loop through all channels starting highest zOrder ending with lowest.
    for (std::multimap<int, int>::reverse_iterator r_it =
             z_order_to_channel_.rbegin();
         r_it != z_order_to_channel_.rend();
         r_it++) {
      int channel_id = r_it->second;
      std::map<int, VideoRenderIosChannel*>::iterator it =
          agl_channels_.find(channel_id);

      VideoRenderIosChannel* agl_channel = it->second;

      agl_channel->RenderOffScreenBuffer();
    }

    [view_ presentFramebuffer];
  }

  return true;
}

int VideoRenderIosGles20::GetWindowRect(Rect& rect) {
  CriticalSectionScoped cs(gles_crit_sec_.get());

  if (!view_) {
    return -1;
  }

  CGRect bounds = [view_ bounds];
  rect.top = bounds.origin.y;
  rect.left = bounds.origin.x;
  rect.bottom = bounds.size.height + bounds.origin.y;
  rect.right = bounds.size.width + bounds.origin.x;

  return 0;
}

int VideoRenderIosGles20::ChangeWindow(void* new_window) {
  CriticalSectionScoped cs(gles_crit_sec_.get());

  view_ = (__bridge VideoRenderIosView*)new_window;

  return 0;
}

int VideoRenderIosGles20::StartRender() {
  is_rendering_ = true;
  return 0;
}

int VideoRenderIosGles20::StopRender() {
  is_rendering_ = false;
  return 0;
}

int VideoRenderIosGles20::GetScreenResolution(uint& screen_width,
                                              uint& screen_height) {
  screen_width = [view_ bounds].size.width;
  screen_height = [view_ bounds].size.height;
  return 0;
}

int VideoRenderIosGles20::SetStreamCropping(const uint stream_id,
                                            const float left,
                                            const float top,
                                            const float right,
                                            const float bottom) {
  // Check if there are any updated buffers
  // bool updated = false;
  uint counter = 0;

  std::map<int, VideoRenderIosChannel*>::iterator it = agl_channels_.begin();
  while (it != agl_channels_.end()) {
    if (counter == stream_id) {
      VideoRenderIosChannel* agl_channel = it->second;
      agl_channel->SetStreamSettings(0, left, top, right, bottom);
    }
    counter++;
    it++;
  }

  return 0;
}
