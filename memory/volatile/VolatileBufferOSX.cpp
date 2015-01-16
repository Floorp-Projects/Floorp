/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VolatileBuffer.h"
#include "mozilla/Assertions.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/mozalloc.h"

#include <mach/mach.h>
#include <sys/mman.h>
#include <unistd.h>

#define MIN_VOLATILE_ALLOC_SIZE 8192

namespace mozilla {

VolatileBuffer::VolatileBuffer()
  : mMutex("VolatileBuffer")
  , mBuf(nullptr)
  , mSize(0)
  , mLockCount(0)
  , mHeap(false)
{
}

bool
VolatileBuffer::Init(size_t aSize, size_t aAlignment)
{
  MOZ_ASSERT(!mSize && !mBuf, "Init called twice");
  MOZ_ASSERT(!(aAlignment % sizeof(void *)),
             "Alignment must be multiple of pointer size");

  mSize = aSize;

  kern_return_t ret = 0;
  if (aSize < MIN_VOLATILE_ALLOC_SIZE) {
    goto heap_alloc;
  }

  ret = vm_allocate(mach_task_self(),
                    (vm_address_t*)&mBuf,
                    mSize,
                    VM_FLAGS_PURGABLE | VM_FLAGS_ANYWHERE);
  if (ret == KERN_SUCCESS) {
    return true;
  }

heap_alloc:
  (void)moz_posix_memalign(&mBuf, aAlignment, aSize);
  mHeap = true;
  return !!mBuf;
}

VolatileBuffer::~VolatileBuffer()
{
  MOZ_ASSERT(mLockCount == 0, "Being destroyed with non-zero lock count?");

  if (OnHeap()) {
    free(mBuf);
  } else {
    vm_deallocate(mach_task_self(), (vm_address_t)mBuf, mSize);
  }
}

bool
VolatileBuffer::Lock(void** aBuf)
{
  MutexAutoLock lock(mMutex);

  MOZ_ASSERT(mBuf, "Attempting to lock an uninitialized VolatileBuffer");

  *aBuf = mBuf;
  if (++mLockCount > 1 || OnHeap()) {
    return true;
  }

  int state = VM_PURGABLE_NONVOLATILE;
  kern_return_t ret =
    vm_purgable_control(mach_task_self(),
                        (vm_address_t)mBuf,
                        VM_PURGABLE_SET_STATE,
                        &state);
  return ret == KERN_SUCCESS && !(state & VM_PURGABLE_EMPTY);
}

void
VolatileBuffer::Unlock()
{
  MutexAutoLock lock(mMutex);

  MOZ_ASSERT(mLockCount > 0, "VolatileBuffer unlocked too many times!");
  if (--mLockCount || OnHeap()) {
    return;
  }

  int state = VM_PURGABLE_VOLATILE | VM_VOLATILE_GROUP_DEFAULT;
  DebugOnly<kern_return_t> ret =
    vm_purgable_control(mach_task_self(),
                        (vm_address_t)mBuf,
                        VM_PURGABLE_SET_STATE,
                        &state);
  MOZ_ASSERT(ret == KERN_SUCCESS, "Failed to set buffer as purgable");
}

bool
VolatileBuffer::OnHeap() const
{
  return mHeap;
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

  unsigned long pagemask = getpagesize() - 1;
  return (mSize + pagemask) & ~pagemask;
}

} // namespace mozilla
