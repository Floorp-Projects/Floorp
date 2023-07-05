/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ProfilerThreadRegistration_h
#define ProfilerThreadRegistration_h

#include "mozilla/BaseProfilerDetail.h"
#include "mozilla/ProfilerThreadRegistrationData.h"
#include "mozilla/ThreadLocal.h"

namespace mozilla::profiler {

class ThreadRegistry;

// To use as RAII object, or through RegisterThread/UnregisterThread.
// Automatically registers itself with TLS and Profiler.
// It can be safely nested, but nested instances are just ignored.
// See Get.../With... functions for how to access the data.
class ThreadRegistration {
 private:
  using DataMutex = baseprofiler::detail::BaseProfilerMutex;
  using DataLock = baseprofiler::detail::BaseProfilerAutoLock;

 public:
  // Constructor to use as RAII auto-registration object.
  // It stores itself in the TLS (its effective owner), and gives its pointer to
  // the Profiler.
  ThreadRegistration(const char* aName, const void* aStackTop);

  // Destruction reverses construction: Remove pointer from the Profiler (except
  // for the main thread, because it should be done by the profiler itself) and
  // from the TLS.
  ~ThreadRegistration();

  // Manual construction&destruction, if RAII is not possible or too expensive
  // in stack space.
  // RegisterThread() *must* be paired with exactly one UnregisterThread() on
  // the same thread. (Extra UnregisterThread() calls are handled safely, but
  // they may cause profiling of this thread to stop earlier than expected.)
  static ProfilingStack* RegisterThread(const char* aName,
                                        const void* aStackTop);
  static void UnregisterThread();

  [[nodiscard]] static bool IsRegistered() { return GetFromTLS(); }

  // Prevent copies&moves.
  ThreadRegistration(const ThreadRegistration&) = delete;
  ThreadRegistration& operator=(const ThreadRegistration&) = delete;

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

  // On-thread access from the TLS, providing the following data accessors:
  // UnlockedConstReader, UnlockedConstReaderAndAtomicRW,
  // UnlockedRWForLockedProfiler, UnlockedReaderAndAtomicRWOnThread, and
  // LockedRWOnThread.
  // (See ThreadRegistry class for OFF-thread access.)

  // Reference-like class pointing at the ThreadRegistration for the current
  // thread.
  class OnThreadRef {
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

    // const UnlockedReaderAndAtomicRWOnThread

    [[nodiscard]] const UnlockedReaderAndAtomicRWOnThread&
    UnlockedReaderAndAtomicRWOnThreadCRef() const {
      return mThreadRegistration->mData;
    }

    template <typename F>
    auto WithUnlockedReaderAndAtomicRWOnThread(F&& aF) const {
      return std::forward<F>(aF)(UnlockedReaderAndAtomicRWOnThreadCRef());
    }

    // UnlockedReaderAndAtomicRWOnThread

    [[nodiscard]] UnlockedReaderAndAtomicRWOnThread&
    UnlockedReaderAndAtomicRWOnThreadRef() {
      return mThreadRegistration->mData;
    }

    template <typename F>
    auto WithUnlockedReaderAndAtomicRWOnThread(F&& aF) {
      return std::forward<F>(aF)(UnlockedReaderAndAtomicRWOnThreadRef());
    }

    // const LockedRWOnThread through ConstRWOnThreadWithLock

    // Locking order: Profiler, ThreadRegistry, ThreadRegistration.
    class ConstRWOnThreadWithLock {
     public:
      [[nodiscard]] const LockedRWOnThread& DataCRef() const {
        return mLockedRWOnThread;
      }
      [[nodiscard]] const LockedRWOnThread* operator->() const {
        return &mLockedRWOnThread;
      }

     private:
      friend class OnThreadRef;
      ConstRWOnThreadWithLock(const LockedRWOnThread& aLockedRWOnThread,
                              DataMutex& aDataMutex)
          : mLockedRWOnThread(aLockedRWOnThread), mDataLock(aDataMutex) {}

      const LockedRWOnThread& mLockedRWOnThread;
      DataLock mDataLock;
    };

    [[nodiscard]] ConstRWOnThreadWithLock ConstLockedRWOnThread() const {
      return ConstRWOnThreadWithLock{mThreadRegistration->mData,
                                     mThreadRegistration->mDataMutex};
    }

    template <typename F>
    auto WithConstLockedRWOnThread(F&& aF) const {
      ConstRWOnThreadWithLock lockedData = ConstLockedRWOnThread();
      return std::forward<F>(aF)(lockedData.DataCRef());
    }

    // LockedRWOnThread through RWOnThreadWithLock

    // Locking order: Profiler, ThreadRegistry, ThreadRegistration.
    class RWOnThreadWithLock {
     public:
      [[nodiscard]] const LockedRWOnThread& DataCRef() const {
        return mLockedRWOnThread;
      }
      [[nodiscard]] LockedRWOnThread& DataRef() { return mLockedRWOnThread; }
      [[nodiscard]] const LockedRWOnThread* operator->() const {
        return &mLockedRWOnThread;
      }
      [[nodiscard]] LockedRWOnThread* operator->() {
        return &mLockedRWOnThread;
      }

     private:
      friend class OnThreadRef;
      RWOnThreadWithLock(LockedRWOnThread& aLockedRWOnThread,
                         DataMutex& aDataMutex)
          : mLockedRWOnThread(aLockedRWOnThread), mDataLock(aDataMutex) {}

      LockedRWOnThread& mLockedRWOnThread;
      DataLock mDataLock;
    };

