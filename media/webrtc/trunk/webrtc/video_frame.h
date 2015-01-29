/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_FRAME_H_
#define WEBRTC_VIDEO_FRAME_H_

#include <assert.h>

#include "webrtc/common_video/plane.h"
// TODO(pbos): Remove scoped_refptr include (and AddRef/Release if they're not
// used).
#include "webrtc/system_wrappers/interface/scoped_refptr.h"
#include "webrtc/typedefs.h"

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
  virtual int32_t AddRef() {
    assert(false);
    return -1;
  }
  virtual int32_t Release() {
    assert(false);
    return -1;
  }

  // CreateEmptyFrame: Sets frame dimensions and allocates buffers based
  // on set dimensions - height and plane stride.
  // If required size is bigger than the allocated one, new buffers of adequate
  // size will be allocated.
  // Return value: 0 on success, -1 on error.
  virtual int CreateEmptyFrame(int width,
                               int height,
                               int stride_y,
                               int stride_u,
                               int stride_v);

  // CreateFrame: Sets the frame's members and buffers. If required size is
  // bigger than allocated one, new buffers of adequate size will be allocated.
  // Return value: 0 on success, -1 on error.
  virtual int CreateFrame(int size_y,
                          const uint8_t* buffer_y,
                          int size_u,
                          const uint8_t* buffer_u,
                          int size_v,
                          const uint8_t* buffer_v,
                          int width,
                          int height,
                          int stride_y,
                          int stride_u,
                          int stride_v);

  // Copy frame: If required size is bigger than allocated one, new buffers of
  // adequate size will be allocated.
  // Return value: 0 on success, -1 on error.
  virtual int CopyFrame(const I420VideoFrame& videoFrame);

  // Make a copy of |this|. The caller owns the returned frame.
  // Return value: a new frame on success, NULL on error.
  virtual I420VideoFrame* CloneFrame() const;

  // Swap Frame.
  virtual void SwapFrame(I420VideoFrame* videoFrame);

  // Get pointer to buffer per plane.
  virtual uint8_t* buffer(PlaneType type);
  // Overloading with const.
  virtual const uint8_t* buffer(PlaneType type) const;

  // Get allocated size per plane.
  virtual int allocated_size(PlaneType type) const;

  // Get allocated stride per plane.
  virtual int stride(PlaneType type) const;

  // Set frame width.
  virtual int set_width(int width);

  // Set frame height.
  virtual int set_height(int height);

  // Get frame width.
  virtual int width() const { return width_; }

  // Get frame height.
  virtual int height() const { return height_; }

  // Set frame timestamp (90kHz).
  virtual void set_timestamp(uint32_t timestamp) { timestamp_ = timestamp; }

  // Get frame timestamp (90kHz).
  virtual uint32_t timestamp() const { return timestamp_; }

  // Set capture ntp time in miliseconds.
  virtual void set_ntp_time_ms(int64_t ntp_time_ms) {
    ntp_time_ms_ = ntp_time_ms;
  }

  // Get capture ntp time in miliseconds.
  virtual int64_t ntp_time_ms() const { return ntp_time_ms_; }

  // Set render time in miliseconds.
  virtual void set_render_time_ms(int64_t render_time_ms) {
    render_time_ms_ = render_time_ms;
  }

  // Get render time in miliseconds.
  virtual int64_t render_time_ms() const { return render_time_ms_; }

  // Return true if underlying plane buffers are of zero size, false if not.
  virtual bool IsZeroSize() const;

  // Reset underlying plane buffers sizes to 0. This function doesn't
  // clear memory.
  virtual void ResetSize();

  // Return the handle of the underlying video frame. This is used when the
  // frame is backed by a texture. The object should be destroyed when it is no
  // longer in use, so the underlying resource can be freed.
  virtual void* native_handle() const;

 protected:
  // Verifies legality of parameters.
  // Return value: 0 on success, -1 on error.
  virtual int CheckDimensions(int width,
                              int height,
                              int stride_y,
                              int stride_u,
                              int stride_v);

 private:
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
  int64_t ntp_time_ms_;
  int64_t render_time_ms_;
};

enum VideoFrameType {
  kKeyFrame = 0,
  kDeltaFrame = 1,
  kGoldenFrame = 2,
  kAltRefFrame = 3,
  kSkipFrame = 4
};

// TODO(pbos): Rename EncodedFrame and reformat this class' members.
class EncodedImage {
 public:
  EncodedImage()
      : _encodedWidth(0),
        _encodedHeight(0),
        _timeStamp(0),
        capture_time_ms_(0),
        _frameType(kDeltaFrame),
        _buffer(NULL),
        _length(0),
        _size(0),
        _completeFrame(false) {}

  EncodedImage(uint8_t* buffer, uint32_t length, uint32_t size)
      : _encodedWidth(0),
        _encodedHeight(0),
        _timeStamp(0),
        ntp_time_ms_(0),
        capture_time_ms_(0),
        _frameType(kDeltaFrame),
        _buffer(buffer),
        _length(length),
        _size(size),
        _completeFrame(false) {}

  uint32_t _encodedWidth;
  uint32_t _encodedHeight;
  uint32_t _timeStamp;
  // NTP time of the capture time in local timebase in milliseconds.
  int64_t ntp_time_ms_;
  int64_t capture_time_ms_;
  VideoFrameType _frameType;
  uint8_t* _buffer;
  uint32_t _length;
  uint32_t _size;
  bool _completeFrame;
};

}  // namespace webrtc
#endif  // WEBRTC_VIDEO_FRAME_H_

