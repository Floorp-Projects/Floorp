/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_DESKTOP_CAPTURE_SCREEN_CAPTURE_FRAME_QUEUE_H_
#define WEBRTC_MODULES_DESKTOP_CAPTURE_SCREEN_CAPTURE_FRAME_QUEUE_H_

#include "webrtc/base/scoped_ptr.h"
#include "webrtc/modules/desktop_capture/shared_desktop_frame.h"
#include "webrtc/typedefs.h"

namespace webrtc {
class DesktopFrame;
}  // namespace webrtc

namespace webrtc {

// Represents a queue of reusable video frames. Provides access to the 'current'
// frame - the frame that the caller is working with at the moment, and to the
// 'previous' frame - the predecessor of the current frame swapped by
// MoveToNextFrame() call, if any.
//
// The caller is expected to (re)allocate frames if current_frame() returns
// NULL. The caller can mark all frames in the queue for reallocation (when,
// say, frame dimensions change). The queue records which frames need updating
// which the caller can query.
//
// Frame consumer is expected to never hold more than kQueueLength frames
// created by this function and it should release the earliest one before trying
// to capture a new frame (i.e. before MoveToNextFrame() is called).
class ScreenCaptureFrameQueue {
 public:
  ScreenCaptureFrameQueue();
  ~ScreenCaptureFrameQueue();

  // Moves to the next frame in the queue, moving the 'current' frame to become
  // the 'previous' one.
  void MoveToNextFrame();

  // Replaces the current frame with a new one allocated by the caller. The
  // existing frame (if any) is destroyed. Takes ownership of |frame|.
  void ReplaceCurrentFrame(DesktopFrame* frame);

  // Marks all frames obsolete and resets the previous frame pointer. No
  // frames are freed though as the caller can still access them.
  void Reset();

  SharedDesktopFrame* current_frame() const {
    return frames_[current_].get();
  }

  SharedDesktopFrame* previous_frame() const {
    return frames_[(current_ + kQueueLength - 1) % kQueueLength].get();
  }

 private:
  // Index of the current frame.
  int current_;

  static const int kQueueLength = 2;
  rtc::scoped_ptr<SharedDesktopFrame> frames_[kQueueLength];

  RTC_DISALLOW_COPY_AND_ASSIGN(ScreenCaptureFrameQueue);
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_DESKTOP_CAPTURE_SCREEN_CAPTURE_FRAME_QUEUE_H_
