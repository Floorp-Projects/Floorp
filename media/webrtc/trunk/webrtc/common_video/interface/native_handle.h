/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef COMMON_VIDEO_INTERFACE_NATIVEHANDLE_H_
#define COMMON_VIDEO_INTERFACE_NATIVEHANDLE_H_

#include "webrtc/typedefs.h"

namespace webrtc {

// A class to store an opaque handle of the underlying video frame. This is used
// when the frame is backed by a texture. WebRTC carries the handle in
// TextureVideoFrame. This object keeps a reference to the handle. The reference
// is cleared when the object is destroyed. It is important to destroy the
// object as soon as possible so the texture can be recycled.
class NativeHandle {
 public:
  virtual ~NativeHandle() {}
  // For scoped_refptr
  virtual int32_t AddRef() = 0;
  virtual int32_t Release() = 0;

  // Gets the handle.
  virtual void* GetHandle() = 0;
};

}  // namespace webrtc

#endif  // COMMON_VIDEO_INTERFACE_NATIVEHANDLE_H_
