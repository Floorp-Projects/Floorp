/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <assert.h>
#include <ApplicationServices/ApplicationServices.h>
#include <Cocoa/Cocoa.h>
#include <CoreFoundation/CoreFoundation.h>

#include <utility>

#include "modules/desktop_capture/desktop_capture_options.h"
#include "modules/desktop_capture/desktop_capturer.h"
#include "modules/desktop_capture/desktop_frame.h"
#include "modules/desktop_capture/window_finder_mac.h"
#include "modules/desktop_capture/mac/desktop_configuration.h"
#include "modules/desktop_capture/mac/desktop_configuration_monitor.h"
#include "modules/desktop_capture/mac/full_screen_chrome_window_detector.h"
#include "modules/desktop_capture/mac/window_list_utils.h"
#include "rtc_base/constructormagic.h"
#include "rtc_base/logging.h"
#include "rtc_base/macutils.h"
#include "rtc_base/scoped_ref_ptr.h"

namespace webrtc {

namespace {

// Returns true if the window exists.
bool IsWindowValid(CGWindowID id) {
  CFArrayRef window_id_array =
      CFArrayCreate(nullptr, reinterpret_cast<const void**>(&id), 1, nullptr);
  CFArrayRef window_array =
      CGWindowListCreateDescriptionFromArray(window_id_array);
  bool valid = window_array && CFArrayGetCount(window_array);
  CFRelease(window_id_array);
  CFRelease(window_array);

  return valid;
}

class WindowCapturerMac : public DesktopCapturer {
 public:
  explicit WindowCapturerMac(rtc::scoped_refptr<FullScreenChromeWindowDetector>
                                 full_screen_chrome_window_detector,
                             rtc::scoped_refptr<DesktopConfigurationMonitor>
                                 configuration_monitor);
  ~WindowCapturerMac() override;

  // DesktopCapturer interface.
  void Start(Callback* callback) override;
  void CaptureFrame() override;
  bool GetSourceList(SourceList* sources) override;
  bool SelectSource(SourceId id) override;
  bool FocusOnSelectedSource() override;
  bool IsOccluded(const DesktopVector& pos) override;

 private:
  Callback* callback_ = nullptr;

  // The window being captured.
  CGWindowID window_id_ = 0;

  const rtc::scoped_refptr<FullScreenChromeWindowDetector>
      full_screen_chrome_window_detector_;

  const rtc::scoped_refptr<DesktopConfigurationMonitor> configuration_monitor_;

  WindowFinderMac window_finder_;

