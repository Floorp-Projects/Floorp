/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCVideoFrame.h"

#include "webrtc/base/scoped_ptr.h"

#import "webrtc/api/objc/RTCVideoFrame+Private.h"

@implementation RTCVideoFrame {
  rtc::scoped_ptr<cricket::VideoFrame> _videoFrame;
}

- (size_t)width {
  return _videoFrame->GetWidth();
}

- (size_t)height {
  return _videoFrame->GetHeight();
}

- (size_t)chromaWidth {
  return _videoFrame->GetChromaWidth();
}

- (size_t)chromaHeight {
  return _videoFrame->GetChromaHeight();
}

- (size_t)chromaSize {
  return _videoFrame->GetChromaSize();
}

- (const uint8_t *)yPlane {
  const cricket::VideoFrame *const_frame = _videoFrame.get();
  return const_frame->GetYPlane();
}

- (const uint8_t *)uPlane {
  const cricket::VideoFrame *const_frame = _videoFrame.get();
  return const_frame->GetUPlane();
}

- (const uint8_t *)vPlane {
  const cricket::VideoFrame *const_frame = _videoFrame.get();
  return const_frame->GetVPlane();
}

- (int32_t)yPitch {
  return _videoFrame->GetYPitch();
}

- (int32_t)uPitch {
  return _videoFrame->GetUPitch();
}

- (int32_t)vPitch {
  return _videoFrame->GetVPitch();
}

#pragma mark - Private

- (instancetype)initWithNativeFrame:(const cricket::VideoFrame *)nativeFrame {
  if (self = [super init]) {
    // Keep a shallow copy of the video frame. The underlying frame buffer is
    // not copied.
    _videoFrame.reset(nativeFrame->Copy());
  }
  return self;
}

@end
