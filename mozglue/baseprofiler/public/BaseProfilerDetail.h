/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Internal Base Profiler utilities.

#ifndef BaseProfilerDetail_h
#define BaseProfilerDetail_h

#include "mozilla/Atomics.h"
#include "mozilla/Maybe.h"
#include "mozilla/PlatformMutex.h"
#include "mozilla/BaseProfilerUtils.h"

namespace mozilla {
namespace baseprofiler {

namespace detail {

// Thin shell around mozglue PlatformMutex, for Base Profiler internal use.
class BaseProfilerMutex : private ::mozilla::detail::MutexImpl {
 public:
  BaseProfilerMutex() : ::mozilla::detail::MutexImpl() {}
  explicit BaseProfilerMutex(const char* aName)
      : ::mozilla::detail::MutexImpl(), mName(aName) {}

  BaseProfilerMutex(const BaseProfilerMutex&) = delete;
  BaseProfilerMutex& operator=(const BaseProfilerMutex&) = delete;
  BaseProfilerMutex(BaseProfilerMutex&&) = delete;
  BaseProfilerMutex& operator=(BaseProfilerMutex&&) = delete;

#ifdef DEBUG
  ~BaseProfilerMutex() {
    MOZ_ASSERT(!BaseProfilerThreadId::FromNumber(mOwningThreadId).IsSpecified(),
               "BaseProfilerMutex should have been unlocked when destroyed");
  }
#endif  // DEBUG

  [[nodiscard]] bool IsLockedOnCurrentThread() const {
    return BaseProfilerThreadId::FromNumber(mOwningThreadId) ==
           baseprofiler::profiler_current_thread_id();
  }

  void AssertCurrentThreadOwns() const {
    MOZ_ASSERT(IsLockedOnCurrentThread());
  }

  void Lock() {
    const BaseProfilerThreadId tid = baseprofiler::profiler_current_thread_id();
    MOZ_ASSERT(tid.IsSpecified());
    MOZ_ASSERT(!IsLockedOnCurrentThread(), "Recursive locking");
    ::mozilla::detail::MutexImpl::lock();
    MOZ_ASSERT(!BaseProfilerThreadId::FromNumber(mOwningThreadId).IsSpecified(),
               "Not unlocked properly");
    mOwningThreadId = tid.ToNumber();
  }

  [[nodiscard]] bool TryLock() {
    const BaseProfilerThreadId tid = baseprofiler::profiler_current_thread_id();
    MOZ_ASSERT(tid.IsSpecified());
    MOZ_ASSERT(!IsLockedOnCurrentThread(), "Recursive locking");
    if (!::mozilla::detail::MutexImpl::tryLock()) {
      // Failed to lock, nothing more to do.
      return false;
    }
    MOZ_ASSERT(!BaseProfilerThreadId::FromNumber(mOwningThreadId).IsSpecified(),
               "Not unlocked properly");
    mOwningThreadId = tid.ToNumber();
    return true;
  }

  void Unlock() {
    MOZ_ASSERT(IsLockedOnCurrentThread(), "Unlocking when not locked here");
    // We're still holding the mutex here, so it's safe to just reset
    // `mOwningThreadId`.
    mOwningThreadId = BaseProfilerThreadId{}.ToNumber();
    ::mozilla::detail::MutexImpl::unlock();
  }

  const char* GetName() const { return mName; }

 private:
  // Thread currently owning the lock, or 0.
  // Atomic because it may be read at any time independent of the mutex.
  // Relaxed because threads only need to know if they own it already, so:
  // - If it's their id, only *they* wrote that value with a locked mutex.
  // - If it's different from their thread id it doesn't matter what other
  //   number it is (0 or another id) and that it can change again at any time.
  Atomic<typename BaseProfilerThreadId::NumberType, MemoryOrdering::Relaxed>
      mOwningThreadId;

  const char* mName = nullptr;
};

// RAII class to lock a mutex.
class MOZ_RAII BaseProfilerAutoLock {
 public:
  explicit BaseProfilerAutoLock(BaseProfilerMutex& aMutex) : mMutex(aMutex) {
    mMutex.Lock();
  }

  BaseProfilerAutoLock(const BaseProfilerAutoLock&) = delete;
  BaseProfilerAutoLock& operator=(const BaseProfilerAutoLock&) = delete;
  BaseProfilerAutoLock(BaseProfilerAutoLock&&) = delete;
  BaseProfilerAutoLock& operator=(BaseProfilerAutoLock&&) = delete;

  ~BaseProfilerAutoLock() { mMutex.Unlock(); }

 private:
  BaseProfilerMutex& mMutex;
};

// Thin shell around mozglue PlatformMutex, for Base Profiler internal use.
// Actual mutex may be disabled at construction time.
class BaseProfilerMaybeMutex : private ::mozilla::detail::MutexImpl {
 public:
  explicit BaseProfilerMaybeMutex(bool aActivate) {
    if (aActivate) {
      mMaybeMutex.emplace();
    }
  }

  BaseProfilerMaybeMutex(const BaseProfilerMaybeMutex&) = delete;
  BaseProfilerMaybeMutex& operator=(const BaseProfilerMaybeMutex&) = delete;
  BaseProfilerMaybeMutex(BaseProfilerMaybeMutex&&) = delete;
  BaseProfilerMaybeMutex& operator=(BaseProfilerMaybeMutex&&) = delete;

  ~BaseProfilerMaybeMutex() = default;

  bool IsActivated() const { return mMaybeMutex.isSome(); }

  [[nodiscard]] bool IsActivatedAndLockedOnCurrentThread() const {
    if (!IsActivated()) {
      // Not activated, so we can never be locked.
      return false;
    }
    return mMaybeMutex->IsLockedOnCurrentThread();
  }

  void AssertCurrentThreadOwns() const {
#ifdef DEBUG
    if (IsActivated()) {
      mMaybeMutex->AssertCurrentThreadOwns();
    }
#endif  // DEBUG
  }

  void Lock() {
    if (IsActivated()) {
      mMaybeMutex->Lock();
    }
  }

  void Unlock() {
    if (IsActivated()) {
      mMaybeMutex->Unlock();
    }
  }

 private:
  Maybe<BaseProfilerMutex> mMaybeMutex;
};

// RAII class to lock a mutex.
class MOZ_RAII BaseProfilerMaybeAutoLock {
 public:
  explicit BaseProfilerMaybeAutoLock(BaseProfilerMaybeMutex& aMaybeMutex)
      : mMaybeMutex(aMaybeMutex) {
    mMaybeMutex.Lock();
  }

  BaseProfilerMaybeAutoLock(const BaseProfilerMaybeAutoLock&) = delete;
  BaseProfilerMaybeAutoLock& operator=(const BaseProfilerMaybeAutoLock&) =
      delete;
  BaseProfilerMaybeAutoLock(BaseProfilerMaybeAutoLock&&) = delete;
  BaseProfilerMaybeAutoLock& operator=(BaseProfilerMaybeAutoLock&&) = delete;

  ~BaseProfilerMaybeAutoLock() { mMaybeMutex.Unlock(); }

 private:
  BaseProfilerMaybeMutex& mMaybeMutex;
};

}  // namespace detail
}  // namespace baseprofiler
}  // namespace mozilla

#endif  // BaseProfilerDetail_h
