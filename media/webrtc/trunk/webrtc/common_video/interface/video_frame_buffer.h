/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_FRAME_BUFFER_H_
#define WEBRTC_VIDEO_FRAME_BUFFER_H_

#include "webrtc/base/callback.h"
#include "webrtc/base/refcount.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/base/scoped_ref_ptr.h"
#include "webrtc/common_video/interface/native_handle.h"
#include "webrtc/system_wrappers/interface/aligned_malloc.h"

namespace webrtc {

enum PlaneType {
  kYPlane = 0,
  kUPlane = 1,
  kVPlane = 2,
  kNumOfPlanes = 3,
};

// Interface of a simple frame buffer containing pixel data. This interface does
// not contain any frame metadata such as rotation, timestamp, pixel_width, etc.
class VideoFrameBuffer : public rtc::RefCountInterface {
 public:
  // Returns true if this buffer has a single exclusive owner.
  virtual bool HasOneRef() const = 0;

  // The resolution of the frame in pixels. For formats where some planes are
  // subsampled, this is the highest-resolution plane.
  virtual int width() const = 0;
  virtual int height() const = 0;

  // Returns pointer to the pixel data for a given plane. The memory is owned by
  // the VideoFrameBuffer object and must not be freed by the caller.
  virtual const uint8_t* data(PlaneType type) const = 0;

  // Non-const data access is only allowed if |HasOneRef| is true.
  virtual uint8_t* data(PlaneType type) = 0;

  // Returns the number of bytes between successive rows for a given plane.
  virtual int stride(PlaneType type) const = 0;

  // Return the handle of the underlying video frame. This is used when the
  // frame is backed by a texture.
  virtual rtc::scoped_refptr<NativeHandle> native_handle() const = 0;

 protected:
  virtual ~VideoFrameBuffer();
};

// Plain I420 buffer in standard memory.
class I420Buffer : public VideoFrameBuffer {
 public:
  I420Buffer(int width, int height);
  I420Buffer(int width, int height, int stride_y, int stride_u, int stride_v);

  int width() const override;
  int height() const override;
  const uint8_t* data(PlaneType type) const override;
  uint8_t* data(PlaneType type) override;
  int stride(PlaneType type) const override;
  rtc::scoped_refptr<NativeHandle> native_handle() const override;

 protected:
  ~I420Buffer() override;

 private:
  const int width_;
  const int height_;
  const int stride_y_;
  const int stride_u_;
  const int stride_v_;
  const rtc::scoped_ptr<uint8_t, AlignedFreeDeleter> data_;
};

// Texture buffer around a NativeHandle.
class TextureBuffer : public VideoFrameBuffer {
 public:
  TextureBuffer(const rtc::scoped_refptr<NativeHandle>& native_handle,
                int width,
                int height);

  int width() const override;
  int height() const override;
  const uint8_t* data(PlaneType type) const override;
  uint8_t* data(PlaneType type) override;
  int stride(PlaneType type) const override;
  rtc::scoped_refptr<NativeHandle> native_handle() const override;

 private:
  friend class rtc::RefCountedObject<TextureBuffer>;
  ~TextureBuffer() override;

  const rtc::scoped_refptr<NativeHandle> native_handle_;
  const int width_;
  const int height_;
};

class WrappedI420Buffer : public webrtc::VideoFrameBuffer {
 public:
  WrappedI420Buffer(int desired_width,
                    int desired_height,
                    int width,
                    int height,
                    const uint8_t* y_plane,
                    int y_stride,
                    const uint8_t* u_plane,
                    int u_stride,
                    const uint8_t* v_plane,
                    int v_stride,
                    const rtc::Callback0<void>& no_longer_used);
  int width() const override;
  int height() const override;

  const uint8_t* data(PlaneType type) const override;
  uint8_t* data(PlaneType type) override;

  int stride(PlaneType type) const override;
  rtc::scoped_refptr<NativeHandle> native_handle() const override;

 private:
  friend class rtc::RefCountedObject<WrappedI420Buffer>;
  ~WrappedI420Buffer() override;

  int width_;
  int height_;
  const uint8_t* y_plane_;
  const uint8_t* u_plane_;
  const uint8_t* v_plane_;
  const int y_stride_;
  const int u_stride_;
  const int v_stride_;
  rtc::Callback0<void> no_longer_used_cb_;
};

}  // namespace webrtc

#endif  // WEBRTC_VIDEO_FRAME_BUFFER_H_
