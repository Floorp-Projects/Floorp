/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/desktop_capture/win/screen_capturer_win_magnifier.h"

#include <assert.h>

#include <utility>

#include "webrtc/modules/desktop_capture/desktop_capture_options.h"
#include "webrtc/modules/desktop_capture/desktop_frame.h"
#include "webrtc/modules/desktop_capture/desktop_frame_win.h"
#include "webrtc/modules/desktop_capture/desktop_region.h"
#include "webrtc/modules/desktop_capture/differ.h"
#include "webrtc/modules/desktop_capture/mouse_cursor.h"
#include "webrtc/modules/desktop_capture/win/cursor.h"
#include "webrtc/modules/desktop_capture/win/desktop.h"
#include "webrtc/modules/desktop_capture/win/screen_capture_utils.h"
#include "webrtc/system_wrappers/include/logging.h"
#include "webrtc/system_wrappers/include/tick_util.h"

namespace webrtc {

// kMagnifierWindowClass has to be "Magnifier" according to the Magnification
// API. The other strings can be anything.
static LPCTSTR kMagnifierHostClass = L"ScreenCapturerWinMagnifierHost";
static LPCTSTR kHostWindowName = L"MagnifierHost";
static LPCTSTR kMagnifierWindowClass = L"Magnifier";
static LPCTSTR kMagnifierWindowName = L"MagnifierWindow";

Atomic32 ScreenCapturerWinMagnifier::tls_index_(TLS_OUT_OF_INDEXES);

ScreenCapturerWinMagnifier::ScreenCapturerWinMagnifier(
    rtc::scoped_ptr<ScreenCapturer> fallback_capturer)
    : fallback_capturer_(std::move(fallback_capturer)),
      fallback_capturer_started_(false),
      callback_(NULL),
      current_screen_id_(kFullDesktopScreenId),
      excluded_window_(NULL),
      set_thread_execution_state_failed_(false),
      desktop_dc_(NULL),
      mag_lib_handle_(NULL),
      mag_initialize_func_(NULL),
      mag_uninitialize_func_(NULL),
      set_window_source_func_(NULL),
      set_window_filter_list_func_(NULL),
      set_image_scaling_callback_func_(NULL),
      host_window_(NULL),
      magnifier_window_(NULL),
      magnifier_initialized_(false),
      magnifier_capture_succeeded_(true) {}

ScreenCapturerWinMagnifier::~ScreenCapturerWinMagnifier() {
  Stop();
}

void ScreenCapturerWinMagnifier::Start(Callback* callback) {
  assert(!callback_);
  assert(callback);
  callback_ = callback;

  InitializeMagnifier();
}

void ScreenCapturerWinMagnifier::Stop() {
  callback_ = NULL;

  // DestroyWindow must be called before MagUninitialize. magnifier_window_ is
  // destroyed automatically when host_window_ is destroyed.
  if (host_window_) {
    DestroyWindow(host_window_);
    host_window_ = NULL;
  }

  if (magnifier_initialized_) {
    mag_uninitialize_func_();
    magnifier_initialized_ = false;
  }

  if (mag_lib_handle_) {
    FreeLibrary(mag_lib_handle_);
    mag_lib_handle_ = NULL;
  }

  if (desktop_dc_) {
    ReleaseDC(NULL, desktop_dc_);
    desktop_dc_ = NULL;
  }
}

void ScreenCapturerWinMagnifier::Capture(const DesktopRegion& region) {
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
  // Switch to the desktop receiving user input if different from the current
  // one.
  rtc::scoped_ptr<Desktop> input_desktop(Desktop::GetInputDesktop());
  if (input_desktop.get() != NULL && !desktop_.IsSame(*input_desktop)) {
    // Release GDI resources otherwise SetThreadDesktop will fail.
    if (desktop_dc_) {
      ReleaseDC(NULL, desktop_dc_);
      desktop_dc_ = NULL;
    }
    // If SetThreadDesktop() fails, the thread is still assigned a desktop.
    // So we can continue capture screen bits, just from the wrong desktop.
    desktop_.SetThreadDesktop(input_desktop.release());
  }

  bool succeeded = false;

  // Do not try to use the magnifier if it failed before and in multi-screen
  // setup (where the API crashes sometimes).
  if (magnifier_initialized_ && (GetSystemMetrics(SM_CMONITORS) == 1) &&
      magnifier_capture_succeeded_) {
    DesktopRect rect = GetScreenRect(current_screen_id_, current_device_key_);
    CreateCurrentFrameIfNecessary(rect.size());

    // CaptureImage may fail in some situations, e.g. windows8 metro mode.
    succeeded = CaptureImage(rect);
  }

  // Defer to the fallback capturer if magnifier capturer did not work.
  if (!succeeded) {
    LOG_F(LS_WARNING) << "Switching to the fallback screen capturer.";
    StartFallbackCapturer();
    fallback_capturer_->Capture(region);
    return;
  }

  const DesktopFrame* current_frame = queue_.current_frame();
  const DesktopFrame* last_frame = queue_.previous_frame();
  if (last_frame && last_frame->size().equals(current_frame->size())) {
    // Make sure the differencer is set up correctly for these previous and
    // current screens.
    if (!differ_.get() || (differ_->width() != current_frame->size().width()) ||
        (differ_->height() != current_frame->size().height()) ||
        (differ_->bytes_per_row() != current_frame->stride())) {
      differ_.reset(new Differ(current_frame->size().width(),
                               current_frame->size().height(),
                               DesktopFrame::kBytesPerPixel,
                               current_frame->stride()));
    }

    // Calculate difference between the two last captured frames.
    DesktopRegion region;
    differ_->CalcDirtyRegion(
        last_frame->data(), current_frame->data(), &region);
    helper_.InvalidateRegion(region);
  } else {
    // No previous frame is available, or the screen is resized. Invalidate the
    // whole screen.
    helper_.InvalidateScreen(current_frame->size());
  }

  helper_.set_size_most_recent(current_frame->size());

  // Emit the current frame.
  DesktopFrame* frame = queue_.current_frame()->Share();
  frame->set_dpi(DesktopVector(GetDeviceCaps(desktop_dc_, LOGPIXELSX),
                               GetDeviceCaps(desktop_dc_, LOGPIXELSY)));
  frame->mutable_updated_region()->Clear();
  helper_.TakeInvalidRegion(frame->mutable_updated_region());
  frame->set_capture_time_ms(
      (TickTime::Now() - capture_start_time).Milliseconds());
  callback_->OnCaptureCompleted(frame);
}

bool ScreenCapturerWinMagnifier::GetScreenList(ScreenList* screens) {
  return webrtc::GetScreenList(screens);
}

bool ScreenCapturerWinMagnifier::SelectScreen(ScreenId id) {
  bool valid = IsScreenValid(id, &current_device_key_);

  // Set current_screen_id_ even if the fallback capturer is being used, so we
  // can switch back to the magnifier when possible.
  if (valid)
    current_screen_id_ = id;

  if (fallback_capturer_started_)
    fallback_capturer_->SelectScreen(id);

  return valid;
}

void ScreenCapturerWinMagnifier::SetExcludedWindow(WindowId excluded_window) {
  excluded_window_ = (HWND)excluded_window;
  if (excluded_window_ && magnifier_initialized_) {
    set_window_filter_list_func_(
        magnifier_window_, MW_FILTERMODE_EXCLUDE, 1, &excluded_window_);
  }
}

bool ScreenCapturerWinMagnifier::CaptureImage(const DesktopRect& rect) {
  assert(magnifier_initialized_);

  // Set the magnifier control to cover the captured rect. The content of the
  // magnifier control will be the captured image.
  BOOL result = SetWindowPos(magnifier_window_,
                             NULL,
                             rect.left(), rect.top(),
                             rect.width(), rect.height(),
                             0);
  if (!result) {
    LOG_F(LS_WARNING) << "Failed to call SetWindowPos: " << GetLastError()
                      << ". Rect = {" << rect.left() << ", " << rect.top()
                      << ", " << rect.right() << ", " << rect.bottom() << "}";
    return false;
  }

  magnifier_capture_succeeded_ = false;

  RECT native_rect = {rect.left(), rect.top(), rect.right(), rect.bottom()};

  // OnCaptured will be called via OnMagImageScalingCallback and fill in the
  // frame before set_window_source_func_ returns.
  result = set_window_source_func_(magnifier_window_, native_rect);

  if (!result) {
    LOG_F(LS_WARNING) << "Failed to call MagSetWindowSource: " << GetLastError()
                      << ". Rect = {" << rect.left() << ", " << rect.top()
                      << ", " << rect.right() << ", " << rect.bottom() << "}";
    return false;
  }

  return magnifier_capture_succeeded_;
}

BOOL ScreenCapturerWinMagnifier::OnMagImageScalingCallback(
    HWND hwnd,
    void* srcdata,
    MAGIMAGEHEADER srcheader,
    void* destdata,
    MAGIMAGEHEADER destheader,
    RECT unclipped,
    RECT clipped,
    HRGN dirty) {
  assert(tls_index_.Value() != static_cast<int32_t>(TLS_OUT_OF_INDEXES));

  ScreenCapturerWinMagnifier* owner =
      reinterpret_cast<ScreenCapturerWinMagnifier*>(
          TlsGetValue(tls_index_.Value()));

  owner->OnCaptured(srcdata, srcheader);

  return TRUE;
}

bool ScreenCapturerWinMagnifier::InitializeMagnifier() {
  assert(!magnifier_initialized_);

  desktop_dc_ = GetDC(NULL);

  mag_lib_handle_ = LoadLibrary(L"Magnification.dll");
  if (!mag_lib_handle_)
    return false;

  // Initialize Magnification API function pointers.
  mag_initialize_func_ = reinterpret_cast<MagInitializeFunc>(
      GetProcAddress(mag_lib_handle_, "MagInitialize"));
  mag_uninitialize_func_ = reinterpret_cast<MagUninitializeFunc>(
      GetProcAddress(mag_lib_handle_, "MagUninitialize"));
  set_window_source_func_ = reinterpret_cast<MagSetWindowSourceFunc>(
      GetProcAddress(mag_lib_handle_, "MagSetWindowSource"));
  set_window_filter_list_func_ = reinterpret_cast<MagSetWindowFilterListFunc>(
      GetProcAddress(mag_lib_handle_, "MagSetWindowFilterList"));
  set_image_scaling_callback_func_ =
      reinterpret_cast<MagSetImageScalingCallbackFunc>(
          GetProcAddress(mag_lib_handle_, "MagSetImageScalingCallback"));

  if (!mag_initialize_func_ || !mag_uninitialize_func_ ||
      !set_window_source_func_ || !set_window_filter_list_func_ ||
      !set_image_scaling_callback_func_) {
    LOG_F(LS_WARNING) << "Failed to initialize ScreenCapturerWinMagnifier: "
                      << "library functions missing.";
    return false;
  }

  BOOL result = mag_initialize_func_();
  if (!result) {
    LOG_F(LS_WARNING) << "Failed to initialize ScreenCapturerWinMagnifier: "
                      << "error from MagInitialize " << GetLastError();
    return false;
  }

  HMODULE hInstance = NULL;
  result = GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                  GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                              reinterpret_cast<char*>(&DefWindowProc),
                              &hInstance);
  if (!result) {
    mag_uninitialize_func_();
    LOG_F(LS_WARNING) << "Failed to initialize ScreenCapturerWinMagnifier: "
                      << "error from GetModulehandleExA " << GetLastError();
    return false;
  }

