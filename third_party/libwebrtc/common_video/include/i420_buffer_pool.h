/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef COMMON_VIDEO_INCLUDE_I420_BUFFER_POOL_H_
#define COMMON_VIDEO_INCLUDE_I420_BUFFER_POOL_H_

#include <stddef.h>

#include <list>

#include "api/scoped_refptr.h"
#include "api/video/i420_buffer.h"
#include "common_video/include/video_frame_buffer_pool.h"
#include "rtc_base/race_checker.h"
#include "rtc_base/ref_counted_object.h"

namespace webrtc {

// Deprecated. Use VideoFrameBufferPool instead.
class I420BufferPool : public VideoFrameBufferPool {
 public:
  I420BufferPool() : VideoFrameBufferPool() {}
  explicit I420BufferPool(bool zero_initialize)
      : VideoFrameBufferPool(zero_initialize) {}
  I420BufferPool(bool zero_initialze, size_t max_number_of_buffers)
      : VideoFrameBufferPool(zero_initialze, max_number_of_buffers) {}
  ~I420BufferPool() = default;

  rtc::scoped_refptr<I420Buffer> CreateBuffer(int width, int height) {
    return VideoFrameBufferPool::CreateI420Buffer(width, height);
  }
};

}  // namespace webrtc

#endif  // COMMON_VIDEO_INCLUDE_I420_BUFFER_POOL_H_
