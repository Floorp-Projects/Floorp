/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_Mutex_h
#define mozilla_Mutex_h

#include "mozilla/BlockingResourceBase.h"
#include "mozilla/ThreadSafety.h"
#include "mozilla/PlatformMutex.h"
#include "mozilla/Maybe.h"
#include "nsISupports.h"

//
// Provides:
//
//  - Mutex, a non-recursive mutex
//  - MutexAutoLock, an RAII class for ensuring that Mutexes are properly
//    locked and unlocked
//  - MutexAutoUnlock, complementary sibling to MutexAutoLock
//
//  - OffTheBooksMutex, a non-recursive mutex that doesn't do leak checking
//  - OffTheBooksMutexAuto{Lock,Unlock} - Like MutexAuto{Lock,Unlock}, but for
//    an OffTheBooksMutex.
//
// Using MutexAutoLock/MutexAutoUnlock etc. is MUCH preferred to making bare
// calls to Lock and Unlock.
//
namespace mozilla {

/**
 * OffTheBooksMutex is identical to Mutex, except that OffTheBooksMutex doesn't
 * include leak checking.  Sometimes you want to intentionally "leak" a mutex
 * until shutdown; in these cases, OffTheBooksMutex is for you.
 */
class MOZ_CAPABILITY("mutex") OffTheBooksMutex : public detail::MutexImpl,
                                                 BlockingResourceBase {
 public:
  /**
   * @param aName A name which can reference this lock
   * @returns If failure, nullptr
   *          If success, a valid Mutex* which must be destroyed
   *          by Mutex::DestroyMutex()
   **/
  explicit OffTheBooksMutex(const char* aName)
      : BlockingResourceBase(aName, eMutex)
#ifdef DEBUG
        ,
        mOwningThread(nullptr)
#endif
  {
  }

  ~OffTheBooksMutex() {
#ifdef DEBUG
    MOZ_ASSERT(!mOwningThread, "destroying a still-owned lock!");
#endif
  }

#ifndef DEBUG
  /**
   * Lock this mutex.
   **/
  void Lock() MOZ_CAPABILITY_ACQUIRE() { this->lock(); }

  /**
   * Try to lock this mutex, returning true if we were successful.
   **/
  [[nodiscard]] bool TryLock() MOZ_TRY_ACQUIRE(true) { return this->tryLock(); }

  /**
   * Unlock this mutex.
   **/
  void Unlock() MOZ_CAPABILITY_RELEASE() { this->unlock(); }

  /**
   * Assert that the current thread owns this mutex in debug builds.
   *
   * Does nothing in non-debug builds.
   **/
  void AssertCurrentThreadOwns() const MOZ_ASSERT_CAPABILITY(this) {}

  /**
   * Assert that the current thread does not own this mutex.
   *
   * Note that this function is not implemented for debug builds *and*
   * non-debug builds due to difficulties in dealing with memory ordering.
   *
   * It is therefore mostly useful as documentation.
   **/
  void AssertNotCurrentThreadOwns() const MOZ_ASSERT_CAPABILITY(!this) {}

#else
  void Lock() MOZ_CAPABILITY_ACQUIRE();

  [[nodiscard]] bool TryLock() MOZ_TRY_ACQUIRE(true);
  void Unlock() MOZ_CAPABILITY_RELEASE();

  void AssertCurrentThreadOwns() const MOZ_ASSERT_CAPABILITY(this);
  void AssertNotCurrentThreadOwns() const MOZ_ASSERT_CAPABILITY(!this) {
    // FIXME bug 476536
  }
#endif  // ifndef DEBUG

 private:
  OffTheBooksMutex() = delete;
  OffTheBooksMutex(const OffTheBooksMutex&) = delete;
  OffTheBooksMutex& operator=(const OffTheBooksMutex&) = delete;

  friend class OffTheBooksCondVar;

#ifdef DEBUG
  PRThread* mOwningThread;
#endif
};

/**
 * Mutex
 * When possible, use MutexAutoLock/MutexAutoUnlock to lock/unlock this
 * mutex within a scope, instead of calling Lock/Unlock directly.
 */
class Mutex : public OffTheBooksMutex {
 public:
  explicit Mutex(const char* aName) : OffTheBooksMutex(aName) {
    MOZ_COUNT_CTOR(Mutex);
  }

