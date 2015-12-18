/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/desktop_capture/window_capturer.h"

#include <assert.h>
#include <ApplicationServices/ApplicationServices.h>
#include <Cocoa/Cocoa.h>
#include <CoreFoundation/CoreFoundation.h>

#include "webrtc/base/macutils.h"
#include "webrtc/modules/desktop_capture/desktop_capture_options.h"
#include "webrtc/modules/desktop_capture/desktop_frame.h"
#include "webrtc/modules/desktop_capture/mac/desktop_configuration.h"
#include "webrtc/modules/desktop_capture/mac/full_screen_chrome_window_detector.h"
#include "webrtc/modules/desktop_capture/mac/window_list_utils.h"
#include "webrtc/system_wrappers/interface/logging.h"
#include "webrtc/system_wrappers/interface/scoped_refptr.h"
#include "webrtc/system_wrappers/interface/tick_util.h"

namespace webrtc {

namespace {

// Returns true if the window exists.
bool IsWindowValid(CGWindowID id) {
  CFArrayRef window_id_array =
      CFArrayCreate(NULL, reinterpret_cast<const void **>(&id), 1, NULL);
  CFArrayRef window_array =
      CGWindowListCreateDescriptionFromArray(window_id_array);
  bool valid = window_array && CFArrayGetCount(window_array);
  CFRelease(window_id_array);
  CFRelease(window_array);

  return valid;
}

class WindowCapturerMac : public WindowCapturer {
 public:
  explicit WindowCapturerMac(
      scoped_refptr<FullScreenChromeWindowDetector>
          full_screen_chrome_window_detector);
  virtual ~WindowCapturerMac();

  // WindowCapturer interface.
  bool GetWindowList(WindowList* windows) override;
  bool SelectWindow(WindowId id) override;
  bool BringSelectedWindowToFront() override;

  // DesktopCapturer interface.
  void Start(Callback* callback) override;
  void Capture(const DesktopRegion& region) override;

 private:
  Callback* callback_;

  // The window being captured.
  CGWindowID window_id_;

  scoped_refptr<FullScreenChromeWindowDetector>
      full_screen_chrome_window_detector_;

  DISALLOW_COPY_AND_ASSIGN(WindowCapturerMac);
};

WindowCapturerMac::WindowCapturerMac(
    scoped_refptr<FullScreenChromeWindowDetector>
        full_screen_chrome_window_detector)
    : callback_(NULL),
      window_id_(0),
      full_screen_chrome_window_detector_(full_screen_chrome_window_detector) {
}

WindowCapturerMac::~WindowCapturerMac() {
}

bool WindowCapturerMac::GetWindowList(WindowList* windows) {
  // Only get on screen, non-desktop windows.
  CFArrayRef window_array = CGWindowListCopyWindowInfo(
      kCGWindowListOptionOnScreenOnly | kCGWindowListExcludeDesktopElements,
      kCGNullWindowID);
  if (!window_array)
    return false;

  // Check windows to make sure they have an id, title, and use window layer
  // other than 0.
  CFIndex count = CFArrayGetCount(window_array);
  for (CFIndex i = 0; i < count; ++i) {
    CFDictionaryRef window = reinterpret_cast<CFDictionaryRef>(
        CFArrayGetValueAtIndex(window_array, i));
    CFStringRef window_title = reinterpret_cast<CFStringRef>(
        CFDictionaryGetValue(window, kCGWindowName));
    CFNumberRef window_id = reinterpret_cast<CFNumberRef>(
        CFDictionaryGetValue(window, kCGWindowNumber));
    CFNumberRef window_layer = reinterpret_cast<CFNumberRef>(
        CFDictionaryGetValue(window, kCGWindowLayer));
    if (window_title && window_id && window_layer) {
      //Skip windows of zero area
      CFDictionaryRef bounds_ref = reinterpret_cast<CFDictionaryRef>(
           CFDictionaryGetValue(window,kCGWindowBounds));
      CGRect bounds_rect;
      if(!(bounds_ref) ||
        !(CGRectMakeWithDictionaryRepresentation(bounds_ref,&bounds_rect))){
        continue;
      }
      bounds_rect = CGRectStandardize(bounds_rect);
      if((bounds_rect.size.width <= 0) || (bounds_rect.size.height <= 0)){
        continue;
      }
      // Skip windows with layer=0 (menu, dock).
      int layer;
      CFNumberGetValue(window_layer, kCFNumberIntType, &layer);
      if (layer != 0)
        continue;

      int id;
      CFNumberGetValue(window_id, kCFNumberIntType, &id);
      WindowCapturer::Window window;
      window.id = id;
      if (!rtc::ToUtf8(window_title, &(window.title)) ||
          window.title.empty()) {
        continue;
      }
      windows->push_back(window);
    }
  }

  CFRelease(window_array);
  return true;
}

bool WindowCapturerMac::SelectWindow(WindowId id) {
  if (!IsWindowValid(id))
    return false;
  window_id_ = id;
  return true;
}

bool WindowCapturerMac::BringSelectedWindowToFront() {
  if (!window_id_)
    return false;

  CGWindowID ids[1];
  ids[0] = window_id_;
  CFArrayRef window_id_array =
      CFArrayCreate(NULL, reinterpret_cast<const void **>(&ids), 1, NULL);

  CFArrayRef window_array =
      CGWindowListCreateDescriptionFromArray(window_id_array);
  if (window_array == NULL || 0 == CFArrayGetCount(window_array)) {
    // Could not find the window. It might have been closed.
    LOG(LS_INFO) << "Window not found";
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

void WindowCapturerMac::Start(Callback* callback) {
  assert(!callback_);
  assert(callback);

  callback_ = callback;
}

void WindowCapturerMac::Capture(const DesktopRegion& region) {
  if (!IsWindowValid(window_id_)) {
    callback_->OnCaptureCompleted(NULL);
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
    callback_->OnCaptureCompleted(NULL);
    return;
  }

  int bits_per_pixel = CGImageGetBitsPerPixel(window_image);
  if (bits_per_pixel != 32) {
    LOG(LS_ERROR) << "Unsupported window image depth: " << bits_per_pixel;
    CFRelease(window_image);
    callback_->OnCaptureCompleted(NULL);
    return;
  }

  int width = CGImageGetWidth(window_image);
  int height = CGImageGetHeight(window_image);
  CGDataProviderRef provider = CGImageGetDataProvider(window_image);
  CFDataRef cf_data = CGDataProviderCopyData(provider);
  DesktopFrame* frame = new BasicDesktopFrame(
      DesktopSize(width, height));

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

  callback_->OnCaptureCompleted(frame);

  if (full_screen_chrome_window_detector_)
    full_screen_chrome_window_detector_->UpdateWindowListIfNeeded(window_id_);
}

}  // namespace

// static
WindowCapturer* WindowCapturer::Create(const DesktopCaptureOptions& options) {
  return new WindowCapturerMac(options.full_screen_chrome_window_detector());
}

}  // namespace webrtc
