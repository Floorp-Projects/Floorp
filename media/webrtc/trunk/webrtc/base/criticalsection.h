/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_BASE_CRITICALSECTION_H_
#define WEBRTC_BASE_CRITICALSECTION_H_

#include "webrtc/base/atomicops.h"
#include "webrtc/base/constructormagic.h"
#include "webrtc/base/thread_annotations.h"

#if defined(WEBRTC_WIN)
// Include winsock2.h before including <windows.h> to maintain consistency with
// win32.h.  We can't include win32.h directly here since it pulls in
// headers such as basictypes.h which causes problems in Chromium where webrtc
// exists as two separate projects, webrtc and libjingle.
#include <winsock2.h>
#include <windows.h>
#include <sal.h>  // must come after windows headers.
#endif  // defined(WEBRTC_WIN)

#if defined(WEBRTC_POSIX)
#include <pthread.h>
#endif

#if (!defined(NDEBUG) || defined(DCHECK_ALWAYS_ON))
#define CS_DEBUG_CHECKS 1
#endif

#if CS_DEBUG_CHECKS
#define CS_DEBUG_CODE(x) x
#else  // !CS_DEBUG_CHECKS
#define CS_DEBUG_CODE(x)
#endif  // !CS_DEBUG_CHECKS

namespace rtc {

class LOCKABLE CriticalSection {
 public:
  CriticalSection();
  ~CriticalSection();

  void Enter() EXCLUSIVE_LOCK_FUNCTION();
  bool TryEnter() EXCLUSIVE_TRYLOCK_FUNCTION(true);
  void Leave() UNLOCK_FUNCTION();

  // Use only for RTC_DCHECKing.
  bool CurrentThreadIsOwner() const;
  // Use only for RTC_DCHECKing.
  bool IsLocked() const;

 private:
#if defined(WEBRTC_WIN)
  CRITICAL_SECTION crit_;
#elif defined(WEBRTC_POSIX)
  pthread_mutex_t mutex_;
  CS_DEBUG_CODE(pthread_t thread_);
  CS_DEBUG_CODE(int recursion_count_);
#endif
};

// CritScope, for serializing execution through a scope.
class SCOPED_LOCKABLE CritScope {
 public:
  explicit CritScope(CriticalSection* cs) EXCLUSIVE_LOCK_FUNCTION(cs);
  ~CritScope() UNLOCK_FUNCTION();
 private:
  CriticalSection* const cs_;
  RTC_DISALLOW_COPY_AND_ASSIGN(CritScope);
};

// Tries to lock a critical section on construction via
// CriticalSection::TryEnter, and unlocks on destruction if the
// lock was taken. Never blocks.
//
// IMPORTANT: Unlike CritScope, the lock may not be owned by this thread in
// subsequent code. Users *must* check locked() to determine if the
// lock was taken. If you're not calling locked(), you're doing it wrong!
class TryCritScope {
 public:
  explicit TryCritScope(CriticalSection* cs);
  ~TryCritScope();
#if defined(WEBRTC_WIN)
  _Check_return_ bool locked() const;
#else
  bool locked() const __attribute__ ((__warn_unused_result__));
#endif
 private:
  CriticalSection* const cs_;
  const bool locked_;
  CS_DEBUG_CODE(mutable bool lock_was_called_);
  RTC_DISALLOW_COPY_AND_ASSIGN(TryCritScope);
};

// A POD lock used to protect global variables. Do NOT use for other purposes.
// No custom constructor or private data member should be added.
class LOCKABLE GlobalLockPod {
 public:
  void Lock() EXCLUSIVE_LOCK_FUNCTION();

  void Unlock() UNLOCK_FUNCTION();

  volatile int lock_acquired;
};

class GlobalLock : public GlobalLockPod {
 public:
  GlobalLock();
};

// GlobalLockScope, for serializing execution through a scope.
class SCOPED_LOCKABLE GlobalLockScope {
 public:
  explicit GlobalLockScope(GlobalLockPod* lock) EXCLUSIVE_LOCK_FUNCTION(lock);
  ~GlobalLockScope() UNLOCK_FUNCTION();
 private:
  GlobalLockPod* const lock_;
  RTC_DISALLOW_COPY_AND_ASSIGN(GlobalLockScope);
};

} // namespace rtc

#endif // WEBRTC_BASE_CRITICALSECTION_H_
