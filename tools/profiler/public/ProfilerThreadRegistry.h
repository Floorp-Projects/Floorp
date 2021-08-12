/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ProfilerThreadRegistry_h
#define ProfilerThreadRegistry_h

#include "mozilla/BaseProfilerDetail.h"
#include "mozilla/ProfilerThreadRegistration.h"
#include "mozilla/Vector.h"

namespace mozilla::profiler {

class ThreadRegistry {
 private:
  using RegistryMutex = baseprofiler::detail::BaseProfilerMutex;
  using RegistryLock = baseprofiler::detail::BaseProfilerAutoLock;

 public:
  // Aliases to data accessors (removing the ThreadRegistration prefix).

  using UnlockedConstReader = ThreadRegistrationUnlockedConstReader;
  using UnlockedConstReaderAndAtomicRW =
      ThreadRegistrationUnlockedConstReaderAndAtomicRW;
  using UnlockedRWForLockedProfiler =
      ThreadRegistrationUnlockedRWForLockedProfiler;
  using UnlockedReaderAndAtomicRWOnThread =
      ThreadRegistrationUnlockedReaderAndAtomicRWOnThread;
  using LockedRWFromAnyThread = ThreadRegistrationLockedRWFromAnyThread;
  using LockedRWOnThread = ThreadRegistrationLockedRWOnThread;

  // Off-thread access through the registry, providing the following data
  // accessors: UnlockedConstReader, UnlockedConstReaderAndAtomicRW,
  // UnlockedRWForLockedProfiler, and LockedRWFromAnyThread.
  // (See ThreadRegistration class for ON-thread access.)

  // Reference-like class pointing at a ThreadRegistration.
  // It should only exist while sRegistryMutex is locked.
  class OffThreadRef {
   public:
    // const UnlockedConstReader

    [[nodiscard]] const UnlockedConstReader& UnlockedConstReaderCRef() const {
      return mThreadRegistration->mData;
    }

    template <typename F>
    auto WithUnlockedConstReader(F&& aF) const {
      return std::forward<F>(aF)(UnlockedConstReaderCRef());
    }

    // const UnlockedConstReaderAndAtomicRW

    [[nodiscard]] const UnlockedConstReaderAndAtomicRW&
    UnlockedConstReaderAndAtomicRWCRef() const {
      return mThreadRegistration->mData;
    }

    template <typename F>
    auto WithUnlockedConstReaderAndAtomicRW(F&& aF) const {
      return std::forward<F>(aF)(UnlockedConstReaderAndAtomicRWCRef());
    }

    // UnlockedConstReaderAndAtomicRW

    [[nodiscard]] UnlockedConstReaderAndAtomicRW&
    UnlockedConstReaderAndAtomicRWRef() {
      return mThreadRegistration->mData;
    }

    template <typename F>
    auto WithUnlockedConstReaderAndAtomicRW(F&& aF) {
      return std::forward<F>(aF)(UnlockedConstReaderAndAtomicRWRef());
    }

    // const UnlockedRWForLockedProfiler

    [[nodiscard]] const UnlockedRWForLockedProfiler&
    UnlockedRWForLockedProfilerCRef() const {
      return mThreadRegistration->mData;
    }

    template <typename F>
    auto WithUnlockedRWForLockedProfiler(F&& aF) const {
      return std::forward<F>(aF)(UnlockedRWForLockedProfilerCRef());
    }

    // UnlockedRWForLockedProfiler

    [[nodiscard]] UnlockedRWForLockedProfiler&
    UnlockedRWForLockedProfilerRef() {
      return mThreadRegistration->mData;
    }

    template <typename F>
    auto WithUnlockedRWForLockedProfiler(F&& aF) {
      return std::forward<F>(aF)(UnlockedRWForLockedProfilerRef());
    }

    // const LockedRWFromAnyThread through ConstRWFromAnyThreadWithLock

    class ConstRWFromAnyThreadWithLock {
     public:
      [[nodiscard]] const LockedRWFromAnyThread& DataCRef() const {
        return mLockedRWFromAnyThread;
      }
      [[nodiscard]] const LockedRWFromAnyThread* operator->() const {
        return &mLockedRWFromAnyThread;
      }

      ConstRWFromAnyThreadWithLock(
          const LockedRWFromAnyThread& aLockedRWFromAnyThread,
          ThreadRegistration::DataMutex& aDataMutex)
          : mLockedRWFromAnyThread(aLockedRWFromAnyThread),
            mDataLock(aDataMutex) {}

     private:
      const LockedRWFromAnyThread& mLockedRWFromAnyThread;
      ThreadRegistration::DataLock mDataLock;
    };

    [[nodiscard]] ConstRWFromAnyThreadWithLock ConstLockedRWFromAnyThread()
        const {
      return ConstRWFromAnyThreadWithLock{mThreadRegistration->mData,
                                          mThreadRegistration->mDataMutex};
    }

    template <typename F>
    auto WithConstLockedRWFromAnyThread(F&& aF) const {
      ConstRWFromAnyThreadWithLock lockedData = ConstLockedRWFromAnyThread();
      return std::forward<F>(aF)(lockedData.DataCRef());
    }

    // LockedRWFromAnyThread through RWFromAnyThreadWithLock

