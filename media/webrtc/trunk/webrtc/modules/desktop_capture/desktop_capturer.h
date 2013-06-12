/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_DESKTOP_CAPTURE_DESKTOP_CAPTURER_H_
#define WEBRTC_MODULES_DESKTOP_CAPTURE_DESKTOP_CAPTURER_H_

namespace webrtc {

class DesktopFrame;
class DesktopRegion;
class SharedMemory;

// Abstract interface for screen and window capturers.
class DesktopCapturer {
 public:
  // Interface that must be implemented by the DesktopCapturer consumers.
  class Callback {
   public:
    // Creates a new shared memory buffer for a frame create by the capturer.
    // Should return null shared memory is not used for captured frames (in that
    // case the capturer will allocate memory on the heap).
    virtual SharedMemory* CreateSharedMemory(size_t size) = 0;

    // Called after a frame has been captured. Handler must take ownership of
    // |frame|. If capture has failed for any reason |frame| is set to NULL
    // (e.g. the window has been closed).
    virtual void OnCaptureCompleted(DesktopFrame* frame) = 0;

   protected:
    virtual ~Callback() {}
  };

  virtual ~DesktopCapturer() {}

  // Called at the beginning of a capturing session. |callback| must remain
  // valid until capturer is destroyed.
  virtual void Start(Callback* callback) = 0;

  // Captures next frame. |region| specifies region of the capture target that
  // should be fresh in the resulting frame. The frame may also include fresh
  // data for areas outside |region|. In that case capturer will include these
  // areas in updated_region() of the frame. |region| is specified relative to
  // the top left corner of the capture target. Pending capture operations are
  // canceled when DesktopCapturer is deleted.
  virtual void Capture(const DesktopRegion& region) = 0;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_DESKTOP_CAPTURE_DESKTOP_CAPTURER_H_