  MOZ_COUNTED_DTOR(Mutex)

 private:
  Mutex() = delete;
  Mutex(const Mutex&) = delete;
  Mutex& operator=(const Mutex&) = delete;
};

/**
 * MutexSingleWriter
 *
 * Mutex where a single writer exists, so that reads from the same thread
 * will not generate data races or consistency issues.
 *
 * When possible, use MutexAutoLock/MutexAutoUnlock to lock/unlock this
 * mutex within a scope, instead of calling Lock/Unlock directly.
 *
 * This requires an object implementing Mutex's SingleWriterLockOwner, so
 * we can do correct-thread checks.
 */
// Subclass this in the object owning the mutex
class SingleWriterLockOwner {
 public:
  SingleWriterLockOwner() = default;
  ~SingleWriterLockOwner() = default;

  virtual bool OnWritingThread() const = 0;
};

class MutexSingleWriter : public OffTheBooksMutex {
 public:
  // aOwner should be the object that contains the mutex, typically.  We
  // will use that object (which must have a lifetime the same or greater
  // than this object) to verify that we're running on the correct thread,
  // typically only in DEBUG builds
  explicit MutexSingleWriter(const char* aName, SingleWriterLockOwner* aOwner)
      : OffTheBooksMutex(aName)
#ifdef DEBUG
        ,
        mOwner(aOwner)
#endif
  {
    MOZ_COUNT_CTOR(MutexSingleWriter);
    MOZ_ASSERT(mOwner);
  }

  MOZ_COUNTED_DTOR(MutexSingleWriter)

  /**
   * Statically assert that we're on the only thread that modifies data
   * guarded by this Mutex.  This allows static checking for the pattern of
   * having a single thread modify a set of data, and read it (under lock)
   * on other threads, and reads on the thread that modifies it doesn't
   * require a lock.  This doesn't solve the issue of some data under the
   * Mutex following this pattern, and other data under the mutex being
   * written from multiple threads.
   *
   * We could set the writing thread and dynamically check it in debug
   * builds, but this doesn't.  We could also use thread-safety/capability
   * system to provide direct thread assertions.
   **/
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

  MutexSingleWriter() = delete;
  MutexSingleWriter(const MutexSingleWriter&) = delete;
  MutexSingleWriter& operator=(const MutexSingleWriter&) = delete;
};

namespace detail {
template <typename T>
class MOZ_RAII BaseAutoUnlock;

/**
 * MutexAutoLock
 * Acquires the Mutex when it enters scope, and releases it when it leaves
 * scope.
 *
 * MUCH PREFERRED to bare calls to Mutex.Lock and Unlock.
 */
template <typename T>
class MOZ_RAII MOZ_SCOPED_CAPABILITY BaseAutoLock {
 public:
  /**
   * Constructor
   * The constructor aquires the given lock.  The destructor
   * releases the lock.
   *
   * @param aLock A valid mozilla::Mutex* returned by
   *              mozilla::Mutex::NewMutex.
   **/
  explicit BaseAutoLock(T aLock) MOZ_CAPABILITY_ACQUIRE(aLock) : mLock(aLock) {
    mLock.Lock();
  }

  ~BaseAutoLock(void) MOZ_CAPABILITY_RELEASE() { mLock.Unlock(); }

  // Assert that aLock is the mutex passed to the constructor and that the
  // current thread owns the mutex.  In coding patterns such as:
  //
  // void LockedMethod(const BaseAutoLock<T>& aProofOfLock)
  // {
  //   aProofOfLock.AssertOwns(mMutex);
  //   ...
  // }
  //
  // Without this assertion, it could be that mMutex is not actually
  // locked. It's possible to have code like:
  //
  // BaseAutoLock lock(someMutex);
  // ...
  // BaseAutoUnlock unlock(someMutex);
  // ...
  // LockedMethod(lock);
  //
  // and in such a case, simply asserting that the mutex pointers match is not
  // sufficient; mutex ownership must be asserted as well.
  //
  // Note that if you are going to use the coding pattern presented above, you
  // should use this method in preference to using AssertCurrentThreadOwns on
  // the mutex you expected to be held, since this method provides stronger
  // guarantees.
  void AssertOwns(const T& aMutex) const MOZ_ASSERT_CAPABILITY(aMutex) {
    MOZ_ASSERT(&aMutex == &mLock);
    mLock.AssertCurrentThreadOwns();
  }