  // Register the host window class. See the MSDN documentation of the
  // Magnification API for more infomation.
  WNDCLASSEX wcex = {};
  wcex.cbSize = sizeof(WNDCLASSEX);
  wcex.lpfnWndProc = &DefWindowProc;
  wcex.hInstance = hInstance;
  wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
  wcex.lpszClassName = kMagnifierHostClass;

  // Ignore the error which may happen when the class is already registered.
  RegisterClassEx(&wcex);

  // Create the host window.
  host_window_ = CreateWindowEx(WS_EX_LAYERED,
                                kMagnifierHostClass,
                                kHostWindowName,
                                0,
                                0, 0, 0, 0,
                                NULL,
                                NULL,
                                hInstance,
                                NULL);
  if (!host_window_) {
    mag_uninitialize_func_();
    LOG_F(LS_WARNING) << "Failed to initialize ScreenCapturerWinMagnifier: "
                      << "error from creating host window " << GetLastError();
    return false;
  }

  // Create the magnifier control.
  magnifier_window_ = CreateWindow(kMagnifierWindowClass,
                                   kMagnifierWindowName,
                                   WS_CHILD | WS_VISIBLE,
                                   0, 0, 0, 0,
                                   host_window_,
                                   NULL,
                                   hInstance,
                                   NULL);
  if (!magnifier_window_) {
    mag_uninitialize_func_();
    LOG_F(LS_WARNING) << "Failed to initialize ScreenCapturerWinMagnifier: "
                      << "error from creating magnifier window "
                      << GetLastError();
    return false;
  }

