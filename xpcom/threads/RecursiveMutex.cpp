/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/RecursiveMutex.h"

#ifdef XP_WIN
#include <windows.h>

#define NativeHandle(m) (reinterpret_cast<CRITICAL_SECTION*>(&m))
#endif

namespace mozilla {

RecursiveMutex::RecursiveMutex(const char* aName MOZ_GUARD_OBJECT_NOTIFIER_PARAM_IN_IMPL)
  : BlockingResourceBase(aName, eRecursiveMutex)
#ifdef DEBUG
  , mOwningThread(nullptr)
  , mEntryCount(0)
#endif
{
  MOZ_GUARD_OBJECT_NOTIFIER_INIT;
#ifdef XP_WIN
  // This number was adapted from NSPR.
  static const DWORD sLockSpinCount = 100;

#if defined(RELEASE_OR_BETA)
  // Vista and later automatically allocate and subsequently leak a debug info
  // object for each critical section that we allocate unless we tell the
  // system not to do that.
  DWORD flags = CRITICAL_SECTION_NO_DEBUG_INFO;
#else
  DWORD flags = 0;
#endif
  BOOL r = InitializeCriticalSectionEx(NativeHandle(mMutex),
                                       sLockSpinCount, flags);
  MOZ_RELEASE_ASSERT(r);
#else
  pthread_mutexattr_t attr;

  MOZ_RELEASE_ASSERT(pthread_mutexattr_init(&attr) == 0,
                     "pthread_mutexattr_init failed");

  MOZ_RELEASE_ASSERT(pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE) == 0,
                     "pthread_mutexattr_settype failed");

  MOZ_RELEASE_ASSERT(pthread_mutex_init(&mMutex, &attr) == 0,
                     "pthread_mutex_init failed");

  MOZ_RELEASE_ASSERT(pthread_mutexattr_destroy(&attr) == 0,
                     "pthread_mutexattr_destroy failed");
#endif
}

RecursiveMutex::~RecursiveMutex()
{
#ifdef XP_WIN
  DeleteCriticalSection(NativeHandle(mMutex));
#else
  MOZ_RELEASE_ASSERT(pthread_mutex_destroy(&mMutex) == 0,
                     "pthread_mutex_destroy failed");
#endif
}

void
RecursiveMutex::LockInternal()
{
#ifdef XP_WIN
  EnterCriticalSection(NativeHandle(mMutex));
#else
  MOZ_RELEASE_ASSERT(pthread_mutex_lock(&mMutex) == 0,
                     "pthread_mutex_lock failed");
#endif
}

void
RecursiveMutex::UnlockInternal()
{
#ifdef XP_WIN
  LeaveCriticalSection(NativeHandle(mMutex));
#else
  MOZ_RELEASE_ASSERT(pthread_mutex_unlock(&mMutex) == 0,
                     "pthread_mutex_unlock failed");
#endif
}

} // namespace mozilla
