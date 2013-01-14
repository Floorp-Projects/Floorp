/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef COMMON_VIDEO_INTERFACE_I420_VIDEO_FRAME_H
#define COMMON_VIDEO_INTERFACE_I420_VIDEO_FRAME_H

// I420VideoFrame class
//
// Storing and handling of YUV (I420) video frames.

#include "webrtc/common_video/plane.h"
#include "webrtc/typedefs.h"

/*
 *  I420VideoFrame includes support for a reference counted impl.
 */

namespace webrtc {

enum PlaneType {
  kYPlane = 0,
  kUPlane = 1,
  kVPlane = 2,
  kNumOfPlanes = 3
};

class I420VideoFrame {
 public:
  I420VideoFrame();
  virtual ~I420VideoFrame();
  // Infrastructure for refCount implementation.
  // Implements dummy functions for reference counting so that non reference
  // counted instantiation can be done. These functions should not be called
  // when creating the frame with new I420VideoFrame().
  // Note: do not pass a I420VideoFrame created with new I420VideoFrame() or
  // equivalent to a scoped_refptr or memory leak will occur.
  virtual int32_t AddRef() {assert(false); return -1;}
  virtual int32_t Release() {assert(false); return -1;}

  // CreateEmptyFrame: Sets frame dimensions and allocates buffers based
  // on set dimensions - height and plane stride.
  // If required size is bigger than the allocated one, new buffers of adequate
  // size will be allocated.
  // Return value: 0 on success ,-1 on error.
  int CreateEmptyFrame(int width, int height,
                       int stride_y, int stride_u, int stride_v);

  // CreateFrame: Sets the frame's members and buffers. If required size is
  // bigger than allocated one, new buffers of adequate size will be allocated.
  // Return value: 0 on success ,-1 on error.
  int CreateFrame(int size_y, const uint8_t* buffer_y,
                  int size_u, const uint8_t* buffer_u,
                  int size_v, const uint8_t* buffer_v,
                  int width, int height,
                  int stride_y, int stride_u, int stride_v);

  // Copy frame: If required size is bigger than allocated one, new buffers of
  // adequate size will be allocated.
  // Return value: 0 on success ,-1 on error.
  int CopyFrame(const I420VideoFrame& videoFrame);

  // Swap Frame.
  void SwapFrame(I420VideoFrame* videoFrame);

  // Get pointer to buffer per plane.
  uint8_t* buffer(PlaneType type);
  // Overloading with const.
  const uint8_t* buffer(PlaneType type) const;

  // Get allocated size per plane.
  int allocated_size(PlaneType type) const;

  // Get allocated stride per plane.
  int stride(PlaneType type) const;

  // Set frame width.
  int set_width(int width);

  // Set frame height.
  int set_height(int height);

  // Get frame width.
  int width() const {return width_;}

  // Get frame height.
  int height() const {return height_;}

  // Set frame timestamp (90kHz).
  void set_timestamp(const uint32_t timestamp) {timestamp_ = timestamp;}

  // Get frame timestamp (90kHz).
  uint32_t timestamp() const {return timestamp_;}

  // Set render time in miliseconds.
  void set_render_time_ms(int64_t render_time_ms) {render_time_ms_ =
                                                   render_time_ms;}

  // Get render time in miliseconds.
  int64_t render_time_ms() const {return render_time_ms_;}

  // Return true if underlying plane buffers are of zero size, false if not.
  bool IsZeroSize() const;

  // Reset underlying plane buffers sizes to 0. This function doesn't
  // clear memory.
  void ResetSize();

 private:
  // Verifies legality of parameters.
  // Return value: 0 on success ,-1 on error.
  int CheckDimensions(int width, int height,
                      int stride_y, int stride_u, int stride_v);
  // Get the pointer to a specific plane.
  const Plane* GetPlane(PlaneType type) const;
  // Overloading with non-const.
  Plane* GetPlane(PlaneType type);

  Plane y_plane_;
  Plane u_plane_;
  Plane v_plane_;
  int width_;
  int height_;
  uint32_t timestamp_;
  int64_t render_time_ms_;
};  // I420VideoFrame

}  // namespace webrtc

#endif  // COMMON_VIDEO_INTERFACE_I420_VIDEO_FRAME_H