 private:
  BaseAutoLock() = delete;
  BaseAutoLock(BaseAutoLock&) = delete;
  BaseAutoLock& operator=(BaseAutoLock&) = delete;
  static void* operator new(size_t) noexcept(true);

  friend class BaseAutoUnlock<T>;

  T mLock;
};

template <typename MutexType>
BaseAutoLock(MutexType&) -> BaseAutoLock<MutexType&>;
}  // namespace detail

typedef detail::BaseAutoLock<Mutex&> MutexAutoLock;
typedef detail::BaseAutoLock<MutexSingleWriter&> MutexSingleWriterAutoLock;
typedef detail::BaseAutoLock<OffTheBooksMutex&> OffTheBooksMutexAutoLock;

// Specialization of Maybe<*AutoLock> for space efficiency and to silence
// thread-safety analysis, which cannot track what's going on.
template <class MutexType>
class Maybe<detail::BaseAutoLock<MutexType&>> {
 public:
  Maybe() : mLock(nullptr) {}
  ~Maybe() MOZ_NO_THREAD_SAFETY_ANALYSIS {
    if (mLock) {
      mLock->Unlock();
    }
  }

  constexpr bool isSome() const { return mLock; }
  constexpr bool isNothing() const { return !mLock; }

  void emplace(MutexType& aMutex) MOZ_NO_THREAD_SAFETY_ANALYSIS {
    MOZ_RELEASE_ASSERT(!mLock);
    mLock = &aMutex;
    mLock->Lock();
  }

  void reset() MOZ_NO_THREAD_SAFETY_ANALYSIS {
    if (mLock) {
      mLock->Unlock();
      mLock = nullptr;
    }
  }

 private:
  MutexType* mLock;
};

// Use if we've done AssertOnWritingThread(), and then later need to take the
// lock to write to a protected member. Instead of
//    MutexSingleWriterAutoLock lock(mutex)
// use
//    MutexSingleWriterAutoLockOnThread(lock, mutex)
#define MutexSingleWriterAutoLockOnThread(lock, mutex) \
  MOZ_PUSH_IGNORE_THREAD_SAFETY                        \
  MutexSingleWriterAutoLock lock(mutex);               \
  MOZ_POP_THREAD_SAFETY

namespace detail {
/**
 * ReleasableMutexAutoLock
 * Acquires the Mutex when it enters scope, and releases it when it leaves
 * scope. Allows calling Unlock (and Lock) as an alternative to
 * MutexAutoUnlock; this can avoid an extra lock/unlock pair.
 *
 */
template <typename T>
class MOZ_RAII MOZ_SCOPED_CAPABILITY ReleasableBaseAutoLock {
 public:
  /**
   * Constructor
   * The constructor aquires the given lock.  The destructor
   * releases the lock.
   *
   * @param aLock A valid mozilla::Mutex& returned by
   *              mozilla::Mutex::NewMutex.
   **/
  explicit ReleasableBaseAutoLock(T aLock) MOZ_CAPABILITY_ACQUIRE(aLock)
      : mLock(aLock) {
    mLock.Lock();
    mLocked = true;
  }

  ~ReleasableBaseAutoLock(void) MOZ_CAPABILITY_RELEASE() {
    if (mLocked) {
      Unlock();
    }
  }

  void AssertOwns(const T& aMutex) const MOZ_ASSERT_CAPABILITY(aMutex) {
    MOZ_ASSERT(&aMutex == &mLock);
    mLock.AssertCurrentThreadOwns();
  }