    class RWFromAnyThreadWithLock {
     public:
      [[nodiscard]] const LockedRWFromAnyThread& DataCRef() const {
        return mLockedRWFromAnyThread;
      }
      [[nodiscard]] LockedRWFromAnyThread& DataRef() {
        return mLockedRWFromAnyThread;
      }
      [[nodiscard]] const LockedRWFromAnyThread* operator->() const {
        return &mLockedRWFromAnyThread;
      }
      [[nodiscard]] LockedRWFromAnyThread* operator->() {
        return &mLockedRWFromAnyThread;
      }

      // In some situations, it may be useful to do some on-thread operations if
      // we are indeed on this thread now. The lock is still held here; caller
      // should not use this pointer longer than this RWFromAnyThreadWithLock.
      [[nodiscard]] LockedRWOnThread* GetLockedRWOnThread() {
        if (mLockedRWFromAnyThread.Info().ThreadId() ==
            profiler_current_thread_id()) {
          // mLockedRWFromAnyThread references a subclass of the
          // ThreadRegistration's mData, so it's safe to downcast it to another
          // hierarchy level of the object.
          return &static_cast<LockedRWOnThread&>(mLockedRWFromAnyThread);
        }
        return nullptr;
      }

     private:
      friend class OffThreadRef;
      RWFromAnyThreadWithLock(LockedRWFromAnyThread& aLockedRWFromAnyThread,
                              ThreadRegistration::DataMutex& aDataMutex)
          : mLockedRWFromAnyThread(aLockedRWFromAnyThread),
            mDataLock(aDataMutex) {}

      LockedRWFromAnyThread& mLockedRWFromAnyThread;
      ThreadRegistration::DataLock mDataLock;
    };

    [[nodiscard]] RWFromAnyThreadWithLock LockedRWFromAnyThread() {
      return RWFromAnyThreadWithLock{mThreadRegistration->mData,
                                     mThreadRegistration->mDataMutex};
    }

    template <typename F>
    auto WithLockedRWFromAnyThread(F&& aF) {
      RWFromAnyThreadWithLock lockedData = LockedRWFromAnyThread();
      return std::forward<F>(aF)(lockedData.DataRef());
    }

   private:
    // Only ThreadRegistry should construct an OnThreadRef.
    friend class ThreadRegistry;
    explicit OffThreadRef(ThreadRegistration& aThreadRegistration)
        : mThreadRegistration(&aThreadRegistration) {}

    // If we have an ON-thread ref, it's safe to convert to an OFF-thread ref.
    explicit OffThreadRef(ThreadRegistration::OnThreadRef aOnThreadRef)
        : mThreadRegistration(aOnThreadRef.mThreadRegistration) {}

    [[nodiscard]] bool IsPointingAt(
        ThreadRegistration& aThreadRegistration) const {
      return mThreadRegistration == &aThreadRegistration;
    }

    // Guaranted to be non-null by construction.
    ThreadRegistration* mThreadRegistration;
  };

  // Lock the registry and allow iteration. E.g.:
  // `for (OffThreadRef thread : LockedRegistry{}) { ... }`
  // Do *not* export copies/references, as they could become dangling.
  // Locking order: Profiler, ThreadRegistry, ThreadRegistration.
  class LockedRegistry {
   public:
    LockedRegistry()
        : mRegistryLock([]() -> RegistryMutex& {
            // In DEBUG builds, *before* we attempt to lock sRegistryMutex, we
            // want to check that the ThreadRegistration mutex is *not* locked
            // on this thread, to avoid inversion deadlocks.
            MOZ_ASSERT(!ThreadRegistration::IsDataMutexLockedOnCurrentThread());
            return sRegistryMutex;
          }()) {}

    [[nodiscard]] const OffThreadRef* begin() const {
      return sRegistryContainer.begin();
    }
    [[nodiscard]] OffThreadRef* begin() { return sRegistryContainer.begin(); }
    [[nodiscard]] const OffThreadRef* end() const {
      return sRegistryContainer.end();
    }
    [[nodiscard]] OffThreadRef* end() { return sRegistryContainer.end(); }

   private:
    RegistryLock mRegistryLock;
  };

  // Call `F(OffThreadRef)` for the given aThreadId.
  template <typename F>
  static void WithOffThreadRef(ProfilerThreadId aThreadId, F&& aF) {
    for (OffThreadRef thread : LockedRegistry{}) {
      if (thread.UnlockedConstReaderCRef().Info().ThreadId() == aThreadId) {
        std::forward<F>(aF)(thread);
        break;
      }
    }
  }

  [[nodiscard]] static bool IsRegistryMutexLockedOnCurrentThread() {
    return sRegistryMutex.IsLockedOnCurrentThread();
  }

 private:
  using RegistryContainer = Vector<OffThreadRef>;

  static RegistryContainer sRegistryContainer;

  // Mutex protecting the registry.
  // Locking order: Profiler, ThreadRegistry, ThreadRegistration.
  static RegistryMutex sRegistryMutex;

  // Only allow ThreadRegistration to (un)register itself.
  friend class ThreadRegistration;
  static void Register(ThreadRegistration::OnThreadRef aOnThreadRef);
  static void Unregister(ThreadRegistration::OnThreadRef aOnThreadRef);
};

}  // namespace mozilla::profiler

#endif  // ProfilerThreadRegistry_h
