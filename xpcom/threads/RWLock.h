/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// An interface for read-write locks.

#ifndef mozilla_RWLock_h
#define mozilla_RWLock_h

#include "mozilla/Assertions.h"
#include "mozilla/Atomics.h"
#include "mozilla/BlockingResourceBase.h"
#include "mozilla/PlatformRWLock.h"

namespace mozilla {

// A RWLock is similar to a Mutex, but whereas a Mutex permits only a single
// reader thread or a single writer thread to access a piece of data, a
// RWLock distinguishes between readers and writers: you may have multiple
// reader threads concurrently accessing a piece of data or a single writer
// thread.  This difference should guide your usage of RWLock: if you are not
// reading the data from multiple threads simultaneously or you are writing
// to the data roughly as often as read from it, then Mutex will suit your
// purposes just fine.
//
// You should be using the AutoReadLock and AutoWriteLock classes, below,
// for RAII read locking and write locking, respectively.  If you really must
// take a read lock manually, call the ReadLock method; to relinquish that
// read lock, call the ReadUnlock method.  Similarly, WriteLock and WriteUnlock
// perform the same operations, but for write locks.
//
// It is unspecified what happens when a given thread attempts to acquire the
// same lock in multiple ways; some underlying implementations of RWLock do
// support acquiring a read lock multiple times on a given thread, but you
// should not rely on this behavior.
//
// It is unspecified whether RWLock gives priority to waiting readers or
// a waiting writer when unlocking.
class RWLock : public detail::RWLockImpl, public BlockingResourceBase {
 public:
  explicit RWLock(const char* aName);

#ifdef DEBUG
  bool LockedForWritingByCurrentThread();
  [[nodiscard]] bool TryReadLock();
  void ReadLock();
  void ReadUnlock();
  [[nodiscard]] bool TryWriteLock();
  void WriteLock();
  void WriteUnlock();
#else
  [[nodiscard]] bool TryReadLock() { return detail::RWLockImpl::tryReadLock(); }
  void ReadLock() { detail::RWLockImpl::readLock(); }
  void ReadUnlock() { detail::RWLockImpl::readUnlock(); }
  [[nodiscard]] bool TryWriteLock() {
    return detail::RWLockImpl::tryWriteLock();
  }
  void WriteLock() { detail::RWLockImpl::writeLock(); }
  void WriteUnlock() { detail::RWLockImpl::writeUnlock(); }
#endif

 private:
  RWLock() = delete;
  RWLock(const RWLock&) = delete;
  RWLock& operator=(const RWLock&) = delete;

#ifdef DEBUG
  // We record the owning thread for write locks only.
  PRThread* mOwningThread;
#endif
};

template <typename T>
class MOZ_RAII BaseAutoTryReadLock {
 public:
  explicit BaseAutoTryReadLock(T& aLock)
      : mLock(aLock.TryReadLock() ? &aLock : nullptr) {}

  ~BaseAutoTryReadLock() {
    if (mLock) {
      mLock->ReadUnlock();
    }
  }

  explicit operator bool() const { return mLock; }

 private:
  BaseAutoTryReadLock() = delete;
  BaseAutoTryReadLock(const BaseAutoTryReadLock&) = delete;
  BaseAutoTryReadLock& operator=(const BaseAutoTryReadLock&) = delete;

  T* mLock;
};

template <typename T>
class MOZ_RAII BaseAutoReadLock {
 public:
  explicit BaseAutoReadLock(T& aLock) : mLock(&aLock) {
    MOZ_ASSERT(mLock, "null lock");
    mLock->ReadLock();
  }

  ~BaseAutoReadLock() { mLock->ReadUnlock(); }

 private:
  BaseAutoReadLock() = delete;
  BaseAutoReadLock(const BaseAutoReadLock&) = delete;
  BaseAutoReadLock& operator=(const BaseAutoReadLock&) = delete;

  T* mLock;
};

template <typename T>
class MOZ_RAII BaseAutoTryWriteLock {
 public:
  explicit BaseAutoTryWriteLock(T& aLock)
      : mLock(aLock.TryWriteLock() ? &aLock : nullptr) {}

  ~BaseAutoTryWriteLock() {
    if (mLock) {
      mLock->WriteUnlock();
    }
  }