  // Allow dropping the lock prematurely; for example to support something like:
  // clang-format off
  // MutexAutoLock lock(mMutex);
  // ...
  // if (foo) {
  //   lock.Unlock();
  //   MethodThatCantBeCalledWithLock()
  //   return;
  // }
  // clang-format on
  void Unlock() MOZ_CAPABILITY_RELEASE() {
    MOZ_ASSERT(mLocked);
    mLock.Unlock();
    mLocked = false;
  }
  void Lock() MOZ_CAPABILITY_ACQUIRE() {
    MOZ_ASSERT(!mLocked);
    mLock.Lock();
    mLocked = true;
  }

 private:
  ReleasableBaseAutoLock() = delete;
  ReleasableBaseAutoLock(ReleasableBaseAutoLock&) = delete;
  ReleasableBaseAutoLock& operator=(ReleasableBaseAutoLock&) = delete;
  static void* operator new(size_t) noexcept(true);

  bool mLocked;
  T mLock;
};

template <typename MutexType>
ReleasableBaseAutoLock(MutexType&) -> ReleasableBaseAutoLock<MutexType&>;
}  // namespace detail

typedef detail::ReleasableBaseAutoLock<Mutex&> ReleasableMutexAutoLock;

namespace detail {
/**
 * BaseAutoUnlock
 * Releases the Mutex when it enters scope, and re-acquires it when it leaves
 * scope.
 *
 * MUCH PREFERRED to bare calls to Mutex.Unlock and Lock.
 */
template <typename T>
class MOZ_RAII MOZ_SCOPED_CAPABILITY BaseAutoUnlock {
 public:
  explicit BaseAutoUnlock(T aLock) MOZ_SCOPED_UNLOCK_RELEASE(aLock)
      : mLock(aLock) {
    mLock.Unlock();
  }

  explicit BaseAutoUnlock(BaseAutoLock<T>& aAutoLock)
      /* MOZ_CAPABILITY_RELEASE(aAutoLock.mLock) */
      : mLock(aAutoLock.mLock) {
    NS_ASSERTION(mLock, "null lock");
    mLock->Unlock();
  }

  ~BaseAutoUnlock() MOZ_SCOPED_UNLOCK_REACQUIRE() { mLock.Lock(); }

 private:
  BaseAutoUnlock() = delete;
  BaseAutoUnlock(BaseAutoUnlock&) = delete;
  BaseAutoUnlock& operator=(BaseAutoUnlock&) = delete;
  static void* operator new(size_t) noexcept(true);

  T mLock;
};

template <typename MutexType>
BaseAutoUnlock(MutexType&) -> BaseAutoUnlock<MutexType&>;
}  // namespace detail

typedef detail::BaseAutoUnlock<Mutex&> MutexAutoUnlock;
typedef detail::BaseAutoUnlock<MutexSingleWriter&> MutexSingleWriterAutoUnlock;
typedef detail::BaseAutoUnlock<OffTheBooksMutex&> OffTheBooksMutexAutoUnlock;

namespace detail {
/**
 * BaseAutoTryLock
 * Tries to acquire the Mutex when it enters scope, and releases it when it
 * leaves scope.
 *
 * MUCH PREFERRED to bare calls to Mutex.TryLock and Unlock.
 */
template <typename T>
class MOZ_RAII MOZ_SCOPED_CAPABILITY BaseAutoTryLock {
 public:
  explicit BaseAutoTryLock(T& aLock) MOZ_CAPABILITY_ACQUIRE(aLock)
      : mLock(aLock.TryLock() ? &aLock : nullptr) {}

  ~BaseAutoTryLock() MOZ_CAPABILITY_RELEASE() {
    if (mLock) {
      mLock->Unlock();
      mLock = nullptr;
    }
  }

  explicit operator bool() const { return mLock; }

 private:
  BaseAutoTryLock(BaseAutoTryLock&) = delete;
  BaseAutoTryLock& operator=(BaseAutoTryLock&) = delete;
  static void* operator new(size_t) noexcept(true);

  T* mLock;
};
}  // namespace detail

typedef detail::BaseAutoTryLock<Mutex> MutexAutoTryLock;
typedef detail::BaseAutoTryLock<OffTheBooksMutex> OffTheBooksMutexAutoTryLock;

}  // namespace mozilla

#endif  // ifndef mozilla_Mutex_h
