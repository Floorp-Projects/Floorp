/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/desktop_capture/win/screen_capturer_win_gdi.h"

#include <assert.h>

#include "webrtc/modules/desktop_capture/desktop_capture_options.h"
#include "webrtc/modules/desktop_capture/desktop_frame.h"
#include "webrtc/modules/desktop_capture/desktop_frame_win.h"
#include "webrtc/modules/desktop_capture/desktop_region.h"
#include "webrtc/modules/desktop_capture/differ.h"
#include "webrtc/modules/desktop_capture/mouse_cursor.h"
#include "webrtc/modules/desktop_capture/win/cursor.h"
#include "webrtc/modules/desktop_capture/win/desktop.h"
#include "webrtc/modules/desktop_capture/win/screen_capture_utils.h"
#include "webrtc/system_wrappers/interface/logging.h"
#include "webrtc/system_wrappers/interface/tick_util.h"

namespace webrtc {

namespace {

// Constants from dwmapi.h.
const UINT DWM_EC_DISABLECOMPOSITION = 0;
const UINT DWM_EC_ENABLECOMPOSITION = 1;

const wchar_t kDwmapiLibraryName[] = L"dwmapi.dll";

}  // namespace

ScreenCapturerWinGdi::ScreenCapturerWinGdi(const DesktopCaptureOptions& options)
    : callback_(NULL),
      current_screen_id_(kFullDesktopScreenId),
      desktop_dc_(NULL),
      memory_dc_(NULL),
      dwmapi_library_(NULL),
      composition_func_(NULL),
      set_thread_execution_state_failed_(false) {
  if (options.disable_effects()) {
    // Load dwmapi.dll dynamically since it is not available on XP.
    if (!dwmapi_library_)
      dwmapi_library_ = LoadLibrary(kDwmapiLibraryName);

    if (dwmapi_library_) {
      composition_func_ = reinterpret_cast<DwmEnableCompositionFunc>(
          GetProcAddress(dwmapi_library_, "DwmEnableComposition"));
    }
  }
}

ScreenCapturerWinGdi::~ScreenCapturerWinGdi() {
  if (desktop_dc_)
    ReleaseDC(NULL, desktop_dc_);
  if (memory_dc_)
    DeleteDC(memory_dc_);

  // Restore Aero.
  if (composition_func_)
    (*composition_func_)(DWM_EC_ENABLECOMPOSITION);

  if (dwmapi_library_)
    FreeLibrary(dwmapi_library_);
}

void ScreenCapturerWinGdi::Capture(const DesktopRegion& region) {
  TickTime capture_start_time = TickTime::Now();

  queue_.MoveToNextFrame();

  // Request that the system not power-down the system, or the display hardware.
  if (!SetThreadExecutionState(ES_DISPLAY_REQUIRED | ES_SYSTEM_REQUIRED)) {
    if (!set_thread_execution_state_failed_) {
      set_thread_execution_state_failed_ = true;
      LOG_F(LS_WARNING) << "Failed to make system & display power assertion: "
                        << GetLastError();
    }
  }

  // Make sure the GDI capture resources are up-to-date.
  PrepareCaptureResources();

  if (!CaptureImage()) {
    callback_->OnCaptureCompleted(NULL);
    return;
  }

  const DesktopFrame* current_frame = queue_.current_frame();
  const DesktopFrame* last_frame = queue_.previous_frame();
  if (last_frame && last_frame->size().equals(current_frame->size())) {
    // Make sure the differencer is set up correctly for these previous and
    // current screens.
    if (!differ_.get() ||
        (differ_->width() != current_frame->size().width()) ||
        (differ_->height() != current_frame->size().height()) ||
        (differ_->bytes_per_row() != current_frame->stride())) {
      differ_.reset(new Differ(current_frame->size().width(),
                               current_frame->size().height(),
                               DesktopFrame::kBytesPerPixel,
                               current_frame->stride()));
    }

    // Calculate difference between the two last captured frames.
    DesktopRegion region;
    differ_->CalcDirtyRegion(last_frame->data(), current_frame->data(),
                             &region);
    helper_.InvalidateRegion(region);
  } else {
    // No previous frame is available, or the screen is resized. Invalidate the
    // whole screen.
    helper_.InvalidateScreen(current_frame->size());
  }

  helper_.set_size_most_recent(current_frame->size());

  // Emit the current frame.
  DesktopFrame* frame = queue_.current_frame()->Share();
  frame->set_dpi(DesktopVector(
      GetDeviceCaps(desktop_dc_, LOGPIXELSX),
      GetDeviceCaps(desktop_dc_, LOGPIXELSY)));
  frame->mutable_updated_region()->Clear();
  helper_.TakeInvalidRegion(frame->mutable_updated_region());
  frame->set_capture_time_ms(
      (TickTime::Now() - capture_start_time).Milliseconds());
  callback_->OnCaptureCompleted(frame);
}

bool ScreenCapturerWinGdi::GetScreenList(ScreenList* screens) {
  return webrtc::GetScreenList(screens);
}

bool ScreenCapturerWinGdi::SelectScreen(ScreenId id) {
  bool valid = IsScreenValid(id, &current_device_key_);
  if (valid)
    current_screen_id_ = id;
  return valid;
}

void ScreenCapturerWinGdi::Start(Callback* callback) {
  assert(!callback_);
  assert(callback);

  callback_ = callback;

  // Vote to disable Aero composited desktop effects while capturing. Windows
  // will restore Aero automatically if the process exits. This has no effect
  // under Windows 8 or higher.  See crbug.com/124018.
  if (composition_func_)
    (*composition_func_)(DWM_EC_DISABLECOMPOSITION);
}

void ScreenCapturerWinGdi::PrepareCaptureResources() {
  // Switch to the desktop receiving user input if different from the current
  // one.
  rtc::scoped_ptr<Desktop> input_desktop(Desktop::GetInputDesktop());
  if (input_desktop.get() != NULL && !desktop_.IsSame(*input_desktop)) {
    // Release GDI resources otherwise SetThreadDesktop will fail.
    if (desktop_dc_) {
      ReleaseDC(NULL, desktop_dc_);
      desktop_dc_ = NULL;
    }

    if (memory_dc_) {
      DeleteDC(memory_dc_);
      memory_dc_ = NULL;
    }

    // If SetThreadDesktop() fails, the thread is still assigned a desktop.
    // So we can continue capture screen bits, just from the wrong desktop.
    desktop_.SetThreadDesktop(input_desktop.release());

    // Re-assert our vote to disable Aero.
    // See crbug.com/124018 and crbug.com/129906.
    if (composition_func_ != NULL) {
      (*composition_func_)(DWM_EC_DISABLECOMPOSITION);
    }
  }

  // If the display bounds have changed then recreate GDI resources.
  // TODO(wez): Also check for pixel format changes.
  DesktopRect screen_rect(DesktopRect::MakeXYWH(
      GetSystemMetrics(SM_XVIRTUALSCREEN),
      GetSystemMetrics(SM_YVIRTUALSCREEN),
      GetSystemMetrics(SM_CXVIRTUALSCREEN),
      GetSystemMetrics(SM_CYVIRTUALSCREEN)));
  if (!screen_rect.equals(desktop_dc_rect_)) {
    if (desktop_dc_) {
      ReleaseDC(NULL, desktop_dc_);
      desktop_dc_ = NULL;
    }
    if (memory_dc_) {
      DeleteDC(memory_dc_);
      memory_dc_ = NULL;
    }
    desktop_dc_rect_ = DesktopRect();
  }

  if (desktop_dc_ == NULL) {
    assert(memory_dc_ == NULL);

    // Create GDI device contexts to capture from the desktop into memory.
    desktop_dc_ = GetDC(NULL);
    if (!desktop_dc_)
      abort();
    memory_dc_ = CreateCompatibleDC(desktop_dc_);
    if (!memory_dc_)
      abort();

    desktop_dc_rect_ = screen_rect;

    // Make sure the frame buffers will be reallocated.
    queue_.Reset();

    helper_.ClearInvalidRegion();
  }
}

bool ScreenCapturerWinGdi::CaptureImage() {
  DesktopRect screen_rect =
      GetScreenRect(current_screen_id_, current_device_key_);
  if (screen_rect.is_empty())
    return false;

  DesktopSize size = screen_rect.size();
  // If the current buffer is from an older generation then allocate a new one.
  // Note that we can't reallocate other buffers at this point, since the caller
  // may still be reading from them.
  if (!queue_.current_frame() ||
      !queue_.current_frame()->size().equals(screen_rect.size())) {
    assert(desktop_dc_ != NULL);
    assert(memory_dc_ != NULL);

    size_t buffer_size = size.width() * size.height() *
        DesktopFrame::kBytesPerPixel;
    SharedMemory* shared_memory = callback_->CreateSharedMemory(buffer_size);

    rtc::scoped_ptr<DesktopFrame> buffer;
    buffer.reset(
        DesktopFrameWin::Create(size, shared_memory, desktop_dc_));
    queue_.ReplaceCurrentFrame(buffer.release());
  }

  // Select the target bitmap into the memory dc and copy the rect from desktop
  // to memory.
  DesktopFrameWin* current = static_cast<DesktopFrameWin*>(
      queue_.current_frame()->GetUnderlyingFrame());
  HGDIOBJ previous_object = SelectObject(memory_dc_, current->bitmap());
  if (previous_object != NULL) {
    BitBlt(memory_dc_,
           0, 0, screen_rect.width(), screen_rect.height(),
           desktop_dc_,
           screen_rect.left(), screen_rect.top(),
           SRCCOPY | CAPTUREBLT);

    // Select back the previously selected object to that the device contect
    // could be destroyed independently of the bitmap if needed.
    SelectObject(memory_dc_, previous_object);
  }
  return true;
}

}  // namespace webrtc
