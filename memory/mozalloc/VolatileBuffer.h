/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozalloc_VolatileBuffer_h
#define mozalloc_VolatileBuffer_h

#include "mozilla/mozalloc.h"
#include "mozilla/RefPtr.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/NullPtr.h"

/* VolatileBuffer
 *
 * This class represents a piece of memory that can potentially be reclaimed
 * by the OS when not in use. As long as there are one or more
 * VolatileBufferPtrs holding on to a VolatileBuffer, the memory will remain
 * available. However, when there are no VolatileBufferPtrs holding a
 * VolatileBuffer, the OS can purge the pages if it wants to. The OS can make
 * better decisions about what pages to purge than we can.
 *
 * VolatileBuffers may not always be volatile - if the allocation is too small,
 * or if the OS doesn't support the feature, or if the OS doesn't want to,
 * the buffer will be allocated on heap.
 *
 * VolatileBuffer allocations are fallible. They are intended for uses where
 * one may allocate large buffers for caching data. Init() must be called
 * exactly once.
 *
 * After getting a reference to VolatileBuffer using VolatileBufferPtr,
 * WasPurged() can be used to check if the OS purged any pages in the buffer.
 * The OS cannot purge a buffer immediately after a VolatileBuffer is
 * initialized. At least one VolatileBufferPtr must be created before the
 * buffer can be purged, so the first use of VolatileBufferPtr does not need
 * to check WasPurged().
 *
 * When a buffer is purged, some or all of the buffer is zeroed out. This
 * API cannot tell which parts of the buffer were lost.
 *
 * VolatileBuffer is not thread safe. Do not use VolatileBufferPtrs on
 * different threads.
 */

namespace mozilla {

class MOZALLOC_EXPORT VolatileBuffer : public RefCounted<VolatileBuffer>
{
  friend class VolatileBufferPtr_base;
public:
  MOZ_DECLARE_REFCOUNTED_TYPENAME(VolatileBuffer)
  VolatileBuffer();
  ~VolatileBuffer();

  /* aAlignment must be a multiple of the pointer size */
  bool Init(size_t aSize, size_t aAlignment = sizeof(void*));

  size_t HeapSizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const;
  size_t NonHeapSizeOfExcludingThis() const;
  bool OnHeap() const;

protected:
  bool Lock(void** aBuf);
  void Unlock();

private:
  void* mBuf;
  size_t mSize;
  int mLockCount;
#if defined(ANDROID)
  int mFd;
#elif defined(XP_DARWIN)
  bool mHeap;
#elif defined(XP_WIN)
  bool mHeap;
  bool mFirstLock;
#endif
};

class VolatileBufferPtr_base {
public:
  explicit VolatileBufferPtr_base(VolatileBuffer* vbuf) : mVBuf(vbuf) {
    Lock();
  }

  ~VolatileBufferPtr_base() {
    Unlock();
  }

  bool WasBufferPurged() const {
    return mPurged;
  }

protected:
  void* mMapping;

  void Set(VolatileBuffer* vbuf) {
    Unlock();
    mVBuf = vbuf;
    Lock();
  }

private:
  RefPtr<VolatileBuffer> mVBuf;
  bool mPurged;

  void Lock() {
    if (mVBuf) {
      mPurged = !mVBuf->Lock(&mMapping);
    } else {
      mMapping = nullptr;
      mPurged = false;
    }
  }

  void Unlock() {
    if (mVBuf) {
      mVBuf->Unlock();
    }
  }
};

template <class T>
class VolatileBufferPtr : public VolatileBufferPtr_base
{
public:
  explicit VolatileBufferPtr(VolatileBuffer* vbuf) : VolatileBufferPtr_base(vbuf) {}
  VolatileBufferPtr() : VolatileBufferPtr_base(nullptr) {}

  operator T*() const {
    return (T*) mMapping;
  }

  void operator =(VolatileBuffer* vbuf) {
    Set(vbuf);
  }
private:
  VolatileBufferPtr(VolatileBufferPtr const& vbufptr) MOZ_DELETE;
};

}; /* namespace mozilla */

#endif /* mozalloc_VolatileBuffer_h */