  explicit operator bool() const { return mLock; }

 private:
  BaseAutoTryWriteLock() = delete;
  BaseAutoTryWriteLock(const BaseAutoTryWriteLock&) = delete;
  BaseAutoTryWriteLock& operator=(const BaseAutoTryWriteLock&) = delete;

  T* mLock;
};

template <typename T>
class MOZ_RAII BaseAutoWriteLock final {
 public:
  explicit BaseAutoWriteLock(T& aLock) : mLock(&aLock) {
    MOZ_ASSERT(mLock, "null lock");
    mLock->WriteLock();
  }

  ~BaseAutoWriteLock() { mLock->WriteUnlock(); }

 private:
  BaseAutoWriteLock() = delete;
  BaseAutoWriteLock(const BaseAutoWriteLock&) = delete;
  BaseAutoWriteLock& operator=(const BaseAutoWriteLock&) = delete;

  T* mLock;
};

// Read try-lock and unlock a RWLock with RAII semantics.  Much preferred to
// bare calls to TryReadLock() and ReadUnlock().
typedef BaseAutoTryReadLock<RWLock> AutoTryReadLock;

// Read lock and unlock a RWLock with RAII semantics.  Much preferred to bare
// calls to ReadLock() and ReadUnlock().
typedef BaseAutoReadLock<RWLock> AutoReadLock;

// Write try-lock and unlock a RWLock with RAII semantics.  Much preferred to
// bare calls to TryWriteLock() and WriteUnlock().
typedef BaseAutoTryWriteLock<RWLock> AutoTryWriteLock;

// Write lock and unlock a RWLock with RAII semantics.  Much preferred to bare
// calls to WriteLock() and WriteUnlock().
typedef BaseAutoWriteLock<RWLock> AutoWriteLock;

// XXX: normally we would define StaticRWLock as
// MOZ_ONLY_USED_TO_AVOID_STATIC_CONSTRUCTORS, but the contexts in which it
// is used (e.g. member variables in a third-party library) are non-trivial
// to modify to properly declare everything at static scope.  As those
// third-party libraries are the only clients, put it behind the detail
// namespace to discourage other (possibly erroneous) uses from popping up.

namespace detail {

class StaticRWLock {
 public:
  // In debug builds, check that mLock is initialized for us as we expect by
  // the compiler.  In non-debug builds, don't declare a constructor so that
  // the compiler can see that the constructor is trivial.
#ifdef DEBUG
  StaticRWLock() { MOZ_ASSERT(!mLock); }
#endif

  [[nodiscard]] bool TryReadLock() { return Lock()->TryReadLock(); }
  void ReadLock() { Lock()->ReadLock(); }
  void ReadUnlock() { Lock()->ReadUnlock(); }
  [[nodiscard]] bool TryWriteLock() { return Lock()->TryWriteLock(); }
  void WriteLock() { Lock()->WriteLock(); }
  void WriteUnlock() { Lock()->WriteUnlock(); }

 private:
  [[nodiscard]] RWLock* Lock() {
    if (mLock) {
      return mLock;
    }

    RWLock* lock = new RWLock("StaticRWLock");
    if (!mLock.compareExchange(nullptr, lock)) {
      delete lock;
    }

    return mLock;
  }

  Atomic<RWLock*> mLock;

  // Disallow copy constructor, but only in debug mode.  We only define
  // a default constructor in debug mode (see above); if we declared
  // this constructor always, the compiler wouldn't generate a trivial
  // default constructor for us in non-debug mode.
#ifdef DEBUG
  StaticRWLock(const StaticRWLock& aOther);
#endif

  // Disallow these operators.
  StaticRWLock& operator=(StaticRWLock* aRhs) = delete;
  static void* operator new(size_t) noexcept(true) = delete;
  static void operator delete(void*) = delete;
};

typedef BaseAutoTryReadLock<StaticRWLock> StaticAutoTryReadLock;
typedef BaseAutoReadLock<StaticRWLock> StaticAutoReadLock;
typedef BaseAutoTryWriteLock<StaticRWLock> StaticAutoTryWriteLock;
typedef BaseAutoWriteLock<StaticRWLock> StaticAutoWriteLock;

}  // namespace detail

}  // namespace mozilla

#endif  // mozilla_RWLock_h
