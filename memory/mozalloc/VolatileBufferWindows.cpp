/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if defined(XP_WIN)
#  define MOZALLOC_EXPORT __declspec(dllexport)
#endif

#include "VolatileBuffer.h"
#include "mozilla/Assertions.h"
#include "mozilla/mozalloc.h"
#include "mozilla/WindowsVersion.h"

#include <windows.h>

#ifdef MOZ_MEMORY
extern "C" int posix_memalign(void** memptr, size_t alignment, size_t size);
#endif

#ifndef MEM_RESET_UNDO
#define MEM_RESET_UNDO 0x1000000
#endif

#define MIN_VOLATILE_ALLOC_SIZE 8192

namespace mozilla {

VolatileBuffer::VolatileBuffer()
  : mBuf(nullptr)
  , mSize(0)
  , mLockCount(0)
  , mHeap(false)
  , mFirstLock(true)
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

  static bool sUndoSupported = IsWin8OrLater();
  if (!sUndoSupported) {
    goto heap_alloc;
  }

  mBuf = VirtualAllocEx(GetCurrentProcess(),
                        nullptr,
                        mSize,
                        MEM_COMMIT | MEM_RESERVE,
                        PAGE_READWRITE);
  if (mBuf) {
    return true;
  }

heap_alloc:
#ifdef MOZ_MEMORY
  posix_memalign(&mBuf, aAlignment, aSize);
#else
  mBuf = _aligned_malloc(aSize, aAlignment);
#endif
  mHeap = true;
  return !!mBuf;
}

VolatileBuffer::~VolatileBuffer()
{
  if (OnHeap()) {
#ifdef MOZ_MEMORY
    free(mBuf);
#else
    _aligned_free(mBuf);
#endif
  } else {
    VirtualFreeEx(GetCurrentProcess(), mBuf, 0, MEM_RELEASE);
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

  // MEM_RESET_UNDO's behavior is undefined when called on memory that
  // hasn't been MEM_RESET.
  if (mFirstLock) {
    mFirstLock = false;
    return true;
  }

  void* addr = VirtualAllocEx(GetCurrentProcess(),
                              mBuf,
                              mSize,
                              MEM_RESET_UNDO,
                              PAGE_READWRITE);
  return !!addr;
}

void
VolatileBuffer::Unlock()
{
  MOZ_ASSERT(mLockCount > 0, "VolatileBuffer unlocked too many times!");
  if (--mLockCount || OnHeap()) {
    return;
  }

  void* addr = VirtualAllocEx(GetCurrentProcess(),
                              mBuf,
                              mSize,
                              MEM_RESET,
                              PAGE_READWRITE);
  MOZ_ASSERT(addr, "Failed to MEM_RESET");
}

bool
VolatileBuffer::OnHeap() const
{
  return mHeap;
}

size_t
VolatileBuffer::HeapSizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const
{
  if (OnHeap()) {
#ifdef MOZ_MEMORY
    return aMallocSizeOf(mBuf);
#else
    return mSize;
#endif
  }

  return 0;
}

size_t
VolatileBuffer::NonHeapSizeOfExcludingThis() const
{
  if (OnHeap()) {
    return 0;
  }

  return (mSize + 4095) & ~4095;
}

} // namespace mozilla
