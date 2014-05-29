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

#include <stddef.h>

#include "webrtc/common_types.h"

namespace webrtc {

class I420VideoFrame;

struct EncodedFrame {
 public:
  EncodedFrame() : data_(NULL), length_(0), frame_type_(kFrameEmpty) {}
  EncodedFrame(const uint8_t* data, size_t length, FrameType frame_type)
    : data_(data), length_(length), frame_type_(frame_type) {}

  const uint8_t* data_;
  const size_t length_;
  const FrameType frame_type_;
};

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