  RTC_DISALLOW_COPY_AND_ASSIGN(WindowCapturerMac);
};

WindowCapturerMac::WindowCapturerMac(
    rtc::scoped_refptr<FullScreenChromeWindowDetector>
        full_screen_chrome_window_detector,
    rtc::scoped_refptr<DesktopConfigurationMonitor> configuration_monitor)
    : full_screen_chrome_window_detector_(
          std::move(full_screen_chrome_window_detector)),
      configuration_monitor_(std::move(configuration_monitor)),
      window_finder_(configuration_monitor_) {}

WindowCapturerMac::~WindowCapturerMac() {}

bool WindowCapturerMac::GetSourceList(SourceList* sources) {
  return webrtc::GetWindowList(sources, true);
}

bool WindowCapturerMac::SelectSource(SourceId id) {
  if (!IsWindowValid(id))
    return false;
  window_id_ = id;
  return true;
}

bool WindowCapturerMac::FocusOnSelectedSource() {
  if (!window_id_)
    return false;

  CGWindowID ids[1];
  ids[0] = window_id_;
  CFArrayRef window_id_array =
      CFArrayCreate(nullptr, reinterpret_cast<const void**>(&ids), 1, nullptr);

  CFArrayRef window_array =
      CGWindowListCreateDescriptionFromArray(window_id_array);
  if (!window_array || 0 == CFArrayGetCount(window_array)) {
    // Could not find the window. It might have been closed.
    RTC_LOG(LS_INFO) << "Window not found";
    CFRelease(window_id_array);
    return false;
  }

  CFDictionaryRef window = reinterpret_cast<CFDictionaryRef>(
      CFArrayGetValueAtIndex(window_array, 0));
  CFNumberRef pid_ref = reinterpret_cast<CFNumberRef>(
      CFDictionaryGetValue(window, kCGWindowOwnerPID));

  int pid;
  CFNumberGetValue(pid_ref, kCFNumberIntType, &pid);

  // TODO(jiayl): this will bring the process main window to the front. We
  // should find a way to bring only the window to the front.
  bool result =
      [[NSRunningApplication runningApplicationWithProcessIdentifier: pid]
          activateWithOptions: NSApplicationActivateIgnoringOtherApps];

  CFRelease(window_id_array);
  CFRelease(window_array);
  return result;
}

bool WindowCapturerMac::IsOccluded(const DesktopVector& pos) {
  DesktopVector sys_pos = pos;
  if (configuration_monitor_) {
    configuration_monitor_->Lock();
    auto configuration = configuration_monitor_->desktop_configuration();
    configuration_monitor_->Unlock();
    sys_pos = pos.add(configuration.bounds.top_left());
  }
  return window_finder_.GetWindowUnderPoint(sys_pos) != window_id_;
}

void WindowCapturerMac::Start(Callback* callback) {
  assert(!callback_);
  assert(callback);

  callback_ = callback;
}

void WindowCapturerMac::CaptureFrame() {
  if (!IsWindowValid(window_id_)) {
    callback_->OnCaptureResult(Result::ERROR_PERMANENT, nullptr);
    return;
  }

  CGWindowID on_screen_window = window_id_;
  if (full_screen_chrome_window_detector_) {
    CGWindowID full_screen_window =
        full_screen_chrome_window_detector_->FindFullScreenWindow(window_id_);

    if (full_screen_window != kCGNullWindowID)
      on_screen_window = full_screen_window;
  }

  CGImageRef window_image = CGWindowListCreateImage(
      CGRectNull, kCGWindowListOptionIncludingWindow,
      on_screen_window, kCGWindowImageBoundsIgnoreFraming);

  if (!window_image) {
    callback_->OnCaptureResult(Result::ERROR_TEMPORARY, nullptr);
    return;
  }

  int bits_per_pixel = CGImageGetBitsPerPixel(window_image);
  if (bits_per_pixel != 32) {
    RTC_LOG(LS_ERROR) << "Unsupported window image depth: " << bits_per_pixel;
    CFRelease(window_image);
    callback_->OnCaptureResult(Result::ERROR_PERMANENT, nullptr);
    return;
  }

  int width = CGImageGetWidth(window_image);
  int height = CGImageGetHeight(window_image);
  CGDataProviderRef provider = CGImageGetDataProvider(window_image);
  CFDataRef cf_data = CGDataProviderCopyData(provider);
  std::unique_ptr<DesktopFrame> frame(
      new BasicDesktopFrame(DesktopSize(width, height)));

  int src_stride = CGImageGetBytesPerRow(window_image);
  const uint8_t* src_data = CFDataGetBytePtr(cf_data);
  for (int y = 0; y < height; ++y) {
    memcpy(frame->data() + frame->stride() * y, src_data + src_stride * y,
           DesktopFrame::kBytesPerPixel * width);
  }

  CFRelease(cf_data);
  CFRelease(window_image);

  frame->mutable_updated_region()->SetRect(
      DesktopRect::MakeSize(frame->size()));
  DesktopVector top_left;
  if (configuration_monitor_) {
    configuration_monitor_->Lock();
    auto configuration = configuration_monitor_->desktop_configuration();
    configuration_monitor_->Unlock();
    top_left = GetWindowBounds(configuration, on_screen_window).top_left();
    top_left = top_left.subtract(configuration.bounds.top_left());
  } else {
    top_left = GetWindowBounds(on_screen_window).top_left();
  }
  frame->set_top_left(top_left);

  callback_->OnCaptureResult(Result::SUCCESS, std::move(frame));

  if (full_screen_chrome_window_detector_)
    full_screen_chrome_window_detector_->UpdateWindowListIfNeeded(window_id_);
}

}  // namespace

// static
std::unique_ptr<DesktopCapturer> DesktopCapturer::CreateRawWindowCapturer(
    const DesktopCaptureOptions& options) {
  return std::unique_ptr<DesktopCapturer>(
      new WindowCapturerMac(options.full_screen_chrome_window_detector(),
                            options.configuration_monitor()));
}

}  // namespace webrtc
