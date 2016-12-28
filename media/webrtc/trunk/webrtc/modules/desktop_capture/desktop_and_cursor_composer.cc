/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/desktop_capture/desktop_and_cursor_composer.h"

#include <string.h>

#include "webrtc/modules/desktop_capture/desktop_capturer.h"
#include "webrtc/modules/desktop_capture/desktop_frame.h"
#include "webrtc/modules/desktop_capture/mouse_cursor.h"

namespace webrtc {

namespace {

// Helper function that blends one image into another. Source image must be
// pre-multiplied with the alpha channel. Destination is assumed to be opaque.
void AlphaBlend(uint8_t* dest, int dest_stride,
                const uint8_t* src, int src_stride,
                const DesktopSize& size) {
  for (int y = 0; y < size.height(); ++y) {
    for (int x = 0; x < size.width(); ++x) {
      uint32_t base_alpha = 255 - src[x * DesktopFrame::kBytesPerPixel + 3];
      if (base_alpha == 255) {
        continue;
      } else if (base_alpha == 0) {
        memcpy(dest + x * DesktopFrame::kBytesPerPixel,
               src + x * DesktopFrame::kBytesPerPixel,
               DesktopFrame::kBytesPerPixel);
      } else {
        dest[x * DesktopFrame::kBytesPerPixel] =
            dest[x * DesktopFrame::kBytesPerPixel] * base_alpha / 255 +
            src[x * DesktopFrame::kBytesPerPixel];
        dest[x * DesktopFrame::kBytesPerPixel + 1] =
            dest[x * DesktopFrame::kBytesPerPixel + 1] * base_alpha / 255 +
            src[x * DesktopFrame::kBytesPerPixel + 1];
        dest[x * DesktopFrame::kBytesPerPixel + 2] =
            dest[x * DesktopFrame::kBytesPerPixel + 2] * base_alpha / 255 +
            src[x * DesktopFrame::kBytesPerPixel + 2];
      }
    }
    src += src_stride;
    dest += dest_stride;
  }
}

// DesktopFrame wrapper that draws mouse on a frame and restores original
// content before releasing the underlying frame.
class DesktopFrameWithCursor : public DesktopFrame {
 public:
  // Takes ownership of |frame|.
  DesktopFrameWithCursor(DesktopFrame* frame,
                         const MouseCursor& cursor,
                         const DesktopVector& position);
  virtual ~DesktopFrameWithCursor();

 private:
  rtc::scoped_ptr<DesktopFrame> original_frame_;

  DesktopVector restore_position_;
  rtc::scoped_ptr<DesktopFrame> restore_frame_;

  RTC_DISALLOW_COPY_AND_ASSIGN(DesktopFrameWithCursor);
};

DesktopFrameWithCursor::DesktopFrameWithCursor(DesktopFrame* frame,
                                               const MouseCursor& cursor,
                                               const DesktopVector& position)
    : DesktopFrame(frame->size(), frame->stride(),
                   frame->data(), frame->shared_memory()),
      original_frame_(frame) {
  set_dpi(frame->dpi());
  set_capture_time_ms(frame->capture_time_ms());
  mutable_updated_region()->Swap(frame->mutable_updated_region());

  DesktopVector image_pos = position.subtract(cursor.hotspot());
  DesktopRect target_rect = DesktopRect::MakeSize(cursor.image()->size());
  target_rect.Translate(image_pos);
  DesktopVector target_origin = target_rect.top_left();
  target_rect.IntersectWith(DesktopRect::MakeSize(size()));

  if (target_rect.is_empty())
    return;

  // Copy original screen content under cursor to |restore_frame_|.
  restore_position_ = target_rect.top_left();
  restore_frame_.reset(new BasicDesktopFrame(target_rect.size()));
  restore_frame_->CopyPixelsFrom(*this, target_rect.top_left(),
                                 DesktopRect::MakeSize(restore_frame_->size()));

  // Blit the cursor.
  uint8_t* target_rect_data = reinterpret_cast<uint8_t*>(data()) +
                              target_rect.top() * stride() +
                              target_rect.left() * DesktopFrame::kBytesPerPixel;
  DesktopVector origin_shift = target_rect.top_left().subtract(target_origin);
  AlphaBlend(target_rect_data, stride(),
             cursor.image()->data() +
                 origin_shift.y() * cursor.image()->stride() +
                 origin_shift.x() * DesktopFrame::kBytesPerPixel,
             cursor.image()->stride(),
             target_rect.size());
}

DesktopFrameWithCursor::~DesktopFrameWithCursor() {
  // Restore original content of the frame.
  if (restore_frame_.get()) {
    DesktopRect target_rect = DesktopRect::MakeSize(restore_frame_->size());
    target_rect.Translate(restore_position_);
    CopyPixelsFrom(restore_frame_->data(), restore_frame_->stride(),
                   target_rect);
  }
}

}  // namespace

DesktopAndCursorComposer::DesktopAndCursorComposer(
    DesktopCapturer* desktop_capturer,
    MouseCursorMonitor* mouse_monitor)
    : desktop_capturer_(desktop_capturer),
      mouse_monitor_(mouse_monitor) {
}

DesktopAndCursorComposer::~DesktopAndCursorComposer() {}

void DesktopAndCursorComposer::Start(DesktopCapturer::Callback* callback) {
  callback_ = callback;
  if (mouse_monitor_.get())
    mouse_monitor_->Start(this, MouseCursorMonitor::SHAPE_AND_POSITION);
  desktop_capturer_->Start(this);
}

void DesktopAndCursorComposer::Stop() {
  desktop_capturer_->Stop();
  if (mouse_monitor_.get())
    mouse_monitor_->Stop();
  callback_ = NULL;
}

void DesktopAndCursorComposer::Capture(const DesktopRegion& region) {
  if (mouse_monitor_.get())
    mouse_monitor_->Capture();
  desktop_capturer_->Capture(region);
}

void DesktopAndCursorComposer::SetExcludedWindow(WindowId window) {
  desktop_capturer_->SetExcludedWindow(window);
}

SharedMemory* DesktopAndCursorComposer::CreateSharedMemory(size_t size) {
  return callback_->CreateSharedMemory(size);
}

void DesktopAndCursorComposer::OnCaptureCompleted(DesktopFrame* frame) {
  if (frame && cursor_.get() && cursor_state_ == MouseCursorMonitor::INSIDE) {
    DesktopFrameWithCursor* frame_with_cursor =
        new DesktopFrameWithCursor(frame, *cursor_, cursor_position_);
    frame = frame_with_cursor;
  }

  callback_->OnCaptureCompleted(frame);
}

void DesktopAndCursorComposer::OnMouseCursor(MouseCursor* cursor) {
  cursor_.reset(cursor);
}

void DesktopAndCursorComposer::OnMouseCursorPosition(
    MouseCursorMonitor::CursorState state,
    const DesktopVector& position) {
  cursor_state_ = state;
  cursor_position_ = position;
}

}  // namespace webrtc
