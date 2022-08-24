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
#  include "mozilla/Assertions.h"
#  include <os/lock.h>
#else
#  include <pthread.h>
#endif
#include "mozilla/Attributes.h"
#include "mozilla/ThreadSafety.h"

#if defined(XP_DARWIN)
// For information about the following undocumented flags and functions see
// https://github.com/apple/darwin-xnu/blob/main/bsd/sys/ulock.h and
// https://github.com/apple/darwin-libplatform/blob/main/private/os/lock_private.h
#  define OS_UNFAIR_LOCK_DATA_SYNCHRONIZATION (0x00010000)
#  define OS_UNFAIR_LOCK_ADAPTIVE_SPIN (0x00040000)

extern "C" {

typedef uint32_t os_unfair_lock_options_t;
OS_UNFAIR_LOCK_AVAILABILITY
OS_EXPORT OS_NOTHROW OS_NONNULL_ALL void os_unfair_lock_lock_with_options(
    os_unfair_lock_t lock, os_unfair_lock_options_t options);
}
#endif  // defined(XP_DARWIN)

// Mutexes based on spinlocks.  We can't use normal pthread spinlocks in all
// places, because they require malloc()ed memory, which causes bootstrapping
// issues in some cases.  We also can't use constructors, because for statics,
// they would fire after the first use of malloc, resetting the locks.
struct MOZ_CAPABILITY Mutex {
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

  inline void Lock() MOZ_CAPABILITY_ACQUIRE() {
#if defined(XP_WIN)
    EnterCriticalSection(&mMutex);
#elif defined(XP_DARWIN)
    // We rely on a non-public function to improve performance here.
    // The OS_UNFAIR_LOCK_DATA_SYNCHRONIZATION flag informs the kernel that
    // the calling thread is able to make progress even in absence of actions
    // from other threads and the OS_UNFAIR_LOCK_ADAPTIVE_SPIN one causes the
    // kernel to spin on a contested lock if the owning thread is running on
    // the same physical core (presumably only on x86 CPUs given that ARM
    // macs don't have cores capable of SMT). On versions of macOS older than
    // 10.15 the latter is not available and we spin in userspace instead.
    if (Mutex::gSpinInKernelSpace) {
      os_unfair_lock_lock_with_options(
          &mMutex,
          OS_UNFAIR_LOCK_DATA_SYNCHRONIZATION | OS_UNFAIR_LOCK_ADAPTIVE_SPIN);
    } else {
#  if defined(__x86_64__)
      // On older versions of macOS (10.14 and older) the
      // `OS_UNFAIR_LOCK_ADAPTIVE_SPIN` flag is not supported by the kernel,
      // we spin in user-space instead like `OSSpinLock` does:
      // https://github.com/apple/darwin-libplatform/blob/215b09856ab5765b7462a91be7076183076600df/src/os/lock.c#L183-L198
      // Note that `OSSpinLock` uses 1000 iterations on x86-64:
      // https://github.com/apple/darwin-libplatform/blob/215b09856ab5765b7462a91be7076183076600df/src/os/lock.c#L93
      // ...but we only use 100 like it does on ARM:
      // https://github.com/apple/darwin-libplatform/blob/215b09856ab5765b7462a91be7076183076600df/src/os/lock.c#L90
      // We choose this value because it yields the same results in our
      // benchmarks but is less likely to have detrimental effects caused by
      // excessive spinning.
      uint32_t retries = 100;

      do {
        if (os_unfair_lock_trylock(&mMutex)) {
          return;
        }

        __asm__ __volatile__("pause");
      } while (retries--);

      os_unfair_lock_lock_with_options(&mMutex,
                                       OS_UNFAIR_LOCK_DATA_SYNCHRONIZATION);
#  else
      MOZ_CRASH("User-space spin-locks should never be used on ARM");
#  endif  // defined(__x86_64__)
    }
#else
    pthread_mutex_lock(&mMutex);
#endif
  }

  inline void Unlock() MOZ_CAPABILITY_RELEASE() {
#if defined(XP_WIN)
    LeaveCriticalSection(&mMutex);
#elif defined(XP_DARWIN)
    os_unfair_lock_unlock(&mMutex);
#else
    pthread_mutex_unlock(&mMutex);
#endif
  }

#if defined(XP_DARWIN)
  static bool SpinInKernelSpace();
  static bool gSpinInKernelSpace;
#endif  // XP_DARWIN
};

// Mutex that can be used for static initialization.
// On Windows, CRITICAL_SECTION requires a function call to be initialized,
// but for the initialization lock, a static initializer calling the
// function would be called too late. We need no-function-call
// initialization, which SRWLock provides.
// Ideally, we'd use the same type of locks everywhere, but SRWLocks
// everywhere incur a performance penalty. See bug 1418389.
#if defined(XP_WIN)
struct MOZ_CAPABILITY StaticMutex {
  SRWLOCK mMutex;

  inline void Lock() MOZ_CAPABILITY_ACQUIRE() {
    AcquireSRWLockExclusive(&mMutex);
  }

  inline void Unlock() MOZ_CAPABILITY_RELEASE() {
    ReleaseSRWLockExclusive(&mMutex);
  }
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
struct MOZ_SCOPED_CAPABILITY MOZ_RAII AutoLock {
  explicit AutoLock(T& aMutex) MOZ_CAPABILITY_ACQUIRE(aMutex) : mMutex(aMutex) {
    mMutex.Lock();
  }

  ~AutoLock() MOZ_CAPABILITY_RELEASE() { mMutex.Unlock(); }

  AutoLock(const AutoLock&) = delete;
  AutoLock(AutoLock&&) = delete;

 private:
  T& mMutex;
};

using MutexAutoLock = AutoLock<Mutex>;

#endif
