/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_COPYONWRITEBUFFER_H_
#define RTC_BASE_COPYONWRITEBUFFER_H_

#include <algorithm>
#include <utility>

#include "rtc_base/buffer.h"
#include "rtc_base/checks.h"
#include "rtc_base/refcount.h"
#include "rtc_base/refcountedobject.h"
#include "rtc_base/scoped_ref_ptr.h"

namespace rtc {

class CopyOnWriteBuffer {
 public:
  // An empty buffer.
  CopyOnWriteBuffer();
  // Copy size and contents of an existing buffer.
  CopyOnWriteBuffer(const CopyOnWriteBuffer& buf);
  // Move contents from an existing buffer.
  CopyOnWriteBuffer(CopyOnWriteBuffer&& buf);

  // Construct a buffer with the specified number of uninitialized bytes.
  explicit CopyOnWriteBuffer(size_t size);
  CopyOnWriteBuffer(size_t size, size_t capacity);

  // Construct a buffer and copy the specified number of bytes into it. The
  // source array may be (const) uint8_t*, int8_t*, or char*.
  template <typename T,
            typename std::enable_if<
                internal::BufferCompat<uint8_t, T>::value>::type* = nullptr>
  CopyOnWriteBuffer(const T* data, size_t size)
      : CopyOnWriteBuffer(data, size, size) {}
  template <typename T,
            typename std::enable_if<
                internal::BufferCompat<uint8_t, T>::value>::type* = nullptr>
  CopyOnWriteBuffer(const T* data, size_t size, size_t capacity)
      : CopyOnWriteBuffer(size, capacity) {
    if (buffer_) {
      std::memcpy(buffer_->data(), data, size);
    }
  }

  // Construct a buffer from the contents of an array.
  template <typename T,
            size_t N,
            typename std::enable_if<
                internal::BufferCompat<uint8_t, T>::value>::type* = nullptr>
  CopyOnWriteBuffer(const T (&array)[N])  // NOLINT: runtime/explicit
      : CopyOnWriteBuffer(array, N) {}

  ~CopyOnWriteBuffer();

  // Get a pointer to the data. Just .data() will give you a (const) uint8_t*,
  // but you may also use .data<int8_t>() and .data<char>().
  template <typename T = uint8_t,
            typename std::enable_if<
                internal::BufferCompat<uint8_t, T>::value>::type* = nullptr>
  const T* data() const {
    return cdata<T>();
  }

  // Get writable pointer to the data. This will create a copy of the underlying
  // data if it is shared with other buffers.
  template <typename T = uint8_t,
            typename std::enable_if<
                internal::BufferCompat<uint8_t, T>::value>::type* = nullptr>
  T* data() {
    RTC_DCHECK(IsConsistent());
    if (!buffer_) {
      return nullptr;
    }
    CloneDataIfReferenced(buffer_->capacity());
    return buffer_->data<T>();
  }

  // Get const pointer to the data. This will not create a copy of the
  // underlying data if it is shared with other buffers.
  template <typename T = uint8_t,
            typename std::enable_if<
                internal::BufferCompat<uint8_t, T>::value>::type* = nullptr>
  const T* cdata() const {
    RTC_DCHECK(IsConsistent());
    if (!buffer_) {
      return nullptr;
    }
    return buffer_->data<T>();
  }

  size_t size() const {
    RTC_DCHECK(IsConsistent());
    return buffer_ ? buffer_->size() : 0;
  }

  size_t capacity() const {
    RTC_DCHECK(IsConsistent());
    return buffer_ ? buffer_->capacity() : 0;
  }

  CopyOnWriteBuffer& operator=(const CopyOnWriteBuffer& buf) {
    RTC_DCHECK(IsConsistent());
    RTC_DCHECK(buf.IsConsistent());
    if (&buf != this) {
      buffer_ = buf.buffer_;
    }
    return *this;
  }

