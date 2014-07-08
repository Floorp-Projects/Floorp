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

#include "webrtc/modules/desktop_capture/desktop_frame.h"
#include "webrtc/system_wrappers/interface/logging.h"

namespace webrtc {

namespace {

bool CFStringRefToUtf8(const CFStringRef string, std::string* str_utf8) {
  assert(string);
  assert(str_utf8);
  CFIndex length = CFStringGetLength(string);
  size_t max_length_utf8 =
      CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8);
  str_utf8->resize(max_length_utf8);
  CFIndex used_bytes;
  int result = CFStringGetBytes(
      string, CFRangeMake(0, length), kCFStringEncodingUTF8, 0, false,
      reinterpret_cast<UInt8*>(&*str_utf8->begin()), max_length_utf8,
      &used_bytes);
  if (result != length) {
    str_utf8->clear();
    return false;
  }
  str_utf8->resize(used_bytes);
  return true;
}

class WindowCapturerMac : public WindowCapturer {
 public:
  WindowCapturerMac();
  virtual ~WindowCapturerMac();

  // WindowCapturer interface.
  virtual bool GetWindowList(WindowList* windows) OVERRIDE;
  virtual bool SelectWindow(WindowId id) OVERRIDE;
  virtual bool BringSelectedWindowToFront() OVERRIDE;

  // DesktopCapturer interface.
  virtual void Start(Callback* callback) OVERRIDE;
  virtual void Capture(const DesktopRegion& region) OVERRIDE;

 private:
  Callback* callback_;
  CGWindowID window_id_;

  DISALLOW_COPY_AND_ASSIGN(WindowCapturerMac);
};

WindowCapturerMac::WindowCapturerMac()
    : callback_(NULL),
      window_id_(0) {
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
      // Skip windows with layer=0 (menu, dock).
      int layer;
      CFNumberGetValue(window_layer, kCFNumberIntType, &layer);
      if (layer != 0)
        continue;

      int id;
      CFNumberGetValue(window_id, kCFNumberIntType, &id);
      WindowCapturer::Window window;
      window.id = id;
      if (!CFStringRefToUtf8(window_title, &(window.title)) ||
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
  // Request description for the specified window to make sure |id| is valid.
  CGWindowID ids[1];
  ids[0] = id;
  CFArrayRef window_id_array =
      CFArrayCreate(NULL, reinterpret_cast<const void **>(&ids), 1, NULL);
  CFArrayRef window_array =
      CGWindowListCreateDescriptionFromArray(window_id_array);
  int results_count = window_array ? CFArrayGetCount(window_array) : 0;
  CFRelease(window_id_array);
  CFRelease(window_array);

  if (results_count == 0) {
    // Could not find the window. It might have been closed.
    return false;
  }

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
  CGImageRef window_image = CGWindowListCreateImage(
      CGRectNull, kCGWindowListOptionIncludingWindow,
      window_id_, kCGWindowImageBoundsIgnoreFraming);

  if (!window_image) {
    CFRelease(window_image);
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

  callback_->OnCaptureCompleted(frame);
}

}  // namespace

// static
WindowCapturer* WindowCapturer::Create(const DesktopCaptureOptions& options) {
  return new WindowCapturerMac();
}

}  // namespace webrtc
