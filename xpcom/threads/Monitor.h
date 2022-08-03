/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_Monitor_h
#define mozilla_Monitor_h

#include "mozilla/CondVar.h"
#include "mozilla/Mutex.h"

namespace mozilla {

/**
 * Monitor provides a *non*-reentrant monitor: *not* a Java-style
 * monitor.  If your code needs support for reentrancy, use
 * ReentrantMonitor instead.  (Rarely should reentrancy be needed.)
 *
 * Instead of directly calling Monitor methods, it's safer and simpler
 * to instead use the RAII wrappers MonitorAutoLock and
 * MonitorAutoUnlock.
 */
class MOZ_CAPABILITY Monitor {
 public:
  explicit Monitor(const char* aName)
      : mMutex(aName), mCondVar(mMutex, "[Monitor.mCondVar]") {}

  ~Monitor() = default;

  void Lock() MOZ_CAPABILITY_ACQUIRE() { mMutex.Lock(); }
  [[nodiscard]] bool TryLock() MOZ_TRY_ACQUIRE(true) {
    return mMutex.TryLock();
  }
  void Unlock() MOZ_CAPABILITY_RELEASE() { mMutex.Unlock(); }

  void Wait() MOZ_REQUIRES(this) { mCondVar.Wait(); }
  CVStatus Wait(TimeDuration aDuration) MOZ_REQUIRES(this) {
    return mCondVar.Wait(aDuration);
  }

  void Notify() { mCondVar.Notify(); }
  void NotifyAll() { mCondVar.NotifyAll(); }

  void AssertCurrentThreadOwns() const MOZ_ASSERT_CAPABILITY(this) {
    mMutex.AssertCurrentThreadOwns();
  }
  void AssertNotCurrentThreadOwns() const MOZ_ASSERT_CAPABILITY(!this) {
    mMutex.AssertNotCurrentThreadOwns();
  }

 private:
  Monitor() = delete;
  Monitor(const Monitor&) = delete;
  Monitor& operator=(const Monitor&) = delete;

  Mutex mMutex;
  CondVar mCondVar;
};

/**
 * MonitorSingleWriter
 *
 * Monitor where a single writer exists, so that reads from the same thread
 * will not generate data races or consistency issues.
 *
 * When possible, use MonitorAutoLock/MonitorAutoUnlock to lock/unlock this
 * monitor within a scope, instead of calling Lock/Unlock directly.
 *
 * This requires an object implementing Mutex's SingleWriterLockOwner, so
 * we can do correct-thread checks.
 */

class MonitorSingleWriter : public Monitor {
 public:
  // aOwner should be the object that contains the mutex, typically.  We
  // will use that object (which must have a lifetime the same or greater
  // than this object) to verify that we're running on the correct thread,
  // typically only in DEBUG builds
  explicit MonitorSingleWriter(const char* aName, SingleWriterLockOwner* aOwner)
      : Monitor(aName)
#ifdef DEBUG
        ,
        mOwner(aOwner)
#endif
  {
    MOZ_COUNT_CTOR(MonitorSingleWriter);
    MOZ_ASSERT(mOwner);
  }

  MOZ_COUNTED_DTOR(MonitorSingleWriter)

  void AssertOnWritingThread() const MOZ_ASSERT_CAPABILITY(this) {
    MOZ_ASSERT(mOwner->OnWritingThread());
  }
  void AssertOnWritingThreadOrHeld() const MOZ_ASSERT_CAPABILITY(this) {
#ifdef DEBUG
    if (!mOwner->OnWritingThread()) {
      AssertCurrentThreadOwns();
    }
#endif
  }

 private:
#ifdef DEBUG
  SingleWriterLockOwner* mOwner MOZ_UNSAFE_REF(
      "This is normally the object that contains the MonitorSingleWriter, so "
      "we don't want to hold a reference to ourselves");
#endif

