/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// A class for values only accessible from a single designated thread.

#ifndef mozilla_ThreadBound_h
#define mozilla_ThreadBound_h

#include "mozilla/Atomics.h"
#include "prthread.h"

#include <type_traits>

namespace mozilla {

template <typename T>
class ThreadBound;

namespace detail {

template <bool Condition, typename T>
struct AddConstIf {
  using type = T;
};

template <typename T>
struct AddConstIf<true, T> {
  using type = typename std::add_const<T>::type;
};

}  // namespace detail

// A ThreadBound<T> is a T that can only be accessed by a specific
// thread. To enforce this rule, the inner T is only accessible
// through a non-copyable, immovable accessor object.
// Given a ThreadBound<T> threadBoundData, it can be accessed like so:
//
//   auto innerData = threadBoundData.Access();
//   innerData->DoStuff();
//
// Trying to access a ThreadBound<T> from a different thread will
// trigger a MOZ_DIAGNOSTIC_ASSERT.
//   The encapsulated T is constructed during the construction of the
// enclosing ThreadBound<T> by forwarding all of the latter's
// constructor parameters to the former. A newly constructed
// ThreadBound<T> is bound to the thread it's constructed in. It's
// possible to rebind the data to some otherThread by calling
//
//   threadBoundData.Transfer(otherThread);
//
// on the thread that threadBoundData is currently bound to, as long
// as it's not currently being accessed. (Trying to rebind from
// another thread or while an accessor exists will trigger an
// assertion.)
//
// Note: A ThreadBound<T> may be destructed from any thread, not just
// its designated thread at the time the destructor is invoked.
template <typename T>
class ThreadBound final {
 public:
  template <typename... Args>
  explicit ThreadBound(Args&&... aArgs)
      : mData(std::forward<Args>(aArgs)...),
        mThread(PR_GetCurrentThread()),
        mAccessCount(0) {}

  ~ThreadBound() { AssertIsNotCurrentlyAccessed(); }

  void Transfer(const PRThread* const aDest) {
    AssertIsCorrectThread();
    AssertIsNotCurrentlyAccessed();
    mThread = aDest;
  }

 private:
  T mData;

  // This member is (potentially) accessed by multiple threads and is
  // thus the first point of synchronization between them.
  Atomic<const PRThread*, ReleaseAcquire> mThread;

  // In order to support nested accesses (e.g. from different stack
  // frames) it's necessary to maintain a counter of the existing
  // accessor. Since it's possible to access a const ThreadBound, the
  // counter is mutable. It's atomic because accessing it synchronizes
  // access to mData (see comment in Accessor's constructor).
  using AccessCountType = Atomic<int, ReleaseAcquire>;
  mutable AccessCountType mAccessCount;

 public:
  template <bool IsConst>
  class MOZ_STACK_CLASS Accessor final {
    using DataType = typename detail::AddConstIf<IsConst, T>::type;

   public:
    explicit Accessor(
        typename detail::AddConstIf<IsConst, ThreadBound>::type& aThreadBound)
        : mData(aThreadBound.mData), mAccessCount(aThreadBound.mAccessCount) {
      aThreadBound.AssertIsCorrectThread();

      // This load/store serves as a memory fence that guards mData
      // against accesses that would trip the thread assertion.
      // (Otherwise one of the loads in the caller's instruction
      // stream might be scheduled before the assertion.)
      ++mAccessCount;
    }

    Accessor(const Accessor&) = delete;
    Accessor(Accessor&&) = delete;
    Accessor& operator=(const Accessor&) = delete;
    Accessor& operator=(Accessor&&) = delete;

    ~Accessor() { --mAccessCount; }

    DataType* operator->() { return &mData; }

   private:
    DataType& mData;
    AccessCountType& mAccessCount;
  };

  auto Access() { return Accessor<false>{*this}; }

  auto Access() const { return Accessor<true>{*this}; }

 private:
  bool IsCorrectThread() const { return mThread == PR_GetCurrentThread(); }

  bool IsNotCurrentlyAccessed() const { return mAccessCount == 0; }

#define MOZ_DEFINE_THREAD_BOUND_ASSERT(predicate) \
  void Assert##predicate() const { MOZ_DIAGNOSTIC_ASSERT(predicate()); }

  MOZ_DEFINE_THREAD_BOUND_ASSERT(IsCorrectThread)
  MOZ_DEFINE_THREAD_BOUND_ASSERT(IsNotCurrentlyAccessed)

#undef MOZ_DEFINE_THREAD_BOUND_ASSERT
};

}  // namespace mozilla

#endif  // mozilla_ThreadBound_h
