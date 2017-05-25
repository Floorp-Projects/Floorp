/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/RWLock.h"

#ifdef XP_WIN
#include <windows.h>

static_assert(sizeof(SRWLOCK) <= sizeof(void*), "SRWLOCK is too big!");

#define NativeHandle(m) (reinterpret_cast<SRWLOCK*>(&m))
#else
#define NativeHandle(m) (&m)
#endif

namespace mozilla {

RWLock::RWLock(const char* aName)
  : BlockingResourceBase(aName, eMutex)
#ifdef DEBUG
  , mOwningThread(nullptr)
#endif
{
#ifdef XP_WIN
  InitializeSRWLock(NativeHandle(mRWLock));
#else
  MOZ_RELEASE_ASSERT(pthread_rwlock_init(NativeHandle(mRWLock), nullptr) == 0,
                     "pthread_rwlock_init failed");
#endif
}

#ifdef DEBUG
bool
RWLock::LockedForWritingByCurrentThread()
{
  return mOwningThread == PR_GetCurrentThread();
}
#endif

#ifndef XP_WIN
RWLock::~RWLock()
{
  MOZ_RELEASE_ASSERT(pthread_rwlock_destroy(NativeHandle(mRWLock)) == 0,
                     "pthread_rwlock_destroy failed");
}
#endif

void
RWLock::ReadLockInternal()
{
#ifdef XP_WIN
  AcquireSRWLockShared(NativeHandle(mRWLock));
#else
  MOZ_RELEASE_ASSERT(pthread_rwlock_rdlock(NativeHandle(mRWLock)) == 0,
                     "pthread_rwlock_rdlock failed");
#endif
}

void
RWLock::ReadUnlockInternal()
{
#ifdef XP_WIN
  ReleaseSRWLockShared(NativeHandle(mRWLock));
#else
  MOZ_RELEASE_ASSERT(pthread_rwlock_unlock(NativeHandle(mRWLock)) == 0,
                     "pthread_rwlock_unlock failed");
#endif
}

void
RWLock::WriteLockInternal()
{
#ifdef XP_WIN
  AcquireSRWLockExclusive(NativeHandle(mRWLock));
#else
  MOZ_RELEASE_ASSERT(pthread_rwlock_wrlock(NativeHandle(mRWLock)) == 0,
                     "pthread_rwlock_wrlock failed");
#endif
}

void
RWLock::WriteUnlockInternal()
{
#ifdef XP_WIN
  ReleaseSRWLockExclusive(NativeHandle(mRWLock));
#else
  MOZ_RELEASE_ASSERT(pthread_rwlock_unlock(NativeHandle(mRWLock)) == 0,
                     "pthread_rwlock_unlock failed");
#endif
}

}

#undef NativeHandle
