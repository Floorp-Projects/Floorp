/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef Mutex_h
#define Mutex_h

#if defined(XP_WIN)
#  include <windows.h>
#elif defined(XP_DARWIN)
#  include <os/lock.h>
#else
#  include <pthread.h>
#endif
#include "mozilla/Attributes.h"

// Mutexes based on spinlocks.  We can't use normal pthread spinlocks in all
// places, because they require malloc()ed memory, which causes bootstrapping
// issues in some cases.  We also can't use constructors, because for statics,
// they would fire after the first use of malloc, resetting the locks.
struct Mutex {
#if defined(XP_WIN)
  CRITICAL_SECTION mMutex;
#elif defined(XP_DARWIN)
  os_unfair_lock mMutex;
#else
  pthread_mutex_t mMutex;
#endif

  // Initializes a mutex. Returns whether initialization succeeded.
  inline bool Init() {
#if defined(XP_WIN)
    if (!InitializeCriticalSectionAndSpinCount(&mMutex, 5000)) {
      return false;
    }
#elif defined(XP_DARWIN)
    mMutex = OS_UNFAIR_LOCK_INIT;
#elif defined(XP_LINUX) && !defined(ANDROID)
    pthread_mutexattr_t attr;
    if (pthread_mutexattr_init(&attr) != 0) {
      return false;
    }
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ADAPTIVE_NP);
    if (pthread_mutex_init(&mMutex, &attr) != 0) {
      pthread_mutexattr_destroy(&attr);
      return false;
    }
    pthread_mutexattr_destroy(&attr);
#else
    if (pthread_mutex_init(&mMutex, nullptr) != 0) {
      return false;
    }
#endif
    return true;
  }

  inline void Lock() {
#if defined(XP_WIN)
    EnterCriticalSection(&mMutex);
#elif defined(XP_DARWIN)
    os_unfair_lock_lock(&mMutex);
#else
    pthread_mutex_lock(&mMutex);
#endif
  }

  inline void Unlock() {
#if defined(XP_WIN)
    LeaveCriticalSection(&mMutex);
#elif defined(XP_DARWIN)
    os_unfair_lock_unlock(&mMutex);
#else
    pthread_mutex_unlock(&mMutex);
#endif
  }
};

// Mutex that can be used for static initialization.
// On Windows, CRITICAL_SECTION requires a function call to be initialized,
// but for the initialization lock, a static initializer calling the
// function would be called too late. We need no-function-call
// initialization, which SRWLock provides.
// Ideally, we'd use the same type of locks everywhere, but SRWLocks
// everywhere incur a performance penalty. See bug 1418389.
#if defined(XP_WIN)
struct StaticMutex {
  SRWLOCK mMutex;

  inline void Lock() { AcquireSRWLockExclusive(&mMutex); }

  inline void Unlock() { ReleaseSRWLockExclusive(&mMutex); }
};

// Normally, we'd use a constexpr constructor, but MSVC likes to create
// static initializers anyways.
#  define STATIC_MUTEX_INIT SRWLOCK_INIT

#else
typedef Mutex StaticMutex;

#  if defined(XP_DARWIN)
#    define STATIC_MUTEX_INIT OS_UNFAIR_LOCK_INIT
#  elif defined(XP_LINUX) && !defined(ANDROID)
#    define STATIC_MUTEX_INIT PTHREAD_ADAPTIVE_MUTEX_INITIALIZER_NP
#  else
#    define STATIC_MUTEX_INIT PTHREAD_MUTEX_INITIALIZER
#  endif

#endif

template <typename T>
struct MOZ_RAII AutoLock {
  explicit AutoLock(T& aMutex) : mMutex(aMutex) { mMutex.Lock(); }

  ~AutoLock() { mMutex.Unlock(); }

 private:
  T& mMutex;
};

using MutexAutoLock = AutoLock<Mutex>;

#endif
