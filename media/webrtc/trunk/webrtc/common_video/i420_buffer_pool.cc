/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/common_video/include/i420_buffer_pool.h"

#include "webrtc/base/checks.h"

namespace {

// One extra indirection is needed to make |HasOneRef| work.
class PooledI420Buffer : public webrtc::VideoFrameBuffer {
 public:
  explicit PooledI420Buffer(
      const rtc::scoped_refptr<webrtc::I420Buffer>& buffer)
      : buffer_(buffer) {}

 private:
  ~PooledI420Buffer() override {}

  int width() const override { return buffer_->width(); }
  int height() const override { return buffer_->height(); }
  const uint8_t* data(webrtc::PlaneType type) const override {
    return buffer_->data(type);
  }
  uint8_t* MutableData(webrtc::PlaneType type) override {
    // Make the HasOneRef() check here instead of in |buffer_|, because the pool
    // also has a reference to |buffer_|.
    RTC_DCHECK(HasOneRef());
    return const_cast<uint8_t*>(buffer_->data(type));
  }
  int stride(webrtc::PlaneType type) const override {
    return buffer_->stride(type);
  }
  void* native_handle() const override { return nullptr; }

  rtc::scoped_refptr<VideoFrameBuffer> NativeToI420Buffer() override {
    RTC_NOTREACHED();
    return nullptr;
  }

  friend class rtc::RefCountedObject<PooledI420Buffer>;
  rtc::scoped_refptr<webrtc::I420Buffer> buffer_;
};

}  // namespace

namespace webrtc {

I420BufferPool::I420BufferPool() {
  Release();
}

void I420BufferPool::Release() {
  thread_checker_.DetachFromThread();
  buffers_.clear();
}

rtc::scoped_refptr<VideoFrameBuffer> I420BufferPool::CreateBuffer(int width,
                                                                  int height) {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  // Release buffers with wrong resolution.
  for (auto it = buffers_.begin(); it != buffers_.end();) {
    if ((*it)->width() != width || (*it)->height() != height)
      it = buffers_.erase(it);
    else
      ++it;
  }
  // Look for a free buffer.
  for (const rtc::scoped_refptr<I420Buffer>& buffer : buffers_) {
    // If the buffer is in use, the ref count will be 2, one from the list we
    // are looping over and one from a PooledI420Buffer returned from
    // CreateBuffer that has not been released yet. If the ref count is 1
    // (HasOneRef), then the list we are looping over holds the only reference
    // and it's safe to reuse.
    if (buffer->HasOneRef())
      return new rtc::RefCountedObject<PooledI420Buffer>(buffer);
  }
  // Allocate new buffer.
  buffers_.push_back(new rtc::RefCountedObject<I420Buffer>(width, height));
  return new rtc::RefCountedObject<PooledI420Buffer>(buffers_.back());
}

}  // namespace webrtc
