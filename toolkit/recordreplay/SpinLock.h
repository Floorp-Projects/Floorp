/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_recordreplay_SpinLock_h
#define mozilla_recordreplay_SpinLock_h

#include "mozilla/Assertions.h"
#include "mozilla/Atomics.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/GuardObjects.h"

#include <sched.h>

namespace mozilla {
namespace recordreplay {

// This file provides a couple of primitive lock implementations that are
// implemented using atomic operations. Using these locks does not write to any
// heap locations other than the lock's members, nor will it call any system
// locking APIs. These locks are used in places where reentrance into APIs
// needs to be avoided, or where writes to heap memory are not allowed.

// A basic spin lock.
class SpinLock
{
public:
  inline void Lock();
  inline void Unlock();

private:
  Atomic<bool, SequentiallyConsistent, Behavior::DontPreserve> mLocked;
};

// A basic read/write spin lock. This lock permits either multiple readers and
// no writers, or one writer.
class ReadWriteSpinLock
{
public:
  inline void ReadLock();
  inline void ReadUnlock();
  inline void WriteLock();
  inline void WriteUnlock();

private:
  SpinLock mLock; // Protects mReaders.
  int32_t mReaders; // -1 when in use for writing.
};

// RAII class to lock a spin lock.
struct MOZ_RAII AutoSpinLock
{
  explicit AutoSpinLock(SpinLock& aLock)
    : mLock(aLock)
  {
    mLock.Lock();
  }

  ~AutoSpinLock()
  {
    mLock.Unlock();
  }

private:
  SpinLock& mLock;
};

// RAII class to lock a read/write spin lock for reading.
struct AutoReadSpinLock
{
  explicit AutoReadSpinLock(ReadWriteSpinLock& aLock)
    : mLock(aLock)
  {
    mLock.ReadLock();
  }

  ~AutoReadSpinLock()
  {
    mLock.ReadUnlock();
  }

private:
  ReadWriteSpinLock& mLock;
};

// RAII class to lock a read/write spin lock for writing.
struct AutoWriteSpinLock
{
  explicit AutoWriteSpinLock(ReadWriteSpinLock& aLock)
    : mLock(aLock)
  {
    mLock.WriteLock();
  }

  ~AutoWriteSpinLock()
  {
    mLock.WriteUnlock();
  }

private:
  ReadWriteSpinLock& mLock;
};

///////////////////////////////////////////////////////////////////////////////
// Inline definitions
///////////////////////////////////////////////////////////////////////////////

// Try to yield execution to another thread.
static inline void
ThreadYield()
{
  sched_yield();
}

inline void
SpinLock::Lock()
{
  while (mLocked.exchange(true)) {
    ThreadYield();
  }
}

inline void
SpinLock::Unlock()
{
  DebugOnly<bool> rv = mLocked.exchange(false);
  MOZ_ASSERT(rv);
}

inline void
ReadWriteSpinLock::ReadLock()
{
  while (true) {
    AutoSpinLock ex(mLock);
    if (mReaders != -1) {
      mReaders++;
      return;
    }
  }
}

inline void
ReadWriteSpinLock::ReadUnlock()
{
  AutoSpinLock ex(mLock);
  MOZ_ASSERT(mReaders > 0);
  mReaders--;
}

inline void
ReadWriteSpinLock::WriteLock()
{
  while (true) {
    AutoSpinLock ex(mLock);
    if (mReaders == 0) {
      mReaders = -1;
      return;
    }
  }
}

inline void
ReadWriteSpinLock::WriteUnlock()
{
  AutoSpinLock ex(mLock);
  MOZ_ASSERT(mReaders == -1);
  mReaders = 0;
}

} // namespace recordreplay
} // namespace mozilla

#endif // mozilla_recordreplay_SpinLock_h
