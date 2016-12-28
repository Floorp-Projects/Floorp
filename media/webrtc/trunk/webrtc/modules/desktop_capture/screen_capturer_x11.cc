/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/desktop_capture/screen_capturer.h"

#include <string.h>
#include <set>

#include <X11/extensions/Xdamage.h>
#include <X11/extensions/Xfixes.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "webrtc/base/checks.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/modules/desktop_capture/desktop_capture_options.h"
#include "webrtc/modules/desktop_capture/desktop_frame.h"
#include "webrtc/modules/desktop_capture/differ.h"
#include "webrtc/modules/desktop_capture/screen_capture_frame_queue.h"
#include "webrtc/modules/desktop_capture/screen_capturer_helper.h"
#include "webrtc/modules/desktop_capture/x11/x_server_pixel_buffer.h"
#include "webrtc/system_wrappers/include/logging.h"
#include "webrtc/system_wrappers/include/tick_util.h"

namespace webrtc {
namespace {

// A class to perform video frame capturing for Linux.
class ScreenCapturerLinux : public ScreenCapturer,
                            public SharedXDisplay::XEventHandler {
 public:
  ScreenCapturerLinux();
  virtual ~ScreenCapturerLinux();

  // TODO(ajwong): Do we really want this to be synchronous?
  bool Init(const DesktopCaptureOptions& options);

  // DesktopCapturer interface.
  void Start(Callback* delegate) override;
  void Capture(const DesktopRegion& region) override;

  // ScreenCapturer interface.
  bool GetScreenList(ScreenList* screens) override;
  bool SelectScreen(ScreenId id) override;

 private:
  Display* display() { return options_.x_display()->display(); }

  // SharedXDisplay::XEventHandler interface.
  bool HandleXEvent(const XEvent& event) override;

  void InitXDamage();

  // Capture screen pixels to the current buffer in the queue. In the DAMAGE
  // case, the ScreenCapturerHelper already holds the list of invalid rectangles
  // from HandleXEvent(). In the non-DAMAGE case, this captures the
  // whole screen, then calculates some invalid rectangles that include any
  // differences between this and the previous capture.
  DesktopFrame* CaptureScreen();

  // Called when the screen configuration is changed.
  void ScreenConfigurationChanged();

  // Synchronize the current buffer with |last_buffer_|, by copying pixels from
  // the area of |last_invalid_rects|.
  // Note this only works on the assumption that kNumBuffers == 2, as
  // |last_invalid_rects| holds the differences from the previous buffer and
  // the one prior to that (which will then be the current buffer).
  void SynchronizeFrame();

  void DeinitXlib();

  DesktopCaptureOptions options_;

  Callback* callback_;

  // X11 graphics context.
  GC gc_;
  Window root_window_;

  // XFixes.
  bool has_xfixes_;
  int xfixes_event_base_;
  int xfixes_error_base_;

  // XDamage information.
  bool use_damage_;
  Damage damage_handle_;
  int damage_event_base_;
  int damage_error_base_;
  XserverRegion damage_region_;

  // Access to the X Server's pixel buffer.
  XServerPixelBuffer x_server_pixel_buffer_;

  // A thread-safe list of invalid rectangles, and the size of the most
  // recently captured screen.
  ScreenCapturerHelper helper_;

  // Queue of the frames buffers.
  ScreenCaptureFrameQueue queue_;

  // Invalid region from the previous capture. This is used to synchronize the
  // current with the last buffer used.
  DesktopRegion last_invalid_region_;

  // |Differ| for use when polling for changes.
  rtc::scoped_ptr<Differ> differ_;

  RTC_DISALLOW_COPY_AND_ASSIGN(ScreenCapturerLinux);
};

ScreenCapturerLinux::ScreenCapturerLinux()
    : callback_(NULL),
      gc_(NULL),
      root_window_(BadValue),
      has_xfixes_(false),
      xfixes_event_base_(-1),
      xfixes_error_base_(-1),
      use_damage_(false),
      damage_handle_(0),
      damage_event_base_(-1),
      damage_error_base_(-1),
      damage_region_(0) {
  helper_.SetLogGridSize(4);
}

ScreenCapturerLinux::~ScreenCapturerLinux() {
  options_.x_display()->RemoveEventHandler(ConfigureNotify, this);
  if (use_damage_) {
    options_.x_display()->RemoveEventHandler(
        damage_event_base_ + XDamageNotify, this);
  }
  DeinitXlib();
}

bool ScreenCapturerLinux::Init(const DesktopCaptureOptions& options) {
  options_ = options;

  root_window_ = RootWindow(display(), DefaultScreen(display()));
  if (root_window_ == BadValue) {
    LOG(LS_ERROR) << "Unable to get the root window";
    DeinitXlib();
    return false;
  }

  gc_ = XCreateGC(display(), root_window_, 0, NULL);
  if (gc_ == NULL) {
    LOG(LS_ERROR) << "Unable to get graphics context";
    DeinitXlib();
    return false;
  }

  options_.x_display()->AddEventHandler(ConfigureNotify, this);

  // Check for XFixes extension. This is required for cursor shape
  // notifications, and for our use of XDamage.
  if (XFixesQueryExtension(display(), &xfixes_event_base_,
                           &xfixes_error_base_)) {
    has_xfixes_ = true;
  } else {
    LOG(LS_INFO) << "X server does not support XFixes.";
  }

  // Register for changes to the dimensions of the root window.
  XSelectInput(display(), root_window_, StructureNotifyMask);

  if (!x_server_pixel_buffer_.Init(display(), DefaultRootWindow(display()))) {
    LOG(LS_ERROR) << "Failed to initialize pixel buffer.";
    return false;
  }

  if (options_.use_update_notifications()) {
    InitXDamage();
  }

  return true;
}

void ScreenCapturerLinux::InitXDamage() {
  // Our use of XDamage requires XFixes.
  if (!has_xfixes_) {
    return;
  }

  // Check for XDamage extension.
  if (!XDamageQueryExtension(display(), &damage_event_base_,
                             &damage_error_base_)) {
    LOG(LS_INFO) << "X server does not support XDamage.";
    return;
  }

  // TODO(lambroslambrou): Disable DAMAGE in situations where it is known
  // to fail, such as when Desktop Effects are enabled, with graphics
  // drivers (nVidia, ATI) that fail to report DAMAGE notifications
  // properly.

  // Request notifications every time the screen becomes damaged.
  damage_handle_ = XDamageCreate(display(), root_window_,
                                 XDamageReportNonEmpty);
  if (!damage_handle_) {
    LOG(LS_ERROR) << "Unable to initialize XDamage.";
    return;
  }

  // Create an XFixes server-side region to collate damage into.
  damage_region_ = XFixesCreateRegion(display(), 0, 0);
  if (!damage_region_) {
    XDamageDestroy(display(), damage_handle_);
    LOG(LS_ERROR) << "Unable to create XFixes region.";
    return;
  }

  options_.x_display()->AddEventHandler(
      damage_event_base_ + XDamageNotify, this);

  use_damage_ = true;
  LOG(LS_INFO) << "Using XDamage extension.";
}

void ScreenCapturerLinux::Start(Callback* callback) {
  RTC_DCHECK(!callback_);
  RTC_DCHECK(callback);

  callback_ = callback;
}

void ScreenCapturerLinux::Capture(const DesktopRegion& region) {
  TickTime capture_start_time = TickTime::Now();

  queue_.MoveToNextFrame();

  // Process XEvents for XDamage and cursor shape tracking.
  options_.x_display()->ProcessPendingXEvents();

  // ProcessPendingXEvents() may call ScreenConfigurationChanged() which
  // reinitializes |x_server_pixel_buffer_|. Check if the pixel buffer is still
  // in a good shape.
  if (!x_server_pixel_buffer_.is_initialized()) {
     // We failed to initialize pixel buffer.
     callback_->OnCaptureCompleted(NULL);
     return;
  }

  // If the current frame is from an older generation then allocate a new one.
  // Note that we can't reallocate other buffers at this point, since the caller
  // may still be reading from them.
  if (!queue_.current_frame()) {
    rtc::scoped_ptr<DesktopFrame> frame(
        new BasicDesktopFrame(x_server_pixel_buffer_.window_size()));
    queue_.ReplaceCurrentFrame(frame.release());
  }

  // Refresh the Differ helper used by CaptureFrame(), if needed.
  DesktopFrame* frame = queue_.current_frame();
  if (!use_damage_ && (
      !differ_.get() ||
      (differ_->width() != frame->size().width()) ||
      (differ_->height() != frame->size().height()) ||
      (differ_->bytes_per_row() != frame->stride()))) {
    differ_.reset(new Differ(frame->size().width(), frame->size().height(),
                             DesktopFrame::kBytesPerPixel,
                             frame->stride()));
  }

  DesktopFrame* result = CaptureScreen();
  last_invalid_region_ = result->updated_region();
  result->set_capture_time_ms(
      (TickTime::Now() - capture_start_time).Milliseconds());
  callback_->OnCaptureCompleted(result);
}

bool ScreenCapturerLinux::GetScreenList(ScreenList* screens) {
  RTC_DCHECK(screens->size() == 0);
  // TODO(jiayl): implement screen enumeration.
  Screen default_screen;
  default_screen.id = 0;
  screens->push_back(default_screen);
  return true;
}

bool ScreenCapturerLinux::SelectScreen(ScreenId id) {
  // TODO(jiayl): implement screen selection.
  return true;
}

bool ScreenCapturerLinux::HandleXEvent(const XEvent& event) {
  if (use_damage_ && (event.type == damage_event_base_ + XDamageNotify)) {
    const XDamageNotifyEvent* damage_event =
        reinterpret_cast<const XDamageNotifyEvent*>(&event);
    if (damage_event->damage != damage_handle_)
      return false;
    RTC_DCHECK(damage_event->level == XDamageReportNonEmpty);
    return true;
  } else if (event.type == ConfigureNotify) {
    ScreenConfigurationChanged();
    return true;
  }
  return false;
}

DesktopFrame* ScreenCapturerLinux::CaptureScreen() {
  DesktopFrame* frame = queue_.current_frame()->Share();
  assert(x_server_pixel_buffer_.window_size().equals(frame->size()));

  // Pass the screen size to the helper, so it can clip the invalid region if it
  // expands that region to a grid.
  helper_.set_size_most_recent(frame->size());

  // In the DAMAGE case, ensure the frame is up-to-date with the previous frame
  // if any.  If there isn't a previous frame, that means a screen-resolution
  // change occurred, and |invalid_rects| will be updated to include the whole
  // screen.
  if (use_damage_ && queue_.previous_frame())
    SynchronizeFrame();

  DesktopRegion* updated_region = frame->mutable_updated_region();

  x_server_pixel_buffer_.Synchronize();
  if (use_damage_ && queue_.previous_frame()) {
    // Atomically fetch and clear the damage region.
    XDamageSubtract(display(), damage_handle_, None, damage_region_);
    int rects_num = 0;
    XRectangle bounds;
    XRectangle* rects = XFixesFetchRegionAndBounds(display(), damage_region_,
                                                   &rects_num, &bounds);
    for (int i = 0; i < rects_num; ++i) {
      updated_region->AddRect(DesktopRect::MakeXYWH(
          rects[i].x, rects[i].y, rects[i].width, rects[i].height));
    }
    XFree(rects);
    helper_.InvalidateRegion(*updated_region);

    // Capture the damaged portions of the desktop.
    helper_.TakeInvalidRegion(updated_region);

    // Clip the damaged portions to the current screen size, just in case some
    // spurious XDamage notifications were received for a previous (larger)
    // screen size.
    updated_region->IntersectWith(
        DesktopRect::MakeSize(x_server_pixel_buffer_.window_size()));

    for (DesktopRegion::Iterator it(*updated_region);
         !it.IsAtEnd(); it.Advance()) {
      x_server_pixel_buffer_.CaptureRect(it.rect(), frame);
    }
  } else {
    // Doing full-screen polling, or this is the first capture after a
    // screen-resolution change.  In either case, need a full-screen capture.
    DesktopRect screen_rect = DesktopRect::MakeSize(frame->size());
    x_server_pixel_buffer_.CaptureRect(screen_rect, frame);

    if (queue_.previous_frame()) {
      // Full-screen polling, so calculate the invalid rects here, based on the
      // changed pixels between current and previous buffers.
      RTC_DCHECK(differ_.get() != NULL);
      RTC_DCHECK(queue_.previous_frame()->data());
      differ_->CalcDirtyRegion(queue_.previous_frame()->data(),
                               frame->data(), updated_region);
    } else {
      // No previous buffer, so always invalidate the whole screen, whether
      // or not DAMAGE is being used.  DAMAGE doesn't necessarily send a
      // full-screen notification after a screen-resolution change, so
      // this is done here.
      updated_region->SetRect(screen_rect);
    }
  }

  return frame;
}

void ScreenCapturerLinux::ScreenConfigurationChanged() {
  // Make sure the frame buffers will be reallocated.
  queue_.Reset();

  helper_.ClearInvalidRegion();
  if (!x_server_pixel_buffer_.Init(display(), DefaultRootWindow(display()))) {
    LOG(LS_ERROR) << "Failed to initialize pixel buffer after screen "
        "configuration change.";
  }
}

void ScreenCapturerLinux::SynchronizeFrame() {
  // Synchronize the current buffer with the previous one since we do not
  // capture the entire desktop. Note that encoder may be reading from the
  // previous buffer at this time so thread access complaints are false
  // positives.

  // TODO(hclam): We can reduce the amount of copying here by subtracting
  // |capturer_helper_|s region from |last_invalid_region_|.
  // http://crbug.com/92354
  RTC_DCHECK(queue_.previous_frame());

  DesktopFrame* current = queue_.current_frame();
  DesktopFrame* last = queue_.previous_frame();
  RTC_DCHECK(current != last);
  for (DesktopRegion::Iterator it(last_invalid_region_);
       !it.IsAtEnd(); it.Advance()) {
    current->CopyPixelsFrom(*last, it.rect().top_left(), it.rect());
  }
}

void ScreenCapturerLinux::DeinitXlib() {
  if (gc_) {
    XFreeGC(display(), gc_);
    gc_ = NULL;
  }

  x_server_pixel_buffer_.Release();

  if (display()) {
    if (damage_handle_) {
      XDamageDestroy(display(), damage_handle_);
      damage_handle_ = 0;
    }

    if (damage_region_) {
      XFixesDestroyRegion(display(), damage_region_);
      damage_region_ = 0;
    }
  }
}

}  // namespace

// static
ScreenCapturer* ScreenCapturer::Create(const DesktopCaptureOptions& options) {
  if (!options.x_display())
    return NULL;

  rtc::scoped_ptr<ScreenCapturerLinux> capturer(new ScreenCapturerLinux());
  if (!capturer->Init(options))
    capturer.reset();
  return capturer.release();
}

}  // namespace webrtc