  // Hide the host window.
  ShowWindow(host_window_, SW_HIDE);

  // Set the scaling callback to receive captured image.
  result = set_image_scaling_callback_func_(
      magnifier_window_,
      &ScreenCapturerWinMagnifier::OnMagImageScalingCallback);
  if (!result) {
    mag_uninitialize_func_();
    LOG_F(LS_WARNING) << "Failed to initialize ScreenCapturerWinMagnifier: "
                      << "error from MagSetImageScalingCallback "
                      << GetLastError();
    return false;
  }

  if (excluded_window_) {
    result = set_window_filter_list_func_(
        magnifier_window_, MW_FILTERMODE_EXCLUDE, 1, &excluded_window_);
    if (!result) {
      mag_uninitialize_func_();
      LOG_F(LS_WARNING) << "Failed to initialize ScreenCapturerWinMagnifier: "
                        << "error from MagSetWindowFilterList "
                        << GetLastError();
      return false;
    }
  }

  if (tls_index_.Value() == static_cast<int32_t>(TLS_OUT_OF_INDEXES)) {
    // More than one threads may get here at the same time, but only one will
    // write to tls_index_ using CompareExchange.
    DWORD new_tls_index = TlsAlloc();
    if (!tls_index_.CompareExchange(new_tls_index, TLS_OUT_OF_INDEXES))
      TlsFree(new_tls_index);
  }

