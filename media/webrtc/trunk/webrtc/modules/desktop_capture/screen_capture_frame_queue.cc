/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/desktop_capture/screen_capture_frame_queue.h"

#include <assert.h>
#include <algorithm>

#include "webrtc/modules/desktop_capture/desktop_frame.h"
#include "webrtc/modules/desktop_capture/shared_desktop_frame.h"
#include "webrtc/system_wrappers/include/logging.h"
#include "webrtc/typedefs.h"

namespace webrtc {

ScreenCaptureFrameQueue::ScreenCaptureFrameQueue() : current_(0) {}

ScreenCaptureFrameQueue::~ScreenCaptureFrameQueue() {}

void ScreenCaptureFrameQueue::MoveToNextFrame() {
  current_ = (current_ + 1) % kQueueLength;

  // Verify that the frame is not shared, i.e. that consumer has released it
  // before attempting to capture again.
  assert(!frames_[current_].get() || !frames_[current_]->IsShared());
}

void ScreenCaptureFrameQueue::ReplaceCurrentFrame(DesktopFrame* frame) {
  frames_[current_].reset(SharedDesktopFrame::Wrap(frame));
}

void ScreenCaptureFrameQueue::Reset() {
  for (int i = 0; i < kQueueLength; ++i)
    frames_[i].reset();
}

}  // namespace webrtc
