/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_ENGINE_NEW_INCLUDE_FRAME_CALLBACK_H_
#define WEBRTC_VIDEO_ENGINE_NEW_INCLUDE_FRAME_CALLBACK_H_

namespace webrtc {

class I420VideoFrame;

struct EncodedFrame;

class I420FrameCallback {
 public:
  // This function is called with a I420 frame allowing the user to modify the
  // frame content.
  virtual void FrameCallback(I420VideoFrame* video_frame) = 0;

 protected:
  virtual ~I420FrameCallback() {}
};

class EncodedFrameObserver {
 public:
  virtual void EncodedFrameCallback(const EncodedFrame& encoded_frame) = 0;

 protected:
  virtual ~EncodedFrameObserver() {}
};
}  // namespace webrtc

#endif  // WEBRTC_VIDEO_ENGINE_NEW_INCLUDE_FRAME_CALLBACK_H_
