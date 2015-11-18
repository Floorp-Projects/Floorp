/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/desktop_capture/desktop_frame.h"

#include <assert.h>
#include <string.h>

namespace webrtc {

DesktopFrame::DesktopFrame(DesktopSize size,
                           int stride,
                           uint8_t* data,
                           SharedMemory* shared_memory)
    : size_(size),
      stride_(stride),
      data_(data),
      shared_memory_(shared_memory),
      capture_time_ms_(0) {
}

DesktopFrame::~DesktopFrame() {}

void DesktopFrame::CopyPixelsFrom(uint8_t* src_buffer, int src_stride,
                                  const DesktopRect& dest_rect) {
  assert(DesktopRect::MakeSize(size()).ContainsRect(dest_rect));

  uint8_t* dest = GetFrameDataAtPos(dest_rect.top_left());
  for (int y = 0; y < dest_rect.height(); ++y) {
    memcpy(dest, src_buffer, DesktopFrame::kBytesPerPixel * dest_rect.width());
    src_buffer += src_stride;
    dest += stride();
  }
}

void DesktopFrame::CopyPixelsFrom(const DesktopFrame& src_frame,
                                  const DesktopVector& src_pos,
                                  const DesktopRect& dest_rect) {
  assert(DesktopRect::MakeSize(src_frame.size()).ContainsRect(
      DesktopRect::MakeOriginSize(src_pos, dest_rect.size())));

  CopyPixelsFrom(src_frame.GetFrameDataAtPos(src_pos),
                 src_frame.stride(), dest_rect);
}

uint8_t* DesktopFrame::GetFrameDataAtPos(const DesktopVector& pos) const {
  return data() + stride() * pos.y() + DesktopFrame::kBytesPerPixel * pos.x();
}

BasicDesktopFrame::BasicDesktopFrame(DesktopSize size)
    : DesktopFrame(size, kBytesPerPixel * size.width(),
                   new uint8_t[kBytesPerPixel * size.width() * size.height()],
                   NULL) {
}

BasicDesktopFrame::~BasicDesktopFrame() {
  delete[] data_;
}

DesktopFrame* BasicDesktopFrame::CopyOf(const DesktopFrame& frame) {
  DesktopFrame* result = new BasicDesktopFrame(frame.size());
  for (int y = 0; y < frame.size().height(); ++y) {
    memcpy(result->data() + y * result->stride(),
           frame.data() + y * frame.stride(),
           frame.size().width() * kBytesPerPixel);
  }
  result->set_dpi(frame.dpi());
  result->set_capture_time_ms(frame.capture_time_ms());
  *result->mutable_updated_region() = frame.updated_region();
  return result;
}


SharedMemoryDesktopFrame::SharedMemoryDesktopFrame(
    DesktopSize size,
    int stride,
    SharedMemory* shared_memory)
    : DesktopFrame(size, stride,
                   reinterpret_cast<uint8_t*>(shared_memory->data()),
                   shared_memory) {
}

SharedMemoryDesktopFrame::~SharedMemoryDesktopFrame() {
  delete shared_memory_;
}

}  // namespace webrtc
