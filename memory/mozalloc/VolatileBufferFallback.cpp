/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VolatileBuffer.h"
#include "mozilla/Assertions.h"
#include "mozilla/NullPtr.h"
#include "mozilla/mozalloc.h"

#ifdef MOZ_MEMORY
int posix_memalign(void** memptr, size_t alignment, size_t size);
#endif

namespace mozilla {

VolatileBuffer::VolatileBuffer()
  : mBuf(nullptr)
  , mSize(0)
  , mLockCount(0)
{
}

bool VolatileBuffer::Init(size_t aSize, size_t aAlignment)
{
  MOZ_ASSERT(!mSize && !mBuf, "Init called twice");
  MOZ_ASSERT(!(aAlignment % sizeof(void *)),
             "Alignment must be multiple of pointer size");

  mSize = aSize;
#if defined(MOZ_MEMORY)
  posix_memalign(&mBuf, aAlignment, aSize);
#elif defined(HAVE_POSIX_MEMALIGN)
  (void)moz_posix_memalign(&mBuf, aAlignment, aSize);
#else
#error "No memalign implementation found"
#endif
  return !!mBuf;
}

VolatileBuffer::~VolatileBuffer()
{
  free(mBuf);
}

bool
VolatileBuffer::Lock(void** aBuf)
{
  MOZ_ASSERT(mBuf, "Attempting to lock an uninitialized VolatileBuffer");

  *aBuf = mBuf;
  mLockCount++;

  return true;
}

void
VolatileBuffer::Unlock()
{
  mLockCount--;
  MOZ_ASSERT(mLockCount >= 0, "VolatileBuffer unlocked too many times!");
}

bool
VolatileBuffer::OnHeap() const
{
  return true;
}

size_t
VolatileBuffer::HeapSizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const
{
  return aMallocSizeOf(mBuf);
}

size_t
VolatileBuffer::NonHeapSizeOfExcludingThis() const
{
  return 0;
}

} // namespace mozilla