  assert(tls_index_.Value() != static_cast<int32_t>(TLS_OUT_OF_INDEXES));
  TlsSetValue(tls_index_.Value(), this);

  magnifier_initialized_ = true;
  return true;
}

void ScreenCapturerWinMagnifier::OnCaptured(void* data,
                                            const MAGIMAGEHEADER& header) {
  DesktopFrame* current_frame = queue_.current_frame();

  // Verify the format.
  // TODO(jiayl): support capturing sources with pixel formats other than RGBA.
  int captured_bytes_per_pixel = header.cbSize / header.width / header.height;
  if (header.format != GUID_WICPixelFormat32bppRGBA ||
      header.width != static_cast<UINT>(current_frame->size().width()) ||
      header.height != static_cast<UINT>(current_frame->size().height()) ||
      header.stride != static_cast<UINT>(current_frame->stride()) ||
      captured_bytes_per_pixel != DesktopFrame::kBytesPerPixel) {
    LOG_F(LS_WARNING) << "Output format does not match the captured format: "
                      << "width = " << header.width << ", "
                      << "height = " << header.height << ", "
                      << "stride = " << header.stride << ", "
                      << "bpp = " << captured_bytes_per_pixel << ", "
                      << "pixel format RGBA ? "
                      << (header.format == GUID_WICPixelFormat32bppRGBA) << ".";
    return;
  }

  // Copy the data into the frame.
  current_frame->CopyPixelsFrom(
      reinterpret_cast<uint8_t*>(data),
      header.stride,
      DesktopRect::MakeXYWH(0, 0, header.width, header.height));

  magnifier_capture_succeeded_ = true;
}

void ScreenCapturerWinMagnifier::CreateCurrentFrameIfNecessary(
    const DesktopSize& size) {
  // If the current buffer is from an older generation then allocate a new one.
  // Note that we can't reallocate other buffers at this point, since the caller
  // may still be reading from them.
  if (!queue_.current_frame() || !queue_.current_frame()->size().equals(size)) {
    size_t buffer_size =
        size.width() * size.height() * DesktopFrame::kBytesPerPixel;
    SharedMemory* shared_memory = callback_->CreateSharedMemory(buffer_size);

    rtc::scoped_ptr<DesktopFrame> buffer;
    if (shared_memory) {
      buffer.reset(new SharedMemoryDesktopFrame(
          size, size.width() * DesktopFrame::kBytesPerPixel, shared_memory));
    } else {
      buffer.reset(new BasicDesktopFrame(size));
    }
    queue_.ReplaceCurrentFrame(buffer.release());
  }
}

void ScreenCapturerWinMagnifier::StartFallbackCapturer() {
  assert(fallback_capturer_);
  if (!fallback_capturer_started_) {
    fallback_capturer_started_ = true;

    fallback_capturer_->Start(callback_);
    fallback_capturer_->SelectScreen(current_screen_id_);
  }
}

}  // namespace webrtc