  MonitorSingleWriter() = delete;
  MonitorSingleWriter(const MonitorSingleWriter&) = delete;
  MonitorSingleWriter& operator=(const MonitorSingleWriter&) = delete;
};

/**
 * Lock the monitor for the lexical scope instances of this class are
 * bound to (except for MonitorAutoUnlock in nested scopes).
 *
 * The monitor must be unlocked when instances of this class are
 * created.
 */
namespace detail {
template <typename T>
class MOZ_SCOPED_CAPABILITY MOZ_STACK_CLASS BaseMonitorAutoLock {
 public:
  explicit BaseMonitorAutoLock(T& aMonitor) MOZ_CAPABILITY_ACQUIRE(aMonitor)
      : mMonitor(&aMonitor) {
    mMonitor->Lock();
  }

  ~BaseMonitorAutoLock() MOZ_CAPABILITY_RELEASE() { mMonitor->Unlock(); }
  // It's very hard to mess up MonitorAutoLock lock(mMonitor); ... lock.Wait().
  // The only way you can fail to hold the lock when you call lock.Wait() is to
  // use MonitorAutoUnlock.   For now we'll ignore that case.
  void Wait() {
    mMonitor->AssertCurrentThreadOwns();
    mMonitor->Wait();
  }
  CVStatus Wait(TimeDuration aDuration) {
    mMonitor->AssertCurrentThreadOwns();
    return mMonitor->Wait(aDuration);
  }

  void Notify() { mMonitor->Notify(); }
  void NotifyAll() { mMonitor->NotifyAll(); }

  // Assert that aLock is the monitor passed to the constructor and that the
  // current thread owns the monitor.  In coding patterns such as:
  //
  // void LockedMethod(const BaseAutoLock<T>& aProofOfLock)
  // {
  //   aProofOfLock.AssertOwns(mMonitor);
  //   ...
  // }
  //
  // Without this assertion, it could be that mMonitor is not actually
  // locked. It's possible to have code like:
  //
  // BaseAutoLock lock(someMonitor);
  // ...
  // BaseAutoUnlock unlock(someMonitor);
  // ...
  // LockedMethod(lock);
  //
  // and in such a case, simply asserting that the monitor pointers match is not
  // sufficient; monitor ownership must be asserted as well.
  //
  // Note that if you are going to use the coding pattern presented above, you
  // should use this method in preference to using AssertCurrentThreadOwns on
  // the mutex you expected to be held, since this method provides stronger
  // guarantees.
  void AssertOwns(const T& aMonitor) const MOZ_ASSERT_CAPABILITY(aMonitor) {
    MOZ_ASSERT(&aMonitor == mMonitor);
    mMonitor->AssertCurrentThreadOwns();
  }

 private:
  BaseMonitorAutoLock() = delete;
  BaseMonitorAutoLock(const BaseMonitorAutoLock&) = delete;
  BaseMonitorAutoLock& operator=(const BaseMonitorAutoLock&) = delete;
  static void* operator new(size_t) noexcept(true);

  friend class MonitorAutoUnlock;

 protected:
  T* mMonitor;
};
}  // namespace detail
typedef detail::BaseMonitorAutoLock<Monitor> MonitorAutoLock;
typedef detail::BaseMonitorAutoLock<MonitorSingleWriter>
    MonitorSingleWriterAutoLock;

// clang-format off
// Use if we've done AssertOnWritingThread(), and then later need to take the
// lock to write to a protected member. Instead of
//    MutexSingleWriterAutoLock lock(mutex)
// use
//    MutexSingleWriterAutoLockOnThread(lock, mutex)
// clang-format on
#define MonitorSingleWriterAutoLockOnThread(lock, monitor) \
  MOZ_PUSH_IGNORE_THREAD_SAFETY                            \
  MonitorSingleWriterAutoLock lock(monitor);               \
  MOZ_POP_THREAD_SAFETY

/**
 * Unlock the monitor for the lexical scope instances of this class
 * are bound to (except for MonitorAutoLock in nested scopes).
 *
 * The monitor must be locked by the current thread when instances of
 * this class are created.
 */
namespace detail {
template <typename T>
class MOZ_STACK_CLASS MOZ_SCOPED_CAPABILITY BaseMonitorAutoUnlock {
 public:
  explicit BaseMonitorAutoUnlock(T& aMonitor)
      MOZ_SCOPED_UNLOCK_RELEASE(aMonitor)
      : mMonitor(&aMonitor) {
    mMonitor->Unlock();
  }