  CopyOnWriteBuffer& operator=(CopyOnWriteBuffer&& buf) {
    RTC_DCHECK(IsConsistent());
    RTC_DCHECK(buf.IsConsistent());
    buffer_ = std::move(buf.buffer_);
    return *this;
  }

  bool operator==(const CopyOnWriteBuffer& buf) const;

  bool operator!=(const CopyOnWriteBuffer& buf) const {
    return !(*this == buf);
  }

  uint8_t& operator[](size_t index) {
    RTC_DCHECK_LT(index, size());
    return data()[index];
  }

  uint8_t operator[](size_t index) const {
    RTC_DCHECK_LT(index, size());
    return cdata()[index];
  }

  // Replace the contents of the buffer. Accepts the same types as the
  // constructors.
  template <typename T,
            typename std::enable_if<
                internal::BufferCompat<uint8_t, T>::value>::type* = nullptr>
  void SetData(const T* data, size_t size) {
    RTC_DCHECK(IsConsistent());
    if (!buffer_) {
      buffer_ = size > 0 ? new RefCountedObject<Buffer>(data, size) : nullptr;
    } else if (!buffer_->HasOneRef()) {
      buffer_ = new RefCountedObject<Buffer>(data, size, buffer_->capacity());
    } else {
      buffer_->SetData(data, size);
    }
    RTC_DCHECK(IsConsistent());
  }

  template <typename T,
            size_t N,
            typename std::enable_if<
                internal::BufferCompat<uint8_t, T>::value>::type* = nullptr>
  void SetData(const T (&array)[N]) {
    SetData(array, N);
  }

  void SetData(const CopyOnWriteBuffer& buf) {
    RTC_DCHECK(IsConsistent());
    RTC_DCHECK(buf.IsConsistent());
    if (&buf != this) {
      buffer_ = buf.buffer_;
    }
  }

  // Append data to the buffer. Accepts the same types as the constructors.
  template <typename T,
            typename std::enable_if<
                internal::BufferCompat<uint8_t, T>::value>::type* = nullptr>
  void AppendData(const T* data, size_t size) {
    RTC_DCHECK(IsConsistent());
    if (!buffer_) {
      buffer_ = new RefCountedObject<Buffer>(data, size);
      RTC_DCHECK(IsConsistent());
      return;
    }

    CloneDataIfReferenced(std::max(buffer_->capacity(),
        buffer_->size() + size));
    buffer_->AppendData(data, size);
    RTC_DCHECK(IsConsistent());
  }

  template <typename T,
            size_t N,
            typename std::enable_if<
                internal::BufferCompat<uint8_t, T>::value>::type* = nullptr>
  void AppendData(const T (&array)[N]) {
    AppendData(array, N);
  }

  void AppendData(const CopyOnWriteBuffer& buf) {
    AppendData(buf.data(), buf.size());
  }

  // Sets the size of the buffer. If the new size is smaller than the old, the
  // buffer contents will be kept but truncated; if the new size is greater,
  // the existing contents will be kept and the new space will be
  // uninitialized.
  void SetSize(size_t size);

  // Ensure that the buffer size can be increased to at least capacity without
  // further reallocation. (Of course, this operation might need to reallocate
  // the buffer.)
  void EnsureCapacity(size_t capacity);

  // Resets the buffer to zero size without altering capacity. Works even if the
  // buffer has been moved from.
  void Clear();

  // Swaps two buffers.
  friend void swap(CopyOnWriteBuffer& a, CopyOnWriteBuffer& b) {
    std::swap(a.buffer_, b.buffer_);
  }

 private:
  // Create a copy of the underlying data if it is referenced from other Buffer
  // objects.
  void CloneDataIfReferenced(size_t new_capacity);

  // Pre- and postcondition of all methods.
  bool IsConsistent() const {
    return (!buffer_ || buffer_->capacity() > 0);
  }

  // buffer_ is either null, or points to an rtc::Buffer with capacity > 0.
  scoped_refptr<RefCountedObject<Buffer>> buffer_;
};

}  // namespace rtc

#endif  // RTC_BASE_COPYONWRITEBUFFER_H_
