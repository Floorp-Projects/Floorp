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

#include "webrtc/modules/video_render/ios/video_render_ios_impl.h"
#include "webrtc/modules/video_render/ios/video_render_ios_gles20.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/trace.h"

using namespace webrtc;

#define IOS_UNSUPPORTED()                                  \
  WEBRTC_TRACE(kTraceError,                                \
               kTraceVideoRenderer,                        \
               id_,                                        \
               "%s is not supported on the iOS platform.", \
               __FUNCTION__);                              \
  return -1;

VideoRenderIosImpl::VideoRenderIosImpl(const int32_t id,
                                       void* window,
                                       const bool full_screen)
    : id_(id),
      ptr_window_(window),
      full_screen_(full_screen),
      crit_sec_(CriticalSectionWrapper::CreateCriticalSection()) {}

VideoRenderIosImpl::~VideoRenderIosImpl() {
  delete crit_sec_;
}

int32_t VideoRenderIosImpl::Init() {
  CriticalSectionScoped cs(crit_sec_);

  ptr_ios_render_.reset(new VideoRenderIosGles20(
      (__bridge VideoRenderIosView*)ptr_window_, full_screen_, id_));

  return ptr_ios_render_->Init();
  ;
}

int32_t VideoRenderIosImpl::ChangeWindow(void* window) {
  CriticalSectionScoped cs(crit_sec_);
  if (window == NULL) {
    return -1;
  }

  ptr_window_ = window;

  return ptr_ios_render_->ChangeWindow(ptr_window_);
}

VideoRenderCallback* VideoRenderIosImpl::AddIncomingRenderStream(
    const uint32_t stream_id,
    const uint32_t z_order,
    const float left,
    const float top,
    const float right,
    const float bottom) {
  CriticalSectionScoped cs(crit_sec_);
  if (!ptr_window_) {
    return NULL;
  }

  return ptr_ios_render_->CreateEaglChannel(
      stream_id, z_order, left, top, right, bottom);
}

int32_t VideoRenderIosImpl::DeleteIncomingRenderStream(
    const uint32_t stream_id) {
  CriticalSectionScoped cs(crit_sec_);

  return ptr_ios_render_->DeleteEaglChannel(stream_id);
}

int32_t VideoRenderIosImpl::GetIncomingRenderStreamProperties(
    const uint32_t stream_id,
    uint32_t& z_order,
    float& left,
    float& top,
    float& right,
    float& bottom) const {
  IOS_UNSUPPORTED();
}

int32_t VideoRenderIosImpl::StartRender() {
  return ptr_ios_render_->StartRender();
}

int32_t VideoRenderIosImpl::StopRender() {
  return ptr_ios_render_->StopRender();
}

VideoRenderType VideoRenderIosImpl::RenderType() { return kRenderiOS; }

RawVideoType VideoRenderIosImpl::PerferedVideoType() { return kVideoI420; }

bool VideoRenderIosImpl::FullScreen() { IOS_UNSUPPORTED(); }

int32_t VideoRenderIosImpl::GetGraphicsMemory(
    uint64_t& totalGraphicsMemory,
    uint64_t& availableGraphicsMemory) const {
  IOS_UNSUPPORTED();
}

int32_t VideoRenderIosImpl::GetScreenResolution(uint32_t& screenWidth,
                                                uint32_t& screenHeight) const {
  return ptr_ios_render_->GetScreenResolution(screenWidth, screenHeight);
}

uint32_t VideoRenderIosImpl::RenderFrameRate(const uint32_t streamId) {
  IOS_UNSUPPORTED();
}

int32_t VideoRenderIosImpl::SetStreamCropping(const uint32_t streamId,
                                              const float left,
                                              const float top,
                                              const float right,
                                              const float bottom) {
  return ptr_ios_render_->SetStreamCropping(streamId, left, top, right, bottom);
}

int32_t VideoRenderIosImpl::ConfigureRenderer(const uint32_t streamId,
                                              const unsigned int zOrder,
                                              const float left,
                                              const float top,
                                              const float right,
                                              const float bottom) {
  IOS_UNSUPPORTED();
}

int32_t VideoRenderIosImpl::SetTransparentBackground(const bool enable) {
  IOS_UNSUPPORTED();
}

int32_t VideoRenderIosImpl::SetText(const uint8_t textId,
                                    const uint8_t* text,
                                    const int32_t textLength,
                                    const uint32_t textColorRef,
                                    const uint32_t backgroundColorRef,
                                    const float left,
                                    const float top,
                                    const float right,
                                    const float bottom) {
  IOS_UNSUPPORTED();
}

int32_t VideoRenderIosImpl::SetBitmap(const void* bitMap,
                                      const uint8_t pictureId,
                                      const void* colorKey,
                                      const float left,
                                      const float top,
                                      const float right,
                                      const float bottom) {
  IOS_UNSUPPORTED();
}

int32_t VideoRenderIosImpl::FullScreenRender(void* window, const bool enable) {
  IOS_UNSUPPORTED();
}
