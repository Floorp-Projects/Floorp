/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Assertions.h"
#include "mozilla/Maybe.h"

#include <algorithm>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

#include "mozilla/PlatformMutex.h"
#include "MutexPlatformData_posix.h"

#define REPORT_PTHREADS_ERROR(result, msg)      \
  {                                             \
    errno = result;                             \
    perror(msg);                                \
    MOZ_CRASH(msg);                             \
  }

#define TRY_CALL_PTHREADS(call, msg)            \
  {                                             \
    int result = (call);                        \
    if (result != 0) {                          \
      REPORT_PTHREADS_ERROR(result, msg);       \
    }                                           \
  }

#ifdef XP_DARWIN

// CPU count. Read concurrently from multiple threads. Written once during the
// first mutex initialization; re-initialization is safe hence relaxed ordering
// is OK.
static mozilla::Atomic<uint32_t, mozilla::MemoryOrdering::Relaxed,
                       mozilla::recordreplay::Behavior::DontPreserve> sCPUCount(0);

static void
EnsureCPUCount()
{
  if (sCPUCount) {
    return;
  }

  // _SC_NPROCESSORS_CONF and _SC_NPROCESSORS_ONLN are common, but not
  // standard.
#if defined(_SC_NPROCESSORS_CONF)
  long n = sysconf(_SC_NPROCESSORS_CONF);
  sCPUCount = (n > 0) ? uint32_t(n) : 1;
#elif defined(_SC_NPROCESSORS_ONLN)
  long n = sysconf(_SC_NPROCESSORS_ONLN);
  sCPUCount = (n > 0) ? uint32_t(n) : 1;
#else
  sCPUCount = 1;
#endif
}

#endif // XP_DARWIN

mozilla::detail::MutexImpl::MutexImpl(recordreplay::Behavior aRecorded)
#ifdef XP_DARWIN
  : averageSpins(0)
#endif
{
  pthread_mutexattr_t* attrp = nullptr;

  mozilla::Maybe<mozilla::recordreplay::AutoEnsurePassThroughThreadEvents> pt;
  if (aRecorded == recordreplay::Behavior::DontPreserve) {
    pt.emplace();
  }

  // Linux with glibc and FreeBSD support adaptive mutexes that spin
  // for a short number of tries before sleeping.  NSPR's locks did
  // this, too, and it seems like a reasonable thing to do.
#if (defined(__linux__) && defined(__GLIBC__)) || defined(__FreeBSD__)
#define ADAPTIVE_MUTEX_SUPPORTED
#endif

#if defined(DEBUG)
#define ATTR_REQUIRED
#define MUTEX_KIND PTHREAD_MUTEX_ERRORCHECK
#elif defined(ADAPTIVE_MUTEX_SUPPORTED)
#define ATTR_REQUIRED
#define MUTEX_KIND PTHREAD_MUTEX_ADAPTIVE_NP
#endif

#if defined(ATTR_REQUIRED)
  pthread_mutexattr_t attr;

  TRY_CALL_PTHREADS(pthread_mutexattr_init(&attr),
                    "mozilla::detail::MutexImpl::MutexImpl: pthread_mutexattr_init failed");

  TRY_CALL_PTHREADS(pthread_mutexattr_settype(&attr, MUTEX_KIND),
                    "mozilla::detail::MutexImpl::MutexImpl: pthread_mutexattr_settype failed");
  attrp = &attr;
#endif

  TRY_CALL_PTHREADS(pthread_mutex_init(&platformData()->ptMutex, attrp),
                    "mozilla::detail::MutexImpl::MutexImpl: pthread_mutex_init failed");

#if defined(ATTR_REQUIRED)
  TRY_CALL_PTHREADS(pthread_mutexattr_destroy(&attr),
                    "mozilla::detail::MutexImpl::MutexImpl: pthread_mutexattr_destroy failed");
#endif

#ifdef XP_DARWIN
  EnsureCPUCount();
#endif
}

mozilla::detail::MutexImpl::~MutexImpl()
{
  TRY_CALL_PTHREADS(pthread_mutex_destroy(&platformData()->ptMutex),
                    "mozilla::detail::MutexImpl::~MutexImpl: pthread_mutex_destroy failed");
}

inline void
mozilla::detail::MutexImpl::mutexLock()
{
  TRY_CALL_PTHREADS(pthread_mutex_lock(&platformData()->ptMutex),
                    "mozilla::detail::MutexImpl::mutexLock: pthread_mutex_lock failed");
}

#ifdef XP_DARWIN
inline bool
mozilla::detail::MutexImpl::mutexTryLock()
{
  int result = pthread_mutex_trylock(&platformData()->ptMutex);
  if (result == 0) {
    return true;
  }

  if (result == EBUSY) {
    return false;
  }

  REPORT_PTHREADS_ERROR(result,
                        "mozilla::detail::MutexImpl::mutexTryLock: pthread_mutex_trylock failed");
}
#endif

void
mozilla::detail::MutexImpl::lock()
{
#ifndef XP_DARWIN
  mutexLock();
#else
  // Mutex performance on OSX can be very poor if there's a lot of contention as
  // this causes excessive context switching. On Linux/FreeBSD we use the
  // adaptive mutex type (PTHREAD_MUTEX_ADAPTIVE_NP) to address this, but this
  // isn't available on OSX. The code below is a reimplementation of this
  // feature.

  MOZ_ASSERT(sCPUCount);
  if (sCPUCount == 1 || recordreplay::IsRecordingOrReplaying()) {
    mutexLock();
    return;
  }

  if (!mutexTryLock()) {
    const int32_t SpinLimit = 100;

    int32_t count = 0;
    int32_t maxSpins = std::min(SpinLimit, 2 * averageSpins + 10);
    do {
      if (count >= maxSpins) {
        mutexLock();
        break;
      }
      asm("pause"); // Hint to the processor that we're spinning.
      count++;
    } while (!mutexTryLock());

    // Update moving average.
    averageSpins += (count - averageSpins) / 8;
    MOZ_ASSERT(averageSpins >= 0 && averageSpins <= SpinLimit);
  }
#endif // XP_DARWIN
}

void
mozilla::detail::MutexImpl::unlock()
{
  TRY_CALL_PTHREADS(pthread_mutex_unlock(&platformData()->ptMutex),
                    "mozilla::detail::MutexImpl::unlock: pthread_mutex_unlock failed");
}

#undef TRY_CALL_PTHREADS

mozilla::detail::MutexImpl::PlatformData*
mozilla::detail::MutexImpl::platformData()
{
  static_assert(sizeof(platformData_) >= sizeof(PlatformData),
                "platformData_ is too small");
  return reinterpret_cast<PlatformData*>(platformData_);
}