  ~BaseMonitorAutoUnlock() MOZ_SCOPED_UNLOCK_REACQUIRE() { mMonitor->Lock(); }

 private:
  BaseMonitorAutoUnlock() = delete;
  BaseMonitorAutoUnlock(const BaseMonitorAutoUnlock&) = delete;
  BaseMonitorAutoUnlock& operator=(const BaseMonitorAutoUnlock&) = delete;
  static void* operator new(size_t) noexcept(true);

  T* mMonitor;
};
}  // namespace detail
typedef detail::BaseMonitorAutoUnlock<Monitor> MonitorAutoUnlock;
typedef detail::BaseMonitorAutoUnlock<MonitorSingleWriter>
    MonitorSingleWriterAutoUnlock;

/**
 * Lock the monitor for the lexical scope instances of this class are
 * bound to (except for MonitorAutoUnlock in nested scopes).
 *
 * The monitor must be unlocked when instances of this class are
 * created.
 */
class MOZ_SCOPED_CAPABILITY MOZ_STACK_CLASS ReleasableMonitorAutoLock {
 public:
  explicit ReleasableMonitorAutoLock(Monitor& aMonitor)
      MOZ_CAPABILITY_ACQUIRE(aMonitor)
      : mMonitor(&aMonitor) {
    mMonitor->Lock();
    mLocked = true;
  }

  ~ReleasableMonitorAutoLock() MOZ_CAPABILITY_RELEASE() {
    if (mLocked) {
      mMonitor->Unlock();
    }
  }

  // See BaseMonitorAutoLock::Wait
  void Wait() {
    mMonitor->AssertCurrentThreadOwns();  // someone could have called Unlock()
    mMonitor->Wait();
  }
  CVStatus Wait(TimeDuration aDuration) {
    mMonitor->AssertCurrentThreadOwns();
    return mMonitor->Wait(aDuration);
  }

  void Notify() {
    MOZ_ASSERT(mLocked);
    mMonitor->Notify();
  }
  void NotifyAll() {
    MOZ_ASSERT(mLocked);
    mMonitor->NotifyAll();
  }

  // Allow dropping the lock prematurely; for example to support something like:
  // clang-format off
  // MonitorAutoLock lock(mMonitor);
  // ...
  // if (foo) {
  //   lock.Unlock();
  //   MethodThatCantBeCalledWithLock()
  //   return;
  // }
  // clang-format on
  void Unlock() MOZ_CAPABILITY_RELEASE() {
    MOZ_ASSERT(mLocked);
    mMonitor->Unlock();
    mLocked = false;
  }
  void Lock() MOZ_CAPABILITY_ACQUIRE() {
    MOZ_ASSERT(!mLocked);
    mMonitor->Lock();
    mLocked = true;
  }
  void AssertCurrentThreadOwns() const MOZ_ASSERT_CAPABILITY() {
    mMonitor->AssertCurrentThreadOwns();
  }

 private:
  bool mLocked;
  Monitor* mMonitor;

  ReleasableMonitorAutoLock() = delete;
  ReleasableMonitorAutoLock(const ReleasableMonitorAutoLock&) = delete;
  ReleasableMonitorAutoLock& operator=(const ReleasableMonitorAutoLock&) =
      delete;
  static void* operator new(size_t) noexcept(true);
};

}  // namespace mozilla

#endif  // mozilla_Monitor_h
