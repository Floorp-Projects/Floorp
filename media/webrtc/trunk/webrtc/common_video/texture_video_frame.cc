/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/common_video/interface/texture_video_frame.h"

#include <assert.h>

#include "webrtc/system_wrappers/interface/trace.h"

#define NOTREACHED() \
  do { \
    WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, -1, "Not reached"); \
    assert(false); \
  } while (0)

namespace webrtc {

TextureVideoFrame::TextureVideoFrame(NativeHandle* handle,
                                     int width,
                                     int height,
                                     uint32_t timestamp,
                                     int64_t render_time_ms)
    : handle_(handle) {
  set_width(width);
  set_height(height);
  set_timestamp(timestamp);
  set_render_time_ms(render_time_ms);
}

TextureVideoFrame::~TextureVideoFrame() {}

int TextureVideoFrame::CreateEmptyFrame(int width,
                                        int height,
                                        int stride_y,
                                        int stride_u,
                                        int stride_v) {
  NOTREACHED();
  return -1;
}

int TextureVideoFrame::CreateFrame(int size_y,
                                   const uint8_t* buffer_y,
                                   int size_u,
                                   const uint8_t* buffer_u,
                                   int size_v,
                                   const uint8_t* buffer_v,
                                   int width,
                                   int height,
                                   int stride_y,
                                   int stride_u,
                                   int stride_v) {
  NOTREACHED();
  return -1;
}

int TextureVideoFrame::CopyFrame(const I420VideoFrame& videoFrame) {
  NOTREACHED();
  return -1;
}

void TextureVideoFrame::SwapFrame(I420VideoFrame* videoFrame) {
  NOTREACHED();
}

uint8_t* TextureVideoFrame::buffer(PlaneType type) {
  NOTREACHED();
  return NULL;
}

const uint8_t* TextureVideoFrame::buffer(PlaneType type) const {
  NOTREACHED();
  return NULL;
}

int TextureVideoFrame::allocated_size(PlaneType type) const {
  NOTREACHED();
  return -1;
}

int TextureVideoFrame::stride(PlaneType type) const {
  NOTREACHED();
  return -1;
}

bool TextureVideoFrame::IsZeroSize() const {
  NOTREACHED();
  return true;
}

void TextureVideoFrame::ResetSize() {
  NOTREACHED();
}

void* TextureVideoFrame::native_handle() const { return handle_.get(); }

int TextureVideoFrame::CheckDimensions(
    int width, int height, int stride_y, int stride_u, int stride_v) {
  return 0;
}

}  // namespace webrtc
