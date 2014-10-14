/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VolatileBuffer.h"
#include "mozilla/Assertions.h"
#include "mozilla/NullPtr.h"
#include "mozilla/mozalloc.h"

#include <fcntl.h>
#include <linux/ashmem.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#ifdef MOZ_MEMORY
extern "C" int posix_memalign(void** memptr, size_t alignment, size_t size);
#endif

#define MIN_VOLATILE_ALLOC_SIZE 8192

namespace mozilla {

VolatileBuffer::VolatileBuffer()
  : mBuf(nullptr)
  , mSize(0)
  , mLockCount(0)
  , mFd(-1)
{
}

bool
VolatileBuffer::Init(size_t aSize, size_t aAlignment)
{
  MOZ_ASSERT(!mSize && !mBuf, "Init called twice");
  MOZ_ASSERT(!(aAlignment % sizeof(void *)),
         "Alignment must be multiple of pointer size");

  mSize = aSize;
  if (aSize < MIN_VOLATILE_ALLOC_SIZE) {
    goto heap_alloc;
  }

  mFd = open("/" ASHMEM_NAME_DEF, O_RDWR);
  if (mFd < 0) {
    goto heap_alloc;
  }

  if (ioctl(mFd, ASHMEM_SET_SIZE, mSize) < 0) {
    goto heap_alloc;
  }

  mBuf = mmap(nullptr, mSize, PROT_READ | PROT_WRITE, MAP_SHARED, mFd, 0);
  if (mBuf != MAP_FAILED) {
    return true;
  }

heap_alloc:
  mBuf = nullptr;
  if (mFd >= 0) {
    close(mFd);
    mFd = -1;
  }

#ifdef MOZ_MEMORY
  posix_memalign(&mBuf, aAlignment, aSize);
#else
  mBuf = memalign(aAlignment, aSize);
#endif
  return !!mBuf;
}

VolatileBuffer::~VolatileBuffer()
{
  if (OnHeap()) {
    free(mBuf);
  } else {
    munmap(mBuf, mSize);
    close(mFd);
  }
}

bool
VolatileBuffer::Lock(void** aBuf)
{
  MOZ_ASSERT(mBuf, "Attempting to lock an uninitialized VolatileBuffer");

  *aBuf = mBuf;
  if (++mLockCount > 1 || OnHeap()) {
    return true;
  }

  // Zero offset and zero length means we want to pin/unpin the entire thing.
  struct ashmem_pin pin = { 0, 0 };
  return ioctl(mFd, ASHMEM_PIN, &pin) == ASHMEM_NOT_PURGED;
}

void
VolatileBuffer::Unlock()
{
  MOZ_ASSERT(mLockCount > 0, "VolatileBuffer unlocked too many times!");
  if (--mLockCount || OnHeap()) {
    return;
  }

  struct ashmem_pin pin = { 0, 0 };
  ioctl(mFd, ASHMEM_UNPIN, &pin);
}

bool
VolatileBuffer::OnHeap() const
{
  return mFd < 0;
}

size_t
VolatileBuffer::HeapSizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const
{
  return OnHeap() ? aMallocSizeOf(mBuf) : 0;
}

size_t
VolatileBuffer::NonHeapSizeOfExcludingThis() const
{
  if (OnHeap()) {
    return 0;
  }

  return (mSize + (PAGE_SIZE - 1)) & PAGE_MASK;
}

} // namespace mozilla
