/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Don't include this file in any .h files because it pulls in some X headers.

#ifndef WEBRTC_MODULES_DESKTOP_CAPTURE_X11_X_SERVER_PIXEL_BUFFER_H_
#define WEBRTC_MODULES_DESKTOP_CAPTURE_X11_X_SERVER_PIXEL_BUFFER_H_

#include "webrtc/modules/desktop_capture/desktop_geometry.h"

#include <X11/Xutil.h>
#include <X11/extensions/XShm.h>

namespace webrtc {

// A class to allow the X server's pixel buffer to be accessed as efficiently
// as possible.
class XServerPixelBuffer {
 public:
  XServerPixelBuffer();
  ~XServerPixelBuffer();

  void Release();

  // Allocate (or reallocate) the pixel buffer with the given size, which is
  // assumed to be the current size of the root window.
  // |screen_size| should either come from GetRootWindowSize(), or
  // from a recent ConfigureNotify event on the root window.
  void Init(Display* display, const DesktopSize& screen_size);

  // Request the current size of the root window from the X Server.
  static DesktopSize GetRootWindowSize(Display* display);

  // If shared memory is being used without pixmaps, synchronize this pixel
  // buffer with the root window contents (otherwise, this is a no-op).
  // This is to avoid doing a full-screen capture for each individual
  // rectangle in the capture list, when it only needs to be done once at the
  // beginning.
  void Synchronize();

  // Capture the specified rectangle and return a pointer to its top-left pixel
  // or NULL if capture fails. The returned pointer remains valid until the next
  // call to CaptureRect.
  // In the case where the full-screen data is captured by Synchronize(), this
  // simply returns the pointer without doing any more work.
  // The caller must ensure that |rect| is no larger than the screen size
  // supplied to Init().
  uint8_t* CaptureRect(const DesktopRect& rect);

  // Return information about the most recent capture. This is only guaranteed
  // to be valid between CaptureRect calls.
  int GetStride() const;
  int GetDepth() const;
  int GetBitsPerPixel() const;
  int GetRedMask() const;
  int GetBlueMask() const;
  int GetGreenMask() const;

 private:
  void InitShm(int screen);
  bool InitPixmaps(int depth);

  Display* display_;
  Window root_window_;
  DesktopSize root_window_size_;
  XImage* x_image_;
  XShmSegmentInfo* shm_segment_info_;
  Pixmap shm_pixmap_;
  GC shm_gc_;

  DISALLOW_COPY_AND_ASSIGN(XServerPixelBuffer);
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_DESKTOP_CAPTURE_X11_X_SERVER_PIXEL_BUFFER_H_