    [[nodiscard]] RWOnThreadWithLock GetLockedRWOnThread() {
      return RWOnThreadWithLock{mThreadRegistration->mData,
                                mThreadRegistration->mDataMutex};
    }

    template <typename F>
    auto WithLockedRWOnThread(F&& aF) {
      RWOnThreadWithLock lockedData = GetLockedRWOnThread();
      return std::forward<F>(aF)(lockedData.DataRef());
    }

    // This is needed to allow OnThreadPtr::operator-> to return a temporary
    // OnThreadRef object, for which `->` must work; Here it provides a pointer
    // to itself, so that the next follow-up `->` will work as member accessor.
    OnThreadRef* operator->() && { return this; }

   private:
    // Only ThreadRegistration should construct an OnThreadRef.
    friend class ThreadRegistration;
    explicit OnThreadRef(ThreadRegistration& aThreadRegistration)
        : mThreadRegistration(&aThreadRegistration) {}

    // Allow ThreadRegistry to read mThreadRegistration.
    friend class ThreadRegistry;

    // Guaranted to be non-null by construction from a reference.
    ThreadRegistration* mThreadRegistration;
  };

  // Pointer-like class pointing at the ThreadRegistration for the current
  // thread, if one was registered.
  class OnThreadPtr {
   public:
    [[nodiscard]] explicit operator bool() const { return mThreadRegistration; }

    // Note that this resolves to a temporary OnThreadRef object, which has all
    // the allowed data accessors.
    [[nodiscard]] OnThreadRef operator*() const {
      MOZ_ASSERT(mThreadRegistration);
      return OnThreadRef(*mThreadRegistration);
    }

    // Note that this resolves to a temporary OnThreadRef object, which also
    // overloads operator-> and has all the allowed data accessors.
    [[nodiscard]] OnThreadRef operator->() const {
      MOZ_ASSERT(mThreadRegistration);
      return OnThreadRef(*mThreadRegistration);
    }

   private:
    friend class ThreadRegistration;
    explicit OnThreadPtr(ThreadRegistration* aThreadRegistration)
        : mThreadRegistration(aThreadRegistration) {}

    ThreadRegistration* mThreadRegistration;
  };

  [[nodiscard]] static OnThreadPtr GetOnThreadPtr() {
    return OnThreadPtr{GetFromTLS()};
  }

  // Call `F(OnThreadRef)`.
  template <typename F>
  static void WithOnThreadRef(F&& aF) {
    const auto* tls = GetTLS();
    if (tls) {
      ThreadRegistration* tr = tls->get();
      if (tr) {
        std::forward<F>(aF)(OnThreadRef{*tr});
      }
    }
  }

  // Call `F(OnThreadRef)`.
  template <typename F, typename FallbackReturn>
  [[nodiscard]] static auto WithOnThreadRefOr(F&& aF,
                                              FallbackReturn&& aFallbackReturn)
      -> decltype(std::forward<F>(aF)(std::declval<OnThreadRef>())) {
    const auto* tls = GetTLS();
    if (tls) {
      ThreadRegistration* tr = tls->get();
      if (tr) {
        return std::forward<F>(aF)(OnThreadRef{*tr});
      }
    }
    return std::forward<FallbackReturn>(aFallbackReturn);
  }

  [[nodiscard]] static bool IsDataMutexLockedOnCurrentThread() {
    if (const ThreadRegistration* tr = GetFromTLS(); tr) {
      return tr->mDataMutex.IsLockedOnCurrentThread();
    }
    return false;
  }

  size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const {
    DataLock lock(mDataMutex);
    return mData.SizeOfExcludingThis(aMallocSizeOf);
  }

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const {
    // aMallocSizeOf can only be used on head-allocated objects. Stack
    // allocations and static objects are not counted.
    return (mIsOnHeap ? aMallocSizeOf(this) : 0) +
           SizeOfExcludingThis(aMallocSizeOf);
  }

 private:
  friend class ThreadRegistry;

  // This is what is embedded inside ThreadRegistration.
  // References to sub-classes will be provided, to limit access as appropriate.
  class EmbeddedData final : public LockedRWOnThread {
   private:
    // Only ThreadRegistration can construct (its embedded) `mData`.
    friend class ThreadRegistration;
    EmbeddedData(const char* aName, const void* aStackTop)
        : LockedRWOnThread(aName, aStackTop) {}
  };
  EmbeddedData mData;

  // Used when writing on self thread, and for any access from any thread.
  // Locking order: Profiler, ThreadRegistry, ThreadRegistration.
  mutable DataMutex mDataMutex;

  // In case of nested (non-RAII) registrations. Only accessed on thread.
  int mOtherRegistrations = 0;

  // Set to true if allocated by `RegisterThread()`. Otherwise we assume that it
  // is on the stack.
  bool mIsOnHeap = false;

  // Only accessed by ThreadRegistry on this thread.
  bool mIsRegistryLockedSharedOnThisThread = false;

  static MOZ_THREAD_LOCAL(ThreadRegistration*) tlsThreadRegistration;

  [[nodiscard]] static decltype(tlsThreadRegistration)* GetTLS() {
    if (tlsThreadRegistration.init())
      return &tlsThreadRegistration;
    else
      return nullptr;
  }

  [[nodiscard]] static ThreadRegistration* GetFromTLS() {
    const auto tls = GetTLS();
    return tls ? tls->get() : nullptr;
  }
};

}  // namespace mozilla::profiler

#endif  // ProfilerThreadRegistration_h
