/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// There are three kinds of samples done by the profiler.
//
// - A "periodic" sample is the most complex kind. It is done in response to a
//   timer while the profiler is active. It involves writing a stack trace plus
//   a variety of other values (memory measurements, responsiveness
//   measurements, markers, etc.) into the main ProfileBuffer. The sampling is
//   done from off-thread, and so SuspendAndSampleAndResumeThread() is used to
//   get the register values.
//
// - A "synchronous" sample is a simpler kind. It is done in response to an API
//   call (profiler_get_backtrace()). It involves writing a stack trace and
//   little else into a temporary ProfileBuffer, and wrapping that up in a
//   ProfilerBacktrace that can be subsequently used in a marker. The sampling
//   is done on-thread, and so Registers::SyncPopulate() is used to get the
//   register values.
//
// - A "backtrace" sample is the simplest kind. It is done in response to an
//   API call (profiler_suspend_and_sample_thread()). It involves getting a
//   stack trace via a ProfilerStackCollector; it does not write to a
//   ProfileBuffer. The sampling is done from off-thread, and so uses
//   SuspendAndSampleAndResumeThread() to get the register values.

#include "platform.h"

#include "GeckoProfiler.h"
#include "GeckoProfilerReporter.h"
#include "PageInformation.h"
#include "ProfiledThreadData.h"
#include "ProfilerBacktrace.h"
#include "ProfileBuffer.h"
#include "ProfilerIOInterposeObserver.h"
#include "ProfilerMarkerPayload.h"
#include "ProfilerParent.h"
#include "RegisteredThread.h"
#include "shared-libraries.h"
#include "ThreadInfo.h"
#include "VTuneProfiler.h"

#include "js/TraceLoggerAPI.h"
#include "js/ProfilingFrameIterator.h"
#include "memory_hooks.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/Atomics.h"
#include "mozilla/AutoProfilerLabel.h"
#include "mozilla/ExtensionPolicyService.h"
#include "mozilla/extensions/WebExtensionPolicy.h"
#include "mozilla/Printf.h"
#include "mozilla/SchedulerGroup.h"
#include "mozilla/Services.h"
#include "mozilla/StackWalk.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/ThreadLocal.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/Tuple.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Vector.h"
#include "BaseProfiler.h"
#include "nsDirectoryServiceDefs.h"
#include "nsDirectoryServiceUtils.h"
#include "nsIDocShell.h"
#include "nsIHttpProtocolHandler.h"
#include "nsIObserverService.h"
#include "nsIPropertyBag2.h"
#include "nsIXULAppInfo.h"
#include "nsIXULRuntime.h"
#include "nsJSPrincipals.h"
#include "nsMemoryReporterManager.h"
#include "nsProfilerStartParams.h"
#include "nsScriptSecurityManager.h"
#include "nsSystemInfo.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"
#include "prdtoa.h"
#include "prtime.h"

#include <algorithm>
#include <errno.h>
#include <fstream>
#include <ostream>
#include <sstream>
#include <type_traits>

#ifdef MOZ_TASK_TRACER
#  include "GeckoTaskTracer.h"
#endif

#if defined(GP_OS_android)
#  include "GeneratedJNINatives.h"
#  include "GeneratedJNIWrappers.h"
#endif

// Win32 builds always have frame pointers, so FramePointerStackWalk() always
// works.
#if defined(GP_PLAT_x86_windows)
#  define HAVE_NATIVE_UNWIND
#  define USE_FRAME_POINTER_STACK_WALK
#endif

// Win64 builds always omit frame pointers, so we use the slower
// MozStackWalk(), which works in that case.
#if defined(GP_PLAT_amd64_windows)
#  define HAVE_NATIVE_UNWIND
#  define USE_MOZ_STACK_WALK
#endif

// AArch64 Win64 doesn't seem to use frame pointers, so we use the slower
// MozStackWalk().
#if defined(GP_PLAT_arm64_windows)
#  define HAVE_NATIVE_UNWIND
#  define USE_MOZ_STACK_WALK
#endif

// Mac builds only have frame pointers when MOZ_PROFILING is specified, so
// FramePointerStackWalk() only works in that case. We don't use MozStackWalk()
// on Mac.
#if defined(GP_OS_darwin) && defined(MOZ_PROFILING)
#  define HAVE_NATIVE_UNWIND
#  define USE_FRAME_POINTER_STACK_WALK
#endif

// Android builds use the ARM Exception Handling ABI to unwind.
#if defined(GP_PLAT_arm_linux) || defined(GP_PLAT_arm_android)
#  define HAVE_NATIVE_UNWIND
#  define USE_EHABI_STACKWALK
#  include "EHABIStackWalk.h"
#endif

// Linux builds use LUL, which uses DWARF info to unwind stacks.
#if defined(GP_PLAT_amd64_linux) || defined(GP_PLAT_x86_linux) ||     \
    defined(GP_PLAT_amd64_android) || defined(GP_PLAT_x86_android) || \
    defined(GP_PLAT_mips64_linux) || defined(GP_PLAT_arm64_linux) ||  \
    defined(GP_PLAT_arm64_android)
#  define HAVE_NATIVE_UNWIND
#  define USE_LUL_STACKWALK
#  include "lul/LulMain.h"
#  include "lul/platform-linux-lul.h"

// On linux we use LUL for periodic samples and synchronous samples, but we use
// FramePointerStackWalk for backtrace samples when MOZ_PROFILING is enabled.
// (See the comment at the top of the file for a definition of
// periodic/synchronous/backtrace.).
//
// FramePointerStackWalk can produce incomplete stacks when the current entry is
// in a shared library without framepointers, however LUL can take a long time
// to initialize, which is undesirable for consumers of
// profiler_suspend_and_sample_thread like the Background Hang Reporter.
#  if defined(MOZ_PROFILING)
#    define USE_FRAME_POINTER_STACK_WALK
#  endif
#endif

// We can only stackwalk without expensive initialization on platforms which
// support FramePointerStackWalk or MozStackWalk. LUL Stackwalking requires
// initializing LUL, and EHABIStackWalk requires initializing EHABI, both of
// which can be expensive.
#if defined(USE_FRAME_POINTER_STACK_WALK) || defined(USE_MOZ_STACK_WALK)
#  define HAVE_FASTINIT_NATIVE_UNWIND
#endif

#ifdef MOZ_VALGRIND
#  include <valgrind/memcheck.h>
#else
#  define VALGRIND_MAKE_MEM_DEFINED(_addr, _len) ((void)0)
#endif

#if defined(GP_OS_linux) || defined(GP_OS_android)
#  include <ucontext.h>
#endif

using namespace mozilla;
using mozilla::profiler::detail::RacyFeatures;

LazyLogModule gProfilerLog("prof");

#if defined(GP_OS_android)
class GeckoJavaSampler
    : public java::GeckoJavaSampler::Natives<GeckoJavaSampler> {
 private:
  GeckoJavaSampler();

 public:
  static double GetProfilerTime() {
    if (!profiler_is_active()) {
      return 0.0;
    }
    return profiler_time();
  };
};
#endif

// Return all features that are available on this platform.
static uint32_t AvailableFeatures() {
  uint32_t features = 0;

#define ADD_FEATURE(n_, str_, Name_, desc_) \
  ProfilerFeature::Set##Name_(features);

  // Add all the possible features.
  PROFILER_FOR_EACH_FEATURE(ADD_FEATURE)

#undef ADD_FEATURE

  // Now remove features not supported on this platform/configuration.
#if !defined(GP_OS_android)
  ProfilerFeature::ClearJava(features);
#endif
#if !defined(HAVE_NATIVE_UNWIND)
  ProfilerFeature::ClearStackWalk(features);
#endif
#if !defined(MOZ_TASK_TRACER)
  ProfilerFeature::ClearTaskTracer(features);
#endif
#if defined(MOZ_REPLACE_MALLOC) && defined(MOZ_PROFILER_MEMORY)
  if (getenv("XPCOM_MEM_BLOAT_LOG")) {
    // The memory hooks are available, but the bloat log is enabled, which is
    // not compatible with the native allocations tracking. See the comment in
    // enable_native_allocations() (tools/profiler/core/memory_hooks.cpp) for
    // more information.
    ProfilerFeature::ClearNativeAllocations(features);
  }
#else
  // The memory hooks are not available.
  ProfilerFeature::ClearNativeAllocations(features);
#endif
  if (!JS::TraceLoggerSupported()) {
    ProfilerFeature::ClearJSTracer(features);
  }

  return features;
}

// Default features common to all contexts (even if not available).
static uint32_t DefaultFeatures() {
  return ProfilerFeature::Java | ProfilerFeature::JS | ProfilerFeature::Leaf |
         ProfilerFeature::StackWalk | ProfilerFeature::Threads;
}

// Extra default features when MOZ_PROFILER_STARTUP is set (even if not
// available).
static uint32_t StartupExtraDefaultFeatures() {
  // Enable mainthreadio by default for startup profiles as startup is heavy on
  // I/O operations, and main thread I/O is really important to see there.
  return ProfilerFeature::MainThreadIO;
}

// The class is a thin shell around mozglue PlatformMutex. It does not preserve
// behavior in JS record/replay. It provides a mechanism to determine if it is
// locked or not in order for memory hooks to avoid re-entering the profiler
// locked state.
class PSMutex : private ::mozilla::detail::MutexImpl {
 public:
  PSMutex() : ::mozilla::detail::MutexImpl() {}

  void Lock() {
    const int tid = profiler_current_thread_id();
    MOZ_ASSERT(tid != 0);

    // This is only designed to catch recursive locking:
    // - If the current thread doesn't own the mutex, `mOwningThreadId` must be
    //   zero or a different thread id written by another thread; it may change
    //   again at any time, but never to the current thread's id.
    // - If the current thread owns the mutex, `mOwningThreadId` must be its id.
    MOZ_ASSERT(mOwningThreadId != tid);

    ::mozilla::detail::MutexImpl::lock();

    // We now hold the mutex, it should have been in the unlocked state before.
    MOZ_ASSERT(mOwningThreadId == 0);
    // And we can write our own thread id.
    mOwningThreadId = tid;
  }

  void Unlock() {
    // This should never trigger! But check just in case something has gone
    // very wrong (e.g., memory corruption).
    AssertCurrentThreadOwns();

    // We're still holding the mutex here, so it's safe to just reset
    // `mOwningThreadId`.
    mOwningThreadId = 0;

    ::mozilla::detail::MutexImpl::unlock();
  }

  // Does the current thread own this mutex?
  // False positive or false negatives are not possible:
  // - If `true`, the current thread owns the mutex, it has written its own
  //   `mOwningThreadId` when taking the lock, and no-one else can modify it
  //   until the current thread itself unlocks the mutex.
  // - If `false`, the current thread does not own the mutex, therefore either
  //   `mOwningThreadId` is zero (unlocked), or it is a different thread id
  //   written by another thread, but it can never be the current thread's id
  //   until the current thread itself locks the mutex.
  bool IsLockedOnCurrentThread() const {
    return mOwningThreadId == profiler_current_thread_id();
  }

  void AssertCurrentThreadOwns() const {
    MOZ_ASSERT(IsLockedOnCurrentThread());
  }

  void AssertCurrentThreadDoesNotOwn() const {
    MOZ_ASSERT(!IsLockedOnCurrentThread());
  }

 private:
  // Zero when unlocked, or the thread id of the owning thread.
  // This should only be used to compare with the current thread id; any other
  // number (0 or other id) could change at any time because the current thread
  // wouldn't own the lock.
  Atomic<int, MemoryOrdering::SequentiallyConsistent> mOwningThreadId{0};
};

// RAII class to lock the profiler mutex.
class MOZ_RAII PSAutoLock {
 public:
  explicit PSAutoLock(PSMutex& aMutex) : mMutex(aMutex) { mMutex.Lock(); }
  ~PSAutoLock() { mMutex.Unlock(); }

 private:
  PSMutex& mMutex;
};

// Only functions that take a PSLockRef arg can access CorePS's and ActivePS's
// fields.
typedef const PSAutoLock& PSLockRef;

#define PS_GET(type_, name_)      \
  static type_ name_(PSLockRef) { \
    MOZ_ASSERT(sInstance);        \
    return sInstance->m##name_;   \
  }

#define PS_GET_LOCKLESS(type_, name_) \
  static type_ name_() {              \
    MOZ_ASSERT(sInstance);            \
    return sInstance->m##name_;       \
  }

#define PS_GET_AND_SET(type_, name_)                  \
  PS_GET(type_, name_)                                \
  static void Set##name_(PSLockRef, type_ a##name_) { \
    MOZ_ASSERT(sInstance);                            \
    sInstance->m##name_ = a##name_;                   \
  }

static const size_t MAX_JS_FRAMES = 1024;
using JsFrameBuffer = JS::ProfilingFrameIterator::Frame[MAX_JS_FRAMES];

// All functions in this file can run on multiple threads unless they have an
// NS_IsMainThread() assertion.

// This class contains the profiler's core global state, i.e. that which is
// valid even when the profiler is not active. Most profile operations can't do
// anything useful when this class is not instantiated, so we release-assert
// its non-nullness in all such operations.
//
// Accesses to CorePS are guarded by gPSMutex. Getters and setters take a
// PSAutoLock reference as an argument as proof that the gPSMutex is currently
// locked. This makes it clear when gPSMutex is locked and helps avoid
// accidental unlocked accesses to global state. There are ways to circumvent
// this mechanism, but please don't do so without *very* good reason and a
// detailed explanation.
//
// The exceptions to this rule:
//
// - mProcessStartTime, because it's immutable;
//
// - each thread's RacyRegisteredThread object is accessible without locking via
//   TLSRegisteredThread::RacyRegisteredThread().
class CorePS {
 private:
  CorePS()
      : mProcessStartTime(TimeStamp::ProcessCreation()),
        // This needs its own mutex, because it is used concurrently from
        // functions guarded by gPSMutex as well as others without safety (e.g.,
        // profiler_add_marker). It is *not* used inside the critical section of
        // the sampler, because mutexes cannot be used there.
        mCoreBlocksRingBuffer(BlocksRingBuffer::ThreadSafety::WithMutex)
#ifdef USE_LUL_STACKWALK
        ,
        mLul(nullptr)
#endif
  {
  }

  ~CorePS() {}

 public:
  static void Create(PSLockRef aLock) {
    MOZ_ASSERT(!sInstance);
    sInstance = new CorePS();
  }

  static void Destroy(PSLockRef aLock) {
    MOZ_ASSERT(sInstance);
    delete sInstance;
    sInstance = nullptr;
  }

  // Unlike ActivePS::Exists(), CorePS::Exists() can be called without gPSMutex
  // being locked. This is because CorePS is instantiated so early on the main
  // thread that we don't have to worry about it being racy.
  static bool Exists() { return !!sInstance; }

  static void AddSizeOf(PSLockRef, MallocSizeOf aMallocSizeOf,
                        size_t& aProfSize, size_t& aLulSize) {
    MOZ_ASSERT(sInstance);

    aProfSize += aMallocSizeOf(sInstance);

    for (auto& registeredThread : sInstance->mRegisteredThreads) {
      aProfSize += registeredThread->SizeOfIncludingThis(aMallocSizeOf);
    }

    for (auto& registeredPage : sInstance->mRegisteredPages) {
      aProfSize += registeredPage->SizeOfIncludingThis(aMallocSizeOf);
    }

    // Measurement of the following things may be added later if DMD finds it
    // is worthwhile:
    // - CorePS::mRegisteredThreads itself (its elements' children are
    // measured above)
    // - CorePS::mRegisteredPages itself (its elements' children are
    // measured above)
    // - CorePS::mInterposeObserver

#if defined(USE_LUL_STACKWALK)
    if (sInstance->mLul) {
      aLulSize += sInstance->mLul->SizeOfIncludingThis(aMallocSizeOf);
    }
#endif
  }

  // No PSLockRef is needed for this field because it's immutable.
  PS_GET_LOCKLESS(TimeStamp, ProcessStartTime)

  // No PSLockRef is needed for this field because it's thread-safe.
  PS_GET_LOCKLESS(BlocksRingBuffer&, CoreBlocksRingBuffer)

  PS_GET(const Vector<UniquePtr<RegisteredThread>>&, RegisteredThreads)

  PS_GET(JsFrameBuffer&, JsFrames)

  static void AppendRegisteredThread(
      PSLockRef, UniquePtr<RegisteredThread>&& aRegisteredThread) {
    MOZ_ASSERT(sInstance);
    MOZ_RELEASE_ASSERT(
        sInstance->mRegisteredThreads.append(std::move(aRegisteredThread)));
  }

  static void RemoveRegisteredThread(PSLockRef,
                                     RegisteredThread* aRegisteredThread) {
    MOZ_ASSERT(sInstance);
    // Remove aRegisteredThread from mRegisteredThreads.
    for (UniquePtr<RegisteredThread>& rt : sInstance->mRegisteredThreads) {
      if (rt.get() == aRegisteredThread) {
        sInstance->mRegisteredThreads.erase(&rt);
        return;
      }
    }
  }

  PS_GET(Vector<RefPtr<PageInformation>>&, RegisteredPages)

  static void AppendRegisteredPage(PSLockRef,
                                   RefPtr<PageInformation>&& aRegisteredPage) {
    MOZ_ASSERT(sInstance);
    struct RegisteredPageComparator {
      PageInformation* aA;
      bool operator()(PageInformation* aB) const { return aA->Equals(aB); }
    };

    auto foundPageIter = std::find_if(
        sInstance->mRegisteredPages.begin(), sInstance->mRegisteredPages.end(),
        RegisteredPageComparator{aRegisteredPage.get()});

    if (foundPageIter != sInstance->mRegisteredPages.end()) {
      if ((*foundPageIter)->Url().EqualsLiteral("about:blank")) {
        // When a BrowsingContext is loaded, the first url loaded in it will be
        // about:blank, and if the principal matches, the first document loaded
        // in it will share an inner window. That's why we should delete the
        // intermittent about:blank if they share the inner window.
        sInstance->mRegisteredPages.erase(foundPageIter);
      } else {
        // Do not register the same page again.
        return;
      }
    }

    MOZ_RELEASE_ASSERT(
        sInstance->mRegisteredPages.append(std::move(aRegisteredPage)));
  }

  static void RemoveRegisteredPage(PSLockRef,
                                   uint64_t aRegisteredInnerWindowID) {
    MOZ_ASSERT(sInstance);
    // Remove RegisteredPage from mRegisteredPages by given inner window ID.
    sInstance->mRegisteredPages.eraseIf([&](const RefPtr<PageInformation>& rd) {
      return rd->InnerWindowID() == aRegisteredInnerWindowID;
    });
  }

  static void ClearRegisteredPages(PSLockRef) {
    MOZ_ASSERT(sInstance);
    sInstance->mRegisteredPages.clear();
  }

  PS_GET(const Vector<BaseProfilerCount*>&, Counters)

  static void AppendCounter(PSLockRef, BaseProfilerCount* aCounter) {
    MOZ_ASSERT(sInstance);
    // we don't own the counter; they may be stored in static objects
    MOZ_RELEASE_ASSERT(sInstance->mCounters.append(aCounter));
  }

  static void RemoveCounter(PSLockRef, BaseProfilerCount* aCounter) {
    // we may be called to remove a counter after the profiler is stopped or
    // late in shutdown.
    if (sInstance) {
      auto* counter = std::find(sInstance->mCounters.begin(),
                                sInstance->mCounters.end(), aCounter);
      MOZ_RELEASE_ASSERT(counter != sInstance->mCounters.end());
      sInstance->mCounters.erase(counter);
    }
  }

#ifdef USE_LUL_STACKWALK
  static lul::LUL* Lul(PSLockRef) {
    MOZ_ASSERT(sInstance);
    return sInstance->mLul.get();
  }
  static void SetLul(PSLockRef, UniquePtr<lul::LUL> aLul) {
    MOZ_ASSERT(sInstance);
    sInstance->mLul = std::move(aLul);
  }
#endif

  PS_GET_AND_SET(const nsACString&, ProcessName)

 private:
  // The singleton instance
  static CorePS* sInstance;

  // The time that the process started.
  const TimeStamp mProcessStartTime;

  // The thread-safe blocks-oriented ring buffer into which all profiling data
  // is recorded.
  // ActivePS controls the lifetime of the underlying contents buffer: When
  // ActivePS does not exist, mCoreBlocksRingBuffer is empty and rejects all
  // reads&writes; see ActivePS for further details.
  // Note: This needs to live here outside of ActivePS, because some producers
  // are indirectly controlled (e.g., by atomic flags) and therefore may still
  // attempt to write some data shortly after ActivePS has shutdown and deleted
  // the underlying buffer in memory.
  BlocksRingBuffer mCoreBlocksRingBuffer;

  // Info on all the registered threads.
  // ThreadIds in mRegisteredThreads are unique.
  Vector<UniquePtr<RegisteredThread>> mRegisteredThreads;

  // Info on all the registered pages.
  // InnerWindowIDs in mRegisteredPages are unique.
  Vector<RefPtr<PageInformation>> mRegisteredPages;

  // Non-owning pointers to all active counters
  Vector<BaseProfilerCount*> mCounters;

#ifdef USE_LUL_STACKWALK
  // LUL's state. Null prior to the first activation, non-null thereafter.
  UniquePtr<lul::LUL> mLul;
#endif

  // Process name, provided by child process initialization code.
  nsAutoCString mProcessName;

  // This memory buffer is used by the MergeStacks mechanism. Previously it was
  // stack allocated, but this led to a stack overflow, as it was too much
  // memory. Here the buffer can be pre-allocated, and shared with the
  // MergeStacks feature as needed. MergeStacks is only run while holding the
  // lock, so it is safe to have only one instance allocated for all of the
  // threads.
  JsFrameBuffer mJsFrames;
};

CorePS* CorePS::sInstance = nullptr;

class SamplerThread;

static SamplerThread* NewSamplerThread(PSLockRef aLock, uint32_t aGeneration,
                                       double aInterval);

struct LiveProfiledThreadData {
  RegisteredThread* mRegisteredThread;
  UniquePtr<ProfiledThreadData> mProfiledThreadData;
};

// This class contains the profiler's global state that is valid only when the
// profiler is active. When not instantiated, the profiler is inactive.
//
// Accesses to ActivePS are guarded by gPSMutex, in much the same fashion as
// CorePS.
//
class ActivePS {
 private:
  static uint32_t AdjustFeatures(uint32_t aFeatures, uint32_t aFilterCount) {
    // Filter out any features unavailable in this platform/configuration.
    aFeatures &= AvailableFeatures();

    // Always enable ProfilerFeature::Threads if we have a filter, because
    // users sometimes ask to filter by a list of threads but forget to
    // explicitly specify ProfilerFeature::Threads.
    if (aFilterCount > 0) {
      aFeatures |= ProfilerFeature::Threads;
    }

    return aFeatures;
  }

  ActivePS(PSLockRef aLock, PowerOfTwo32 aCapacity, double aInterval,
           uint32_t aFeatures, const char** aFilters, uint32_t aFilterCount,
           uint64_t aActiveBrowsingContextID, const Maybe<double>& aDuration)
      : mGeneration(sNextGeneration++),
        mCapacity(aCapacity),
        mDuration(aDuration),
        mInterval(aInterval),
        mFeatures(AdjustFeatures(aFeatures, aFilterCount)),
        mActiveBrowsingContextID(aActiveBrowsingContextID),
        // 8 bytes per entry.
        mProfileBuffer(CorePS::CoreBlocksRingBuffer(),
                       PowerOfTwo32(aCapacity.Value() * 8)),
        // The new sampler thread doesn't start sampling immediately because the
        // main loop within Run() is blocked until this function's caller
        // unlocks gPSMutex.
        mSamplerThread(NewSamplerThread(aLock, mGeneration, aInterval)),
        mInterposeObserver(ProfilerFeature::HasMainThreadIO(aFeatures)
                               ? new ProfilerIOInterposeObserver()
                               : nullptr),
        mIsPaused(false)
#if defined(GP_OS_linux)
        ,
        mWasPaused(false)
#endif
  {
    // Deep copy aFilters.
    MOZ_ALWAYS_TRUE(mFilters.resize(aFilterCount));
    for (uint32_t i = 0; i < aFilterCount; ++i) {
      mFilters[i] = aFilters[i];
    }

#if !defined(RELEASE_OR_BETA)
    if (mInterposeObserver) {
      // We need to register the observer on the main thread, because we want
      // to observe IO that happens on the main thread.
      // IOInterposer needs to be initialized before calling
      // IOInterposer::Register or our observer will be silently dropped.
      if (NS_IsMainThread()) {
        IOInterposer::Init();
        IOInterposer::Register(IOInterposeObserver::OpAll, mInterposeObserver);
      } else {
        RefPtr<ProfilerIOInterposeObserver> observer = mInterposeObserver;
        NS_DispatchToMainThread(
            NS_NewRunnableFunction("ActivePS::ActivePS", [=]() {
              IOInterposer::Init();
              IOInterposer::Register(IOInterposeObserver::OpAll, observer);
            }));
      }
    }
#endif
  }

  ~ActivePS() {
#if !defined(RELEASE_OR_BETA)
    if (mInterposeObserver) {
      // We need to unregister the observer on the main thread, because that's
      // where we've registered it.
      if (NS_IsMainThread()) {
        IOInterposer::Unregister(IOInterposeObserver::OpAll,
                                 mInterposeObserver);
      } else {
        RefPtr<ProfilerIOInterposeObserver> observer = mInterposeObserver;
        NS_DispatchToMainThread(
            NS_NewRunnableFunction("ActivePS::~ActivePS", [=]() {
              IOInterposer::Unregister(IOInterposeObserver::OpAll, observer);
            }));
      }
    }
#endif
  }

  bool ThreadSelected(const char* aThreadName) {
    if (mFilters.empty()) {
      return true;
    }

    std::string name = aThreadName;
    std::transform(name.begin(), name.end(), name.begin(), ::tolower);

    for (uint32_t i = 0; i < mFilters.length(); ++i) {
      std::string filter = mFilters[i];

      if (filter == "*") {
        return true;
      }

      std::transform(filter.begin(), filter.end(), filter.begin(), ::tolower);

      // Crude, non UTF-8 compatible, case insensitive substring search
      if (name.find(filter) != std::string::npos) {
        return true;
      }

      // If the filter starts with pid:, check for a pid match
      if (filter.find("pid:") == 0) {
        std::string mypid = std::to_string(profiler_current_process_id());
        if (filter.compare(4, std::string::npos, mypid) == 0) {
          return true;
        }
      }
    }

    return false;
  }

 public:
  static void Create(PSLockRef aLock, PowerOfTwo32 aCapacity, double aInterval,
                     uint32_t aFeatures, const char** aFilters,
                     uint32_t aFilterCount, uint64_t aActiveBrowsingContextID,
                     const Maybe<double>& aDuration) {
    MOZ_ASSERT(!sInstance);
    sInstance = new ActivePS(aLock, aCapacity, aInterval, aFeatures, aFilters,
                             aFilterCount, aActiveBrowsingContextID, aDuration);
  }

  [[nodiscard]] static SamplerThread* Destroy(PSLockRef aLock) {
    MOZ_ASSERT(sInstance);
    auto samplerThread = sInstance->mSamplerThread;
    delete sInstance;
    sInstance = nullptr;

    return samplerThread;
  }

  static bool Exists(PSLockRef) { return !!sInstance; }

  static bool Equals(PSLockRef, PowerOfTwo32 aCapacity,
                     const Maybe<double>& aDuration, double aInterval,
                     uint32_t aFeatures, const char** aFilters,
                     uint32_t aFilterCount, uint64_t aActiveBrowsingContextID) {
    MOZ_ASSERT(sInstance);
    if (sInstance->mCapacity != aCapacity ||
        sInstance->mDuration != aDuration ||
        sInstance->mInterval != aInterval ||
        sInstance->mFeatures != aFeatures ||
        sInstance->mFilters.length() != aFilterCount ||
        sInstance->mActiveBrowsingContextID != aActiveBrowsingContextID) {
      return false;
    }

    for (uint32_t i = 0; i < sInstance->mFilters.length(); ++i) {
      if (strcmp(sInstance->mFilters[i].c_str(), aFilters[i]) != 0) {
        return false;
      }
    }
    return true;
  }

  static size_t SizeOf(PSLockRef, MallocSizeOf aMallocSizeOf) {
    MOZ_ASSERT(sInstance);

    size_t n = aMallocSizeOf(sInstance);

    n += sInstance->mProfileBuffer.SizeOfExcludingThis(aMallocSizeOf);

    // Measurement of the following members may be added later if DMD finds it
    // is worthwhile:
    // - mLiveProfiledThreads (both the array itself, and the contents)
    // - mDeadProfiledThreads (both the array itself, and the contents)
    //

    return n;
  }

  static bool ShouldProfileThread(PSLockRef aLock, ThreadInfo* aInfo) {
    MOZ_ASSERT(sInstance);
    return ((aInfo->IsMainThread() || FeatureThreads(aLock)) &&
            sInstance->ThreadSelected(aInfo->Name()));
  }

  [[nodiscard]] static bool AppendPostSamplingCallback(
      PSLockRef, PostSamplingCallback&& aCallback);

  // Writes out the current active configuration of the profile.
  static void WriteActiveConfiguration(PSLockRef aLock, JSONWriter& aWriter,
                                       const char* aPropertyName = nullptr) {
    if (!sInstance) {
      if (aPropertyName) {
        aWriter.NullProperty(aPropertyName);
      } else {
        aWriter.NullElement();
      }
      return;
    };

    if (aPropertyName) {
      aWriter.StartObjectProperty(aPropertyName);
    } else {
      aWriter.StartObjectElement();
    }

    {
      aWriter.StartArrayProperty("features", aWriter.SingleLineStyle);
#define WRITE_ACTIVE_FEATURES(n_, str_, Name_, desc_)    \
  if (profiler_feature_active(ProfilerFeature::Name_)) { \
    aWriter.StringElement(str_);                         \
  }

      PROFILER_FOR_EACH_FEATURE(WRITE_ACTIVE_FEATURES)
#undef WRITE_ACTIVE_FEATURES
      aWriter.EndArray();
    }
    {
      aWriter.StartArrayProperty("threads", aWriter.SingleLineStyle);
      for (const auto& filter : sInstance->mFilters) {
        aWriter.StringElement(filter.c_str());
      }
      aWriter.EndArray();
    }
    {
      // Now write all the simple values.

      // The interval is also available on profile.meta.interval
      aWriter.DoubleProperty("interval", sInstance->mInterval);
      aWriter.IntProperty("capacity", sInstance->mCapacity.Value());
      if (sInstance->mDuration) {
        aWriter.DoubleProperty("duration", sInstance->mDuration.value());
      }
      // Here, we are converting uint64_t to double. Browsing Context IDs are
      // being created using `nsContentUtils::GenerateProcessSpecificId`, which
      // is specifically designed to only use 53 of the 64 bits to be lossless
      // when passed into and out of JS as a double.
      aWriter.DoubleProperty("activeBrowsingContextID",
                             sInstance->mActiveBrowsingContextID);
    }
    aWriter.EndObject();
  }

  PS_GET(uint32_t, Generation)

  PS_GET(PowerOfTwo32, Capacity)

  PS_GET(Maybe<double>, Duration)

  PS_GET(double, Interval)

  PS_GET(uint32_t, Features)

  PS_GET(uint64_t, ActiveBrowsingContextID)

#define PS_GET_FEATURE(n_, str_, Name_, desc_)                \
  static bool Feature##Name_(PSLockRef) {                     \
    MOZ_ASSERT(sInstance);                                    \
    return ProfilerFeature::Has##Name_(sInstance->mFeatures); \
  }

  PROFILER_FOR_EACH_FEATURE(PS_GET_FEATURE)

#undef PS_GET_FEATURE

  static uint32_t JSFlags(PSLockRef aLock) {
    uint32_t Flags = 0;
    Flags |=
        FeatureJS(aLock) ? uint32_t(JSInstrumentationFlags::StackSampling) : 0;
    Flags |= FeatureTrackOptimizations(aLock)
                 ? uint32_t(JSInstrumentationFlags::TrackOptimizations)
                 : 0;
    Flags |= FeatureJSTracer(aLock)
                 ? uint32_t(JSInstrumentationFlags::TraceLogging)
                 : 0;
    Flags |= FeatureJSAllocations(aLock)
                 ? uint32_t(JSInstrumentationFlags::Allocations)
                 : 0;
    return Flags;
  }

  PS_GET(const Vector<std::string>&, Filters)

  static ProfileBuffer& Buffer(PSLockRef) {
    MOZ_ASSERT(sInstance);
    return sInstance->mProfileBuffer;
  }

  static const Vector<LiveProfiledThreadData>& LiveProfiledThreads(PSLockRef) {
    MOZ_ASSERT(sInstance);
    return sInstance->mLiveProfiledThreads;
  }

  // Returns an array containing (RegisteredThread*, ProfiledThreadData*) pairs
  // for all threads that should be included in a profile, both for threads
  // that are still registered, and for threads that have been unregistered but
  // still have data in the buffer.
  // For threads that have already been unregistered, the RegisteredThread
  // pointer will be null.
  // The returned array is sorted by thread register time.
  // Do not hold on to the return value across thread registration or profiler
  // restarts.
  static Vector<std::pair<RegisteredThread*, ProfiledThreadData*>>
  ProfiledThreads(PSLockRef) {
    MOZ_ASSERT(sInstance);
    Vector<std::pair<RegisteredThread*, ProfiledThreadData*>> array;
    MOZ_RELEASE_ASSERT(
        array.initCapacity(sInstance->mLiveProfiledThreads.length() +
                           sInstance->mDeadProfiledThreads.length()));
    for (auto& t : sInstance->mLiveProfiledThreads) {
      MOZ_RELEASE_ASSERT(array.append(
          std::make_pair(t.mRegisteredThread, t.mProfiledThreadData.get())));
    }
    for (auto& t : sInstance->mDeadProfiledThreads) {
      MOZ_RELEASE_ASSERT(
          array.append(std::make_pair((RegisteredThread*)nullptr, t.get())));
    }

    std::sort(array.begin(), array.end(),
              [](const std::pair<RegisteredThread*, ProfiledThreadData*>& a,
                 const std::pair<RegisteredThread*, ProfiledThreadData*>& b) {
                return a.second->Info()->RegisterTime() <
                       b.second->Info()->RegisterTime();
              });
    return array;
  }

  static Vector<RefPtr<PageInformation>> ProfiledPages(PSLockRef aLock) {
    MOZ_ASSERT(sInstance);
    Vector<RefPtr<PageInformation>> array;
    for (auto& d : CorePS::RegisteredPages(aLock)) {
      MOZ_RELEASE_ASSERT(array.append(d));
    }
    for (auto& d : sInstance->mDeadProfiledPages) {
      MOZ_RELEASE_ASSERT(array.append(d));
    }
    // We don't need to sort the pages like threads since we won't show them
    // as a list.
    return array;
  }

  // Do a linear search through mLiveProfiledThreads to find the
  // ProfiledThreadData object for a RegisteredThread.
  static ProfiledThreadData* GetProfiledThreadData(
      PSLockRef, RegisteredThread* aRegisteredThread) {
    MOZ_ASSERT(sInstance);
    for (const LiveProfiledThreadData& thread :
         sInstance->mLiveProfiledThreads) {
      if (thread.mRegisteredThread == aRegisteredThread) {
        return thread.mProfiledThreadData.get();
      }
    }
    return nullptr;
  }

  static ProfiledThreadData* AddLiveProfiledThread(
      PSLockRef, RegisteredThread* aRegisteredThread,
      UniquePtr<ProfiledThreadData>&& aProfiledThreadData) {
    MOZ_ASSERT(sInstance);
    MOZ_RELEASE_ASSERT(
        sInstance->mLiveProfiledThreads.append(LiveProfiledThreadData{
            aRegisteredThread, std::move(aProfiledThreadData)}));

    // Return a weak pointer to the ProfiledThreadData object.
    return sInstance->mLiveProfiledThreads.back().mProfiledThreadData.get();
  }

  static void UnregisterThread(PSLockRef aLockRef,
                               RegisteredThread* aRegisteredThread) {
    MOZ_ASSERT(sInstance);

    DiscardExpiredDeadProfiledThreads(aLockRef);

    // Find the right entry in the mLiveProfiledThreads array and remove the
    // element, moving the ProfiledThreadData object for the thread into the
    // mDeadProfiledThreads array.
    // The thread's RegisteredThread object gets destroyed here.
    for (size_t i = 0; i < sInstance->mLiveProfiledThreads.length(); i++) {
      LiveProfiledThreadData& thread = sInstance->mLiveProfiledThreads[i];
      if (thread.mRegisteredThread == aRegisteredThread) {
        thread.mProfiledThreadData->NotifyUnregistered(
            sInstance->mProfileBuffer.BufferRangeEnd());
        MOZ_RELEASE_ASSERT(sInstance->mDeadProfiledThreads.append(
            std::move(thread.mProfiledThreadData)));
        sInstance->mLiveProfiledThreads.erase(
            &sInstance->mLiveProfiledThreads[i]);
        return;
      }
    }
  }

  PS_GET_AND_SET(bool, IsPaused)

#if defined(GP_OS_linux)
  PS_GET_AND_SET(bool, WasPaused)
#endif

  static void DiscardExpiredDeadProfiledThreads(PSLockRef) {
    MOZ_ASSERT(sInstance);
    uint64_t bufferRangeStart = sInstance->mProfileBuffer.BufferRangeStart();
    // Discard any dead threads that were unregistered before bufferRangeStart.
    sInstance->mDeadProfiledThreads.eraseIf(
        [bufferRangeStart](
            const UniquePtr<ProfiledThreadData>& aProfiledThreadData) {
          Maybe<uint64_t> bufferPosition =
              aProfiledThreadData->BufferPositionWhenUnregistered();
          MOZ_RELEASE_ASSERT(bufferPosition,
                             "should have unregistered this thread");
          return *bufferPosition < bufferRangeStart;
        });
  }

  static void UnregisterPage(PSLockRef aLock,
                             uint64_t aRegisteredInnerWindowID) {
    MOZ_ASSERT(sInstance);
    auto& registeredPages = CorePS::RegisteredPages(aLock);
    for (size_t i = 0; i < registeredPages.length(); i++) {
      RefPtr<PageInformation>& page = registeredPages[i];
      if (page->InnerWindowID() == aRegisteredInnerWindowID) {
        page->NotifyUnregistered(sInstance->mProfileBuffer.BufferRangeEnd());
        MOZ_RELEASE_ASSERT(
            sInstance->mDeadProfiledPages.append(std::move(page)));
        registeredPages.erase(&registeredPages[i--]);
      }
    }
  }

  static void DiscardExpiredPages(PSLockRef) {
    MOZ_ASSERT(sInstance);
    uint64_t bufferRangeStart = sInstance->mProfileBuffer.BufferRangeStart();
    // Discard any dead pages that were unregistered before
    // bufferRangeStart.
    sInstance->mDeadProfiledPages.eraseIf(
        [bufferRangeStart](const RefPtr<PageInformation>& aProfiledPage) {
          Maybe<uint64_t> bufferPosition =
              aProfiledPage->BufferPositionWhenUnregistered();
          MOZ_RELEASE_ASSERT(bufferPosition,
                             "should have unregistered this page");
          return *bufferPosition < bufferRangeStart;
        });
  }

  static void ClearUnregisteredPages(PSLockRef) {
    MOZ_ASSERT(sInstance);
    sInstance->mDeadProfiledPages.clear();
  }

  static void ClearExpiredExitProfiles(PSLockRef) {
    MOZ_ASSERT(sInstance);
    uint64_t bufferRangeStart = sInstance->mProfileBuffer.BufferRangeStart();
    // Discard exit profiles that were gathered before our buffer RangeStart.
#ifdef MOZ_BASE_PROFILER
    // If we have started to overwrite our data from when the Base profile was
    // added, we should get rid of that Base profile because it's now older than
    // our oldest Gecko profile data.
    //
    // When adding: (In practice the starting buffer should be empty)
    // v Start == End
    // |                 <-- Buffer range, initially empty.
    // ^ mGeckoIndexWhenBaseProfileAdded < Start FALSE -> keep it
    //
    // Later, still in range:
    // v Start   v End
    // |=========|       <-- Buffer range growing.
    // ^ mGeckoIndexWhenBaseProfileAdded < Start FALSE -> keep it
    //
    // Even later, now out of range:
    //       v Start      v End
    //       |============|       <-- Buffer range full and sliding.
    // ^ mGeckoIndexWhenBaseProfileAdded < Start TRUE! -> Discard it
    if (sInstance->mBaseProfileThreads &&
        sInstance->mGeckoIndexWhenBaseProfileAdded <
            CorePS::CoreBlocksRingBuffer().GetState().mRangeStart) {
      DEBUG_LOG("ClearExpiredExitProfiles() - Discarding base profile %p",
                sInstance->mBaseProfileThreads.get());
      sInstance->mBaseProfileThreads.reset();
    }
#endif
    sInstance->mExitProfiles.eraseIf(
        [bufferRangeStart](const ExitProfile& aExitProfile) {
          return aExitProfile.mBufferPositionAtGatherTime < bufferRangeStart;
        });
  }

#ifdef MOZ_BASE_PROFILER
  static void AddBaseProfileThreads(PSLockRef aLock,
                                    UniquePtr<char[]> aBaseProfileThreads) {
    MOZ_ASSERT(sInstance);
    DEBUG_LOG("AddBaseProfileThreads(%p)", aBaseProfileThreads.get());
    sInstance->mBaseProfileThreads = std::move(aBaseProfileThreads);
    sInstance->mGeckoIndexWhenBaseProfileAdded =
        CorePS::CoreBlocksRingBuffer().GetState().mRangeEnd;
  }

  static UniquePtr<char[]> MoveBaseProfileThreads(PSLockRef aLock) {
    MOZ_ASSERT(sInstance);

    ClearExpiredExitProfiles(aLock);

    DEBUG_LOG("MoveBaseProfileThreads() - Consuming base profile %p",
              sInstance->mBaseProfileThreads.get());
    return std::move(sInstance->mBaseProfileThreads);
  }
#endif

  static void AddExitProfile(PSLockRef aLock, const nsCString& aExitProfile) {
    MOZ_ASSERT(sInstance);

    ClearExpiredExitProfiles(aLock);

    MOZ_RELEASE_ASSERT(sInstance->mExitProfiles.append(
        ExitProfile{aExitProfile, sInstance->mProfileBuffer.BufferRangeEnd()}));
  }

  static Vector<nsCString> MoveExitProfiles(PSLockRef aLock) {
    MOZ_ASSERT(sInstance);

    ClearExpiredExitProfiles(aLock);

    Vector<nsCString> profiles;
    MOZ_RELEASE_ASSERT(
        profiles.initCapacity(sInstance->mExitProfiles.length()));
    for (auto& profile : sInstance->mExitProfiles) {
      MOZ_RELEASE_ASSERT(profiles.append(std::move(profile.mJSON)));
    }
    sInstance->mExitProfiles.clear();
    return profiles;
  }

 private:
  // The singleton instance.
  static ActivePS* sInstance;

  // We need to track activity generations. If we didn't we could have the
  // following scenario.
  //
  // - profiler_stop() locks gPSMutex, de-instantiates ActivePS, unlocks
  //   gPSMutex, deletes the SamplerThread (which does a join).
  //
  // - profiler_start() runs on a different thread, locks gPSMutex,
  //   re-instantiates ActivePS, unlocks gPSMutex -- all before the join
  //   completes.
  //
  // - SamplerThread::Run() locks gPSMutex, sees that ActivePS is instantiated,
  //   and continues as if the start/stop pair didn't occur. Also
  //   profiler_stop() is stuck, unable to finish.
  //
  // By checking ActivePS *and* the generation, we can avoid this scenario.
  // sNextGeneration is used to track the next generation number; it is static
  // because it must persist across different ActivePS instantiations.
  const uint32_t mGeneration;
  static uint32_t sNextGeneration;

  // The maximum number of entries in mProfileBuffer.
  const PowerOfTwo32 mCapacity;

  // The maximum duration of entries in mProfileBuffer, in seconds.
  const Maybe<double> mDuration;

  // The interval between samples, measured in milliseconds.
  const double mInterval;

  // The profile features that are enabled.
  const uint32_t mFeatures;

  // Substrings of names of threads we want to profile.
  Vector<std::string> mFilters;

  // Browsing Context ID of the active active browser screen's active tab.
  // It's being used to determine the profiled tab. It's "0" if we failed to
  // get the ID.
  const uint64_t mActiveBrowsingContextID;

  // The buffer into which all samples are recorded.
  ProfileBuffer mProfileBuffer;

  // ProfiledThreadData objects for any threads that were profiled at any point
  // during this run of the profiler:
  //  - mLiveProfiledThreads contains all threads that are still registered, and
  //  - mDeadProfiledThreads contains all threads that have already been
  //    unregistered but for which there is still data in the profile buffer.
  Vector<LiveProfiledThreadData> mLiveProfiledThreads;
  Vector<UniquePtr<ProfiledThreadData>> mDeadProfiledThreads;

  // Info on all the dead pages.
  // Registered pages are being moved to this array after unregistration.
  // We are keeping them in case we need them in the profile data.
  // We are removing them when we ensure that we won't need them anymore.
  Vector<RefPtr<PageInformation>> mDeadProfiledPages;

  // The current sampler thread. This class is not responsible for destroying
  // the SamplerThread object; the Destroy() method returns it so the caller
  // can destroy it.
  SamplerThread* const mSamplerThread;

  // The interposer that records main thread I/O.
  RefPtr<ProfilerIOInterposeObserver> mInterposeObserver;

  // Is the profiler paused?
  bool mIsPaused;

#if defined(GP_OS_linux)
  // Used to record whether the profiler was paused just before forking. False
  // at all times except just before/after forking.
  bool mWasPaused;
#endif

#ifdef MOZ_BASE_PROFILER
  // Optional startup profile thread array from BaseProfiler.
  UniquePtr<char[]> mBaseProfileThreads;
  ProfileBufferBlockIndex mGeckoIndexWhenBaseProfileAdded;
#endif

  struct ExitProfile {
    nsCString mJSON;
    uint64_t mBufferPositionAtGatherTime;
  };
  Vector<ExitProfile> mExitProfiles;
};

ActivePS* ActivePS::sInstance = nullptr;
uint32_t ActivePS::sNextGeneration = 0;

#undef PS_GET
#undef PS_GET_LOCKLESS
#undef PS_GET_AND_SET

// The mutex that guards accesses to CorePS and ActivePS.
static PSMutex gPSMutex;

Atomic<uint32_t, MemoryOrdering::Relaxed> RacyFeatures::sActiveAndFeatures(0);

// Each live thread has a RegisteredThread, and we store a reference to it in
// TLS. This class encapsulates that TLS.
class TLSRegisteredThread {
 public:
  static bool Init(PSLockRef) {
    bool ok1 = sRegisteredThread.init();
    bool ok2 = AutoProfilerLabel::sProfilingStackOwnerTLS.init();
    return ok1 && ok2;
  }

  // Get the entire RegisteredThread. Accesses are guarded by gPSMutex.
  static class RegisteredThread* RegisteredThread(PSLockRef) {
    return sRegisteredThread.get();
  }

  // Get only the RacyRegisteredThread. Accesses are not guarded by gPSMutex.
  static class RacyRegisteredThread* RacyRegisteredThread() {
    class RegisteredThread* registeredThread = sRegisteredThread.get();
    return registeredThread ? &registeredThread->RacyRegisteredThread()
                            : nullptr;
  }

  // Get only the ProfilingStack. Accesses are not guarded by gPSMutex.
  // RacyRegisteredThread() can also be used to get the ProfilingStack, but that
  // is marginally slower because it requires an extra pointer indirection.
  static ProfilingStack* Stack() {
    ProfilingStackOwner* profilingStackOwner =
        AutoProfilerLabel::sProfilingStackOwnerTLS.get();
    if (!profilingStackOwner) {
      return nullptr;
    }
    return &profilingStackOwner->ProfilingStack();
  }

  static void SetRegisteredThreadAndAutoProfilerLabelProfilingStack(
      PSLockRef, class RegisteredThread* aRegisteredThread) {
    MOZ_RELEASE_ASSERT(
        aRegisteredThread,
        "Use ResetRegisteredThread() instead of SetRegisteredThread(nullptr)");
    sRegisteredThread.set(aRegisteredThread);
    ProfilingStackOwner& profilingStackOwner =
        aRegisteredThread->RacyRegisteredThread().ProfilingStackOwner();
    profilingStackOwner.AddRef();
    AutoProfilerLabel::sProfilingStackOwnerTLS.set(&profilingStackOwner);
  }

  // Only reset the registered thread. The AutoProfilerLabel's ProfilingStack
  // is kept, because the thread may not have unregistered itself yet, so it may
  // still push/pop labels even after the profiler has shut down.
  static void ResetRegisteredThread(PSLockRef) {
    sRegisteredThread.set(nullptr);
  }

  // Reset the AutoProfilerLabels' ProfilingStack, because the thread is
  // unregistering itself.
  static void ResetAutoProfilerLabelProfilingStack(PSLockRef) {
    MOZ_RELEASE_ASSERT(
        AutoProfilerLabel::sProfilingStackOwnerTLS.get(),
        "ResetAutoProfilerLabelProfilingStack should only be called once");
    AutoProfilerLabel::sProfilingStackOwnerTLS.get()->Release();
    AutoProfilerLabel::sProfilingStackOwnerTLS.set(nullptr);
  }

 private:
  // This is a non-owning reference to the RegisteredThread;
  // CorePS::mRegisteredThreads is the owning reference. On thread
  // deregistration, this reference is cleared and the RegisteredThread is
  // destroyed.
  static MOZ_THREAD_LOCAL(class RegisteredThread*) sRegisteredThread;
};

MOZ_THREAD_LOCAL(RegisteredThread*) TLSRegisteredThread::sRegisteredThread;

// Although you can access a thread's ProfilingStack via
// TLSRegisteredThread::sRegisteredThread, we also have a second TLS pointer
// directly to the ProfilingStack. Here's why.
//
// - We need to be able to push to and pop from the ProfilingStack in
//   AutoProfilerLabel.
//
// - The class functions are hot and must be defined in GeckoProfiler.h so they
//   can be inlined.
//
// - We don't want to expose TLSRegisteredThread (and RegisteredThread) in
//   GeckoProfiler.h.
//
// This second pointer isn't ideal, but does provide a way to satisfy those
// constraints. TLSRegisteredThread is responsible for updating it.
//
// The (Racy)RegisteredThread and AutoProfilerLabel::sProfilingStackOwnerTLS
// co-own the thread's ProfilingStack, so whichever is reset second, is
// responsible for destroying the ProfilingStack; Because MOZ_THREAD_LOCAL
// doesn't support RefPtr, AddRef&Release are done explicitly in
// TLSRegisteredThread.
MOZ_THREAD_LOCAL(ProfilingStackOwner*)
AutoProfilerLabel::sProfilingStackOwnerTLS;

void ProfilingStackOwner::DumpStackAndCrash() const {
  fprintf(stderr,
          "ProfilingStackOwner::DumpStackAndCrash() thread id: %d, size: %u\n",
          profiler_current_thread_id(), unsigned(mProfilingStack.stackSize()));
  js::ProfilingStackFrame* allFrames = mProfilingStack.frames;
  for (uint32_t i = 0; i < mProfilingStack.stackSize(); i++) {
    js::ProfilingStackFrame& frame = allFrames[i];
    if (frame.isLabelFrame()) {
      fprintf(stderr, "%u: label frame, sp=%p, label='%s' (%s)\n", unsigned(i),
              frame.stackAddress(), frame.label(),
              frame.dynamicString() ? frame.dynamicString() : "-");
    } else {
      fprintf(stderr, "%u: non-label frame\n", unsigned(i));
    }
  }

  MOZ_CRASH("Non-empty stack!");
}

// The name of the main thread.
static const char* const kMainThreadName = "GeckoMain";

////////////////////////////////////////////////////////////////////////
// BEGIN sampling/unwinding code

// The registers used for stack unwinding and a few other sampling purposes.
// The ctor does nothing; users are responsible for filling in the fields.
class Registers {
 public:
  Registers() : mPC{nullptr}, mSP{nullptr}, mFP{nullptr}, mLR{nullptr} {}

#if defined(HAVE_NATIVE_UNWIND)
  // Fills in mPC, mSP, mFP, mLR, and mContext for a synchronous sample.
  void SyncPopulate();
#endif

  void Clear() { memset(this, 0, sizeof(*this)); }

  // These fields are filled in by
  // Sampler::SuspendAndSampleAndResumeThread() for periodic and backtrace
  // samples, and by SyncPopulate() for synchronous samples.
  Address mPC;  // Instruction pointer.
  Address mSP;  // Stack pointer.
  Address mFP;  // Frame pointer.
  Address mLR;  // ARM link register.
#if defined(GP_OS_linux) || defined(GP_OS_android)
  // This contains all the registers, which means it duplicates the four fields
  // above. This is ok.
  ucontext_t* mContext;  // The context from the signal handler.
#endif
};

// Setting MAX_NATIVE_FRAMES too high risks the unwinder wasting a lot of time
// looping on corrupted stacks.
static const size_t MAX_NATIVE_FRAMES = 1024;

struct NativeStack {
  void* mPCs[MAX_NATIVE_FRAMES];
  void* mSPs[MAX_NATIVE_FRAMES];
  size_t mCount;  // Number of frames filled.

  NativeStack() : mPCs(), mSPs(), mCount(0) {}
};

Atomic<bool> WALKING_JS_STACK(false);

struct AutoWalkJSStack {
  bool walkAllowed;

  AutoWalkJSStack() : walkAllowed(false) {
    walkAllowed = WALKING_JS_STACK.compareExchange(false, true);
  }

  ~AutoWalkJSStack() {
    if (walkAllowed) {
      WALKING_JS_STACK = false;
    }
  }
};

// Merges the profiling stack, native stack, and JS stack, outputting the
// details to aCollector.
static void MergeStacks(uint32_t aFeatures, bool aIsSynchronous,
                        const RegisteredThread& aRegisteredThread,
                        const Registers& aRegs, const NativeStack& aNativeStack,
                        ProfilerStackCollector& aCollector,
                        JsFrameBuffer aJsFrames) {
  // WARNING: this function runs within the profiler's "critical section".
  // WARNING: this function might be called while the profiler is inactive, and
  //          cannot rely on ActivePS.

  const ProfilingStack& profilingStack =
      aRegisteredThread.RacyRegisteredThread().ProfilingStack();
  const js::ProfilingStackFrame* profilingStackFrames = profilingStack.frames;
  uint32_t profilingStackFrameCount = profilingStack.stackSize();
  JSContext* context = aRegisteredThread.GetJSContext();

  // Make a copy of the JS stack into a JSFrame array. This is necessary since,
  // like the native stack, the JS stack is iterated youngest-to-oldest and we
  // need to iterate oldest-to-youngest when adding frames to aInfo.

  // Non-periodic sampling passes Nothing() as the buffer write position to
  // ProfilingFrameIterator to avoid incorrectly resetting the buffer position
  // of sampled JIT frames inside the JS engine.
  Maybe<uint64_t> samplePosInBuffer;
  if (!aIsSynchronous) {
    // aCollector.SamplePositionInBuffer() will return Nothing() when
    // profiler_suspend_and_sample_thread is called from the background hang
    // reporter.
    samplePosInBuffer = aCollector.SamplePositionInBuffer();
  }
  uint32_t jsCount = 0;

  // Only walk jit stack if profiling frame iterator is turned on.
  if (context && JS::IsProfilingEnabledForContext(context)) {
    AutoWalkJSStack autoWalkJSStack;

    if (autoWalkJSStack.walkAllowed) {
      JS::ProfilingFrameIterator::RegisterState registerState;
      registerState.pc = aRegs.mPC;
      registerState.sp = aRegs.mSP;
      registerState.lr = aRegs.mLR;
      registerState.fp = aRegs.mFP;

      JS::ProfilingFrameIterator jsIter(context, registerState,
                                        samplePosInBuffer);
      for (; jsCount < MAX_JS_FRAMES && !jsIter.done(); ++jsIter) {
        if (aIsSynchronous || jsIter.isWasm()) {
          uint32_t extracted =
              jsIter.extractStack(aJsFrames, jsCount, MAX_JS_FRAMES);
          jsCount += extracted;
          if (jsCount == MAX_JS_FRAMES) {
            break;
          }
        } else {
          Maybe<JS::ProfilingFrameIterator::Frame> frame =
              jsIter.getPhysicalFrameWithoutLabel();
          if (frame.isSome()) {
            aJsFrames[jsCount++] = frame.value();
          }
        }
      }
    }
  }

  // While the profiling stack array is ordered oldest-to-youngest, the JS and
  // native arrays are ordered youngest-to-oldest. We must add frames to aInfo
  // oldest-to-youngest. Thus, iterate over the profiling stack forwards and JS
  // and native arrays backwards. Note: this means the terminating condition
  // jsIndex and nativeIndex is being < 0.
  uint32_t profilingStackIndex = 0;
  int32_t jsIndex = jsCount - 1;
  int32_t nativeIndex = aNativeStack.mCount - 1;

  uint8_t* lastLabelFrameStackAddr = nullptr;
  uint8_t* jitEndStackAddr = nullptr;

  // Iterate as long as there is at least one frame remaining.
  while (profilingStackIndex != profilingStackFrameCount || jsIndex >= 0 ||
         nativeIndex >= 0) {
    // There are 1 to 3 frames available. Find and add the oldest.
    uint8_t* profilingStackAddr = nullptr;
    uint8_t* jsStackAddr = nullptr;
    uint8_t* nativeStackAddr = nullptr;
    uint8_t* jsActivationAddr = nullptr;

    if (profilingStackIndex != profilingStackFrameCount) {
      const js::ProfilingStackFrame& profilingStackFrame =
          profilingStackFrames[profilingStackIndex];

      if (profilingStackFrame.isLabelFrame() ||
          profilingStackFrame.isSpMarkerFrame()) {
        lastLabelFrameStackAddr = (uint8_t*)profilingStackFrame.stackAddress();
      }

      // Skip any JS_OSR frames. Such frames are used when the JS interpreter
      // enters a jit frame on a loop edge (via on-stack-replacement, or OSR).
      // To avoid both the profiling stack frame and jit frame being recorded
      // (and showing up twice), the interpreter marks the interpreter
      // profiling stack frame as JS_OSR to ensure that it doesn't get counted.
      if (profilingStackFrame.isOSRFrame()) {
        profilingStackIndex++;
        continue;
      }

      MOZ_ASSERT(lastLabelFrameStackAddr);
      profilingStackAddr = lastLabelFrameStackAddr;
    }

    if (jsIndex >= 0) {
      jsStackAddr = (uint8_t*)aJsFrames[jsIndex].stackAddress;
      jsActivationAddr = (uint8_t*)aJsFrames[jsIndex].activation;
    }

    if (nativeIndex >= 0) {
      nativeStackAddr = (uint8_t*)aNativeStack.mSPs[nativeIndex];
    }

    // If there's a native stack frame which has the same SP as a profiling
    // stack frame, pretend we didn't see the native stack frame.  Ditto for a
    // native stack frame which has the same SP as a JS stack frame.  In effect
    // this means profiling stack frames or JS frames trump conflicting native
    // frames.
    if (nativeStackAddr && (profilingStackAddr == nativeStackAddr ||
                            jsStackAddr == nativeStackAddr)) {
      nativeStackAddr = nullptr;
      nativeIndex--;
      MOZ_ASSERT(profilingStackAddr || jsStackAddr);
    }

    // Sanity checks.
    MOZ_ASSERT_IF(profilingStackAddr,
                  profilingStackAddr != jsStackAddr &&
                      profilingStackAddr != nativeStackAddr);
    MOZ_ASSERT_IF(jsStackAddr, jsStackAddr != profilingStackAddr &&
                                   jsStackAddr != nativeStackAddr);
    MOZ_ASSERT_IF(nativeStackAddr, nativeStackAddr != profilingStackAddr &&
                                       nativeStackAddr != jsStackAddr);

    // Check to see if profiling stack frame is top-most.
    if (profilingStackAddr > jsStackAddr &&
        profilingStackAddr > nativeStackAddr) {
      MOZ_ASSERT(profilingStackIndex < profilingStackFrameCount);
      const js::ProfilingStackFrame& profilingStackFrame =
          profilingStackFrames[profilingStackIndex];

      // Sp marker frames are just annotations and should not be recorded in
      // the profile.
      if (!profilingStackFrame.isSpMarkerFrame()) {
        // The JIT only allows the top-most frame to have a nullptr pc.
        MOZ_ASSERT_IF(
            profilingStackFrame.isJsFrame() && profilingStackFrame.script() &&
                !profilingStackFrame.pc(),
            &profilingStackFrame ==
                &profilingStack.frames[profilingStack.stackSize() - 1]);
        aCollector.CollectProfilingStackFrame(profilingStackFrame);
      }
      profilingStackIndex++;
      continue;
    }

    // Check to see if JS jit stack frame is top-most
    if (jsStackAddr > nativeStackAddr) {
      MOZ_ASSERT(jsIndex >= 0);
      const JS::ProfilingFrameIterator::Frame& jsFrame = aJsFrames[jsIndex];
      jitEndStackAddr = (uint8_t*)jsFrame.endStackAddress;
      // Stringifying non-wasm JIT frames is delayed until streaming time. To
      // re-lookup the entry in the JitcodeGlobalTable, we need to store the
      // JIT code address (OptInfoAddr) in the circular buffer.
      //
      // Note that we cannot do this when we are sychronously sampling the
      // current thread; that is, when called from profiler_get_backtrace. The
      // captured backtrace is usually externally stored for an indeterminate
      // amount of time, such as in nsRefreshDriver. Problematically, the
      // stored backtrace may be alive across a GC during which the profiler
      // itself is disabled. In that case, the JS engine is free to discard its
      // JIT code. This means that if we inserted such OptInfoAddr entries into
      // the buffer, nsRefreshDriver would now be holding on to a backtrace
      // with stale JIT code return addresses.
      if (aIsSynchronous ||
          jsFrame.kind == JS::ProfilingFrameIterator::Frame_Wasm) {
        aCollector.CollectWasmFrame(jsFrame.label);
      } else if (jsFrame.kind ==
                 JS::ProfilingFrameIterator::Frame_BaselineInterpreter) {
        // For now treat this as a C++ Interpreter frame by materializing a
        // ProfilingStackFrame.
        JSScript* script = jsFrame.interpreterScript;
        jsbytecode* pc = jsFrame.interpreterPC();
        js::ProfilingStackFrame stackFrame;
        stackFrame.initJsFrame("", jsFrame.label, script, pc, jsFrame.realmID);
        aCollector.CollectProfilingStackFrame(stackFrame);
      } else {
        MOZ_ASSERT(jsFrame.kind == JS::ProfilingFrameIterator::Frame_Ion ||
                   jsFrame.kind == JS::ProfilingFrameIterator::Frame_Baseline);
        aCollector.CollectJitReturnAddr(jsFrame.returnAddress());
      }

      jsIndex--;
      continue;
    }

    // If we reach here, there must be a native stack frame and it must be the
    // greatest frame.
    if (nativeStackAddr &&
        // If the latest JS frame was JIT, this could be the native frame that
        // corresponds to it. In that case, skip the native frame, because
        // there's no need for the same frame to be present twice in the stack.
        // The JS frame can be considered the symbolicated version of the native
        // frame.
        (!jitEndStackAddr || nativeStackAddr < jitEndStackAddr) &&
        // This might still be a JIT operation, check to make sure that is not
        // in range of the NEXT JavaScript's stacks' activation address.
        (!jsActivationAddr || nativeStackAddr > jsActivationAddr)) {
      MOZ_ASSERT(nativeIndex >= 0);
      void* addr = (void*)aNativeStack.mPCs[nativeIndex];
      aCollector.CollectNativeLeafAddr(addr);
    }
    if (nativeIndex >= 0) {
      nativeIndex--;
    }
  }

  // Update the JS context with the current profile sample buffer generation.
  //
  // Only do this for periodic samples. We don't want to do this for
  // synchronous samples, and we also don't want to do it for calls to
  // profiler_suspend_and_sample_thread() from the background hang reporter -
  // in that case, aCollector.BufferRangeStart() will return Nothing().
  if (!aIsSynchronous && context && aCollector.BufferRangeStart()) {
    uint64_t bufferRangeStart = *aCollector.BufferRangeStart();
    JS::SetJSContextProfilerSampleBufferRangeStart(context, bufferRangeStart);
  }
}

#if defined(GP_OS_windows) && defined(USE_MOZ_STACK_WALK)
static HANDLE GetThreadHandle(PlatformData* aData);
#endif

#if defined(USE_FRAME_POINTER_STACK_WALK) || defined(USE_MOZ_STACK_WALK)
static void StackWalkCallback(uint32_t aFrameNumber, void* aPC, void* aSP,
                              void* aClosure) {
  NativeStack* nativeStack = static_cast<NativeStack*>(aClosure);
  MOZ_ASSERT(nativeStack->mCount < MAX_NATIVE_FRAMES);
  nativeStack->mSPs[nativeStack->mCount] = aSP;
  nativeStack->mPCs[nativeStack->mCount] = aPC;
  nativeStack->mCount++;
}
#endif

#if defined(USE_FRAME_POINTER_STACK_WALK)
static void DoFramePointerBacktrace(PSLockRef aLock,
                                    const RegisteredThread& aRegisteredThread,
                                    const Registers& aRegs,
                                    NativeStack& aNativeStack) {
  // WARNING: this function runs within the profiler's "critical section".
  // WARNING: this function might be called while the profiler is inactive, and
  //          cannot rely on ActivePS.

  // Start with the current function. We use 0 as the frame number here because
  // the FramePointerStackWalk() call below will use 1..N. This is a bit weird
  // but it doesn't matter because StackWalkCallback() doesn't use the frame
  // number argument.
  StackWalkCallback(/* frameNum */ 0, aRegs.mPC, aRegs.mSP, &aNativeStack);

  uint32_t maxFrames = uint32_t(MAX_NATIVE_FRAMES - aNativeStack.mCount);

  const void* stackEnd = aRegisteredThread.StackTop();
  if (aRegs.mFP >= aRegs.mSP && aRegs.mFP <= stackEnd) {
    FramePointerStackWalk(StackWalkCallback, /* skipFrames */ 0, maxFrames,
                          &aNativeStack, reinterpret_cast<void**>(aRegs.mFP),
                          const_cast<void*>(stackEnd));
  }
}
#endif

#if defined(USE_MOZ_STACK_WALK)
static void DoMozStackWalkBacktrace(PSLockRef aLock,
                                    const RegisteredThread& aRegisteredThread,
                                    const Registers& aRegs,
                                    NativeStack& aNativeStack) {
  // WARNING: this function runs within the profiler's "critical section".
  // WARNING: this function might be called while the profiler is inactive, and
  //          cannot rely on ActivePS.

  // Start with the current function. We use 0 as the frame number here because
  // the MozStackWalkThread() call below will use 1..N. This is a bit weird but
  // it doesn't matter because StackWalkCallback() doesn't use the frame number
  // argument.
  StackWalkCallback(/* frameNum */ 0, aRegs.mPC, aRegs.mSP, &aNativeStack);

  uint32_t maxFrames = uint32_t(MAX_NATIVE_FRAMES - aNativeStack.mCount);

  HANDLE thread = GetThreadHandle(aRegisteredThread.GetPlatformData());
  MOZ_ASSERT(thread);
  MozStackWalkThread(StackWalkCallback, /* skipFrames */ 0, maxFrames,
                     &aNativeStack, thread, /* context */ nullptr);
}
#endif

#ifdef USE_EHABI_STACKWALK
static void DoEHABIBacktrace(PSLockRef aLock,
                             const RegisteredThread& aRegisteredThread,
                             const Registers& aRegs,
                             NativeStack& aNativeStack) {
  // WARNING: this function runs within the profiler's "critical section".
  // WARNING: this function might be called while the profiler is inactive, and
  //          cannot rely on ActivePS.

  const mcontext_t* mcontext = &aRegs.mContext->uc_mcontext;
  mcontext_t savedContext;
  const ProfilingStack& profilingStack =
      aRegisteredThread.RacyRegisteredThread().ProfilingStack();

  // The profiling stack contains an "EnterJIT" frame whenever we enter
  // JIT code with profiling enabled; the stack pointer value points
  // the saved registers.  We use this to unwind resume unwinding
  // after encounting JIT code.
  for (uint32_t i = profilingStack.stackSize(); i > 0; --i) {
    // The profiling stack grows towards higher indices, so we iterate
    // backwards (from callee to caller).
    const js::ProfilingStackFrame& frame = profilingStack.frames[i - 1];
    if (!frame.isJsFrame() && strcmp(frame.label(), "EnterJIT") == 0) {
      // Found JIT entry frame.  Unwind up to that point (i.e., force
      // the stack walk to stop before the block of saved registers;
      // note that it yields nondecreasing stack pointers), then restore
      // the saved state.
      uint32_t* vSP = reinterpret_cast<uint32_t*>(frame.stackAddress());

      aNativeStack.mCount +=
          EHABIStackWalk(*mcontext, /* stackBase = */ vSP,
                         aNativeStack.mSPs + aNativeStack.mCount,
                         aNativeStack.mPCs + aNativeStack.mCount,
                         MAX_NATIVE_FRAMES - aNativeStack.mCount);

      memset(&savedContext, 0, sizeof(savedContext));

      // See also: struct EnterJITStack in js/src/jit/arm/Trampoline-arm.cpp
      savedContext.arm_r4 = *vSP++;
      savedContext.arm_r5 = *vSP++;
      savedContext.arm_r6 = *vSP++;
      savedContext.arm_r7 = *vSP++;
      savedContext.arm_r8 = *vSP++;
      savedContext.arm_r9 = *vSP++;
      savedContext.arm_r10 = *vSP++;
      savedContext.arm_fp = *vSP++;
      savedContext.arm_lr = *vSP++;
      savedContext.arm_sp = reinterpret_cast<uint32_t>(vSP);
      savedContext.arm_pc = savedContext.arm_lr;
      mcontext = &savedContext;
    }
  }

  // Now unwind whatever's left (starting from either the last EnterJIT frame
  // or, if no EnterJIT was found, the original registers).
  aNativeStack.mCount +=
      EHABIStackWalk(*mcontext, const_cast<void*>(aRegisteredThread.StackTop()),
                     aNativeStack.mSPs + aNativeStack.mCount,
                     aNativeStack.mPCs + aNativeStack.mCount,
                     MAX_NATIVE_FRAMES - aNativeStack.mCount);
}
#endif

#ifdef USE_LUL_STACKWALK

// See the comment at the callsite for why this function is necessary.
#  if defined(MOZ_HAVE_ASAN_BLACKLIST)
MOZ_ASAN_BLACKLIST static void ASAN_memcpy(void* aDst, const void* aSrc,
                                           size_t aLen) {
  // The obvious thing to do here is call memcpy(). However, although
  // ASAN_memcpy() is not instrumented by ASAN, memcpy() still is, and the
  // false positive still manifests! So we must implement memcpy() ourselves
  // within this function.
  char* dst = static_cast<char*>(aDst);
  const char* src = static_cast<const char*>(aSrc);

  for (size_t i = 0; i < aLen; i++) {
    dst[i] = src[i];
  }
}
#  endif

static void DoLULBacktrace(PSLockRef aLock,
                           const RegisteredThread& aRegisteredThread,
                           const Registers& aRegs, NativeStack& aNativeStack) {
  // WARNING: this function runs within the profiler's "critical section".
  // WARNING: this function might be called while the profiler is inactive, and
  //          cannot rely on ActivePS.

  const mcontext_t* mc = &aRegs.mContext->uc_mcontext;

  lul::UnwindRegs startRegs;
  memset(&startRegs, 0, sizeof(startRegs));

#  if defined(GP_PLAT_amd64_linux) || defined(GP_PLAT_amd64_android)
  startRegs.xip = lul::TaggedUWord(mc->gregs[REG_RIP]);
  startRegs.xsp = lul::TaggedUWord(mc->gregs[REG_RSP]);
  startRegs.xbp = lul::TaggedUWord(mc->gregs[REG_RBP]);
#  elif defined(GP_PLAT_arm_linux) || defined(GP_PLAT_arm_android)
  startRegs.r15 = lul::TaggedUWord(mc->arm_pc);
  startRegs.r14 = lul::TaggedUWord(mc->arm_lr);
  startRegs.r13 = lul::TaggedUWord(mc->arm_sp);
  startRegs.r12 = lul::TaggedUWord(mc->arm_ip);
  startRegs.r11 = lul::TaggedUWord(mc->arm_fp);
  startRegs.r7 = lul::TaggedUWord(mc->arm_r7);
#  elif defined(GP_PLAT_arm64_linux) || defined(GP_PLAT_arm64_android)
  startRegs.pc = lul::TaggedUWord(mc->pc);
  startRegs.x29 = lul::TaggedUWord(mc->regs[29]);
  startRegs.x30 = lul::TaggedUWord(mc->regs[30]);
  startRegs.sp = lul::TaggedUWord(mc->sp);
#  elif defined(GP_PLAT_x86_linux) || defined(GP_PLAT_x86_android)
  startRegs.xip = lul::TaggedUWord(mc->gregs[REG_EIP]);
  startRegs.xsp = lul::TaggedUWord(mc->gregs[REG_ESP]);
  startRegs.xbp = lul::TaggedUWord(mc->gregs[REG_EBP]);
#  elif defined(GP_PLAT_mips64_linux)
  startRegs.pc = lul::TaggedUWord(mc->pc);
  startRegs.sp = lul::TaggedUWord(mc->gregs[29]);
  startRegs.fp = lul::TaggedUWord(mc->gregs[30]);
#  else
#    error "Unknown plat"
#  endif

  // Copy up to N_STACK_BYTES from rsp-REDZONE upwards, but not going past the
  // stack's registered top point.  Do some basic sanity checks too.  This
  // assumes that the TaggedUWord holding the stack pointer value is valid, but
  // it should be, since it was constructed that way in the code just above.

  // We could construct |stackImg| so that LUL reads directly from the stack in
  // question, rather than from a copy of it.  That would reduce overhead and
  // space use a bit.  However, it gives a problem with dynamic analysis tools
  // (ASan, TSan, Valgrind) which is that such tools will report invalid or
  // racing memory accesses, and such accesses will be reported deep inside LUL.
  // By taking a copy here, we can either sanitise the copy (for Valgrind) or
  // copy it using an unchecked memcpy (for ASan, TSan).  That way we don't have
  // to try and suppress errors inside LUL.
  //
  // N_STACK_BYTES is set to 160KB.  This is big enough to hold all stacks
  // observed in some minutes of testing, whilst keeping the size of this
  // function (DoNativeBacktrace)'s frame reasonable.  Most stacks observed in
  // practice are small, 4KB or less, and so the copy costs are insignificant
  // compared to other profiler overhead.
  //
  // |stackImg| is allocated on this (the sampling thread's) stack.  That
  // implies that the frame for this function is at least N_STACK_BYTES large.
  // In general it would be considered unacceptable to have such a large frame
  // on a stack, but it only exists for the unwinder thread, and so is not
  // expected to be a problem.  Allocating it on the heap is troublesome because
  // this function runs whilst the sampled thread is suspended, so any heap
  // allocation risks deadlock.  Allocating it as a global variable is not
  // thread safe, which would be a problem if we ever allow multiple sampler
  // threads.  Hence allocating it on the stack seems to be the least-worst
  // option.

  lul::StackImage stackImg;

  {
#  if defined(GP_PLAT_amd64_linux) || defined(GP_PLAT_amd64_android)
    uintptr_t rEDZONE_SIZE = 128;
    uintptr_t start = startRegs.xsp.Value() - rEDZONE_SIZE;
#  elif defined(GP_PLAT_arm_linux) || defined(GP_PLAT_arm_android)
    uintptr_t rEDZONE_SIZE = 0;
    uintptr_t start = startRegs.r13.Value() - rEDZONE_SIZE;
#  elif defined(GP_PLAT_arm64_linux) || defined(GP_PLAT_arm64_android)
    uintptr_t rEDZONE_SIZE = 0;
    uintptr_t start = startRegs.sp.Value() - rEDZONE_SIZE;
#  elif defined(GP_PLAT_x86_linux) || defined(GP_PLAT_x86_android)
    uintptr_t rEDZONE_SIZE = 0;
    uintptr_t start = startRegs.xsp.Value() - rEDZONE_SIZE;
#  elif defined(GP_PLAT_mips64_linux)
    uintptr_t rEDZONE_SIZE = 0;
    uintptr_t start = startRegs.sp.Value() - rEDZONE_SIZE;
#  else
#    error "Unknown plat"
#  endif
    uintptr_t end = reinterpret_cast<uintptr_t>(aRegisteredThread.StackTop());
    uintptr_t ws = sizeof(void*);
    start &= ~(ws - 1);
    end &= ~(ws - 1);
    uintptr_t nToCopy = 0;
    if (start < end) {
      nToCopy = end - start;
      if (nToCopy > lul::N_STACK_BYTES) nToCopy = lul::N_STACK_BYTES;
    }
    MOZ_ASSERT(nToCopy <= lul::N_STACK_BYTES);
    stackImg.mLen = nToCopy;
    stackImg.mStartAvma = start;
    if (nToCopy > 0) {
      // If this is a vanilla memcpy(), ASAN makes the following complaint:
      //
      //   ERROR: AddressSanitizer: stack-buffer-underflow ...
      //   ...
      //   HINT: this may be a false positive if your program uses some custom
      //   stack unwind mechanism or swapcontext
      //
      // This code is very much a custom stack unwind mechanism! So we use an
      // alternative memcpy() implementation that is ignored by ASAN.
#  if defined(MOZ_HAVE_ASAN_BLACKLIST)
      ASAN_memcpy(&stackImg.mContents[0], (void*)start, nToCopy);
#  else
      memcpy(&stackImg.mContents[0], (void*)start, nToCopy);
#  endif
      (void)VALGRIND_MAKE_MEM_DEFINED(&stackImg.mContents[0], nToCopy);
    }
  }

  size_t framePointerFramesAcquired = 0;
  lul::LUL* lul = CorePS::Lul(aLock);
  lul->Unwind(reinterpret_cast<uintptr_t*>(aNativeStack.mPCs),
              reinterpret_cast<uintptr_t*>(aNativeStack.mSPs),
              &aNativeStack.mCount, &framePointerFramesAcquired,
              MAX_NATIVE_FRAMES, &startRegs, &stackImg);

  // Update stats in the LUL stats object.  Unfortunately this requires
  // three global memory operations.
  lul->mStats.mContext += 1;
  lul->mStats.mCFI += aNativeStack.mCount - 1 - framePointerFramesAcquired;
  lul->mStats.mFP += framePointerFramesAcquired;
}

#endif

#ifdef HAVE_NATIVE_UNWIND
static void DoNativeBacktrace(PSLockRef aLock,
                              const RegisteredThread& aRegisteredThread,
                              const Registers& aRegs,
                              NativeStack& aNativeStack) {
  // This method determines which stackwalker is used for periodic and
  // synchronous samples. (Backtrace samples are treated differently, see
  // profiler_suspend_and_sample_thread() for details). The only part of the
  // ordering that matters is that LUL must precede FRAME_POINTER, because on
  // Linux they can both be present.
#  if defined(USE_LUL_STACKWALK)
  DoLULBacktrace(aLock, aRegisteredThread, aRegs, aNativeStack);
#  elif defined(USE_EHABI_STACKWALK)
  DoEHABIBacktrace(aLock, aRegisteredThread, aRegs, aNativeStack);
#  elif defined(USE_FRAME_POINTER_STACK_WALK)
  DoFramePointerBacktrace(aLock, aRegisteredThread, aRegs, aNativeStack);
#  elif defined(USE_MOZ_STACK_WALK)
  DoMozStackWalkBacktrace(aLock, aRegisteredThread, aRegs, aNativeStack);
#  else
#    error "Invalid configuration"
#  endif
}
#endif

// Writes some components shared by periodic and synchronous profiles to
// ActivePS's ProfileBuffer. (This should only be called from DoSyncSample()
// and DoPeriodicSample().)
//
// The grammar for entry sequences is in a comment above
// ProfileBuffer::StreamSamplesToJSON.
static inline void DoSharedSample(PSLockRef aLock, bool aIsSynchronous,
                                  RegisteredThread& aRegisteredThread,
                                  const Registers& aRegs, uint64_t aSamplePos,
                                  ProfileBuffer& aBuffer) {
  // WARNING: this function runs within the profiler's "critical section".

  MOZ_ASSERT(!aBuffer.IsThreadSafe(),
             "Mutexes cannot be used inside this critical section");

  MOZ_RELEASE_ASSERT(ActivePS::Exists(aLock));

  ProfileBufferCollector collector(aBuffer, ActivePS::Features(aLock),
                                   aSamplePos);
  NativeStack nativeStack;
#if defined(HAVE_NATIVE_UNWIND)
  if (ActivePS::FeatureStackWalk(aLock)) {
    DoNativeBacktrace(aLock, aRegisteredThread, aRegs, nativeStack);

    MergeStacks(ActivePS::Features(aLock), aIsSynchronous, aRegisteredThread,
                aRegs, nativeStack, collector, CorePS::JsFrames(aLock));
  } else
#endif
  {
    MergeStacks(ActivePS::Features(aLock), aIsSynchronous, aRegisteredThread,
                aRegs, nativeStack, collector, CorePS::JsFrames(aLock));

    // We can't walk the whole native stack, but we can record the top frame.
    if (ActivePS::FeatureLeaf(aLock)) {
      aBuffer.AddEntry(ProfileBufferEntry::NativeLeafAddr((void*)aRegs.mPC));
    }
  }
}

// Writes the components of a synchronous sample to the given ProfileBuffer.
static void DoSyncSample(PSLockRef aLock, RegisteredThread& aRegisteredThread,
                         const TimeStamp& aNow, const Registers& aRegs,
                         ProfileBuffer& aBuffer) {
  // WARNING: this function runs within the profiler's "critical section".

  uint64_t samplePos =
      aBuffer.AddThreadIdEntry(aRegisteredThread.Info()->ThreadId());

  TimeDuration delta = aNow - CorePS::ProcessStartTime();
  aBuffer.AddEntry(ProfileBufferEntry::Time(delta.ToMilliseconds()));

  DoSharedSample(aLock, /* aIsSynchronous = */ true, aRegisteredThread, aRegs,
                 samplePos, aBuffer);
}

// Writes the components of a periodic sample to ActivePS's ProfileBuffer.
// The ThreadId entry is already written in the main ProfileBuffer, its location
// is `aSamplePos`, we can write the rest to `aBuffer` (which may be different).
static inline void DoPeriodicSample(PSLockRef aLock,
                                    RegisteredThread& aRegisteredThread,
                                    ProfiledThreadData& aProfiledThreadData,
                                    const TimeStamp& aNow,
                                    const Registers& aRegs, uint64_t aSamplePos,
                                    ProfileBuffer& aBuffer) {
  // WARNING: this function runs within the profiler's "critical section".

  DoSharedSample(aLock, /* aIsSynchronous = */ false, aRegisteredThread, aRegs,
                 aSamplePos, aBuffer);
}

// END sampling/unwinding code
////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////
// BEGIN saving/streaming code

const static uint64_t kJS_MAX_SAFE_UINTEGER = +9007199254740991ULL;

static int64_t SafeJSInteger(uint64_t aValue) {
  return aValue <= kJS_MAX_SAFE_UINTEGER ? int64_t(aValue) : -1;
}

static void AddSharedLibraryInfoToStream(JSONWriter& aWriter,
                                         const SharedLibrary& aLib) {
  aWriter.StartObjectElement();
  aWriter.IntProperty("start", SafeJSInteger(aLib.GetStart()));
  aWriter.IntProperty("end", SafeJSInteger(aLib.GetEnd()));
  aWriter.IntProperty("offset", SafeJSInteger(aLib.GetOffset()));
  aWriter.StringProperty("name",
                         NS_ConvertUTF16toUTF8(aLib.GetModuleName()).get());
  aWriter.StringProperty("path",
                         NS_ConvertUTF16toUTF8(aLib.GetModulePath()).get());
  aWriter.StringProperty("debugName",
                         NS_ConvertUTF16toUTF8(aLib.GetDebugName()).get());
  aWriter.StringProperty("debugPath",
                         NS_ConvertUTF16toUTF8(aLib.GetDebugPath()).get());
  aWriter.StringProperty("breakpadId", aLib.GetBreakpadId().get());
  aWriter.StringProperty("arch", aLib.GetArch().c_str());
  aWriter.EndObject();
}

void AppendSharedLibraries(JSONWriter& aWriter) {
  SharedLibraryInfo info = SharedLibraryInfo::GetInfoForSelf();
  info.SortByAddress();
  for (size_t i = 0; i < info.GetSize(); i++) {
    AddSharedLibraryInfoToStream(aWriter, info.GetEntry(i));
  }
}

#ifdef MOZ_TASK_TRACER
static void StreamNameAndThreadId(JSONWriter& aWriter, const char* aName,
                                  int aThreadId) {
  aWriter.StartObjectElement();
  {
    if (XRE_GetProcessType() == GeckoProcessType_Plugin) {
      // TODO Add the proper plugin name
      aWriter.StringProperty("name", "Plugin");
    } else {
      aWriter.StringProperty("name", aName);
    }
    aWriter.IntProperty("tid", aThreadId);
  }
  aWriter.EndObject();
}
#endif

static void StreamTaskTracer(PSLockRef aLock, SpliceableJSONWriter& aWriter) {
#ifdef MOZ_TASK_TRACER
  MOZ_RELEASE_ASSERT(CorePS::Exists() && ActivePS::Exists(aLock));

  aWriter.StartArrayProperty("data");
  {
    UniquePtr<Vector<nsCString>> data =
        tasktracer::GetLoggedData(CorePS::ProcessStartTime());
    for (const nsCString& dataString : *data) {
      aWriter.StringElement(dataString.get());
    }
  }
  aWriter.EndArray();

  aWriter.StartArrayProperty("threads");
  {
    ActivePS::DiscardExpiredDeadProfiledThreads(aLock);
    Vector<std::pair<RegisteredThread*, ProfiledThreadData*>> threads =
        ActivePS::ProfiledThreads(aLock);
    for (auto& thread : threads) {
      RefPtr<ThreadInfo> info = thread.second->Info();
      StreamNameAndThreadId(aWriter, info->Name(), info->ThreadId());
    }
  }
  aWriter.EndArray();

  aWriter.DoubleProperty("start",
                         static_cast<double>(tasktracer::GetStartTime()));
#endif
}

static void StreamCategories(SpliceableJSONWriter& aWriter) {
  // Same order as ProfilingCategory. Format:
  // [
  //   {
  //     name: "Idle",
  //     color: "transparent",
  //     subcategories: ["Other"],
  //   },
  //   {
  //     name: "Other",
  //     color: "grey",
  //     subcategories: [
  //       "JSM loading",
  //       "Subprocess launching",
  //       "DLL loading"
  //     ]
  //   },
  //   ...
  // ]

#define CATEGORY_JSON_BEGIN_CATEGORY(name, labelAsString, color) \
  aWriter.Start();                                               \
  aWriter.StringProperty("name", labelAsString);                 \
  aWriter.StringProperty("color", color);                        \
  aWriter.StartArrayProperty("subcategories");
#define CATEGORY_JSON_SUBCATEGORY(supercategory, name, labelAsString) \
  aWriter.StringElement(labelAsString);
#define CATEGORY_JSON_END_CATEGORY \
  aWriter.EndArray();              \
  aWriter.EndObject();

  PROFILING_CATEGORY_LIST(CATEGORY_JSON_BEGIN_CATEGORY,
                          CATEGORY_JSON_SUBCATEGORY, CATEGORY_JSON_END_CATEGORY)

#undef CATEGORY_JSON_BEGIN_CATEGORY
#undef CATEGORY_JSON_SUBCATEGORY
#undef CATEGORY_JSON_END_CATEGORY
}

static void StreamMetaJSCustomObject(PSLockRef aLock,
                                     SpliceableJSONWriter& aWriter,
                                     bool aIsShuttingDown) {
  MOZ_RELEASE_ASSERT(CorePS::Exists() && ActivePS::Exists(aLock));

  aWriter.IntProperty("version", 19);

  // The "startTime" field holds the number of milliseconds since midnight
  // January 1, 1970 GMT. This grotty code computes (Now - (Now -
  // ProcessStartTime)) to convert CorePS::ProcessStartTime() into that form.
  TimeDuration delta = TimeStamp::NowUnfuzzed() - CorePS::ProcessStartTime();
  aWriter.DoubleProperty(
      "startTime",
      static_cast<double>(PR_Now() / 1000.0 - delta.ToMilliseconds()));

  // Write the shutdownTime field. Unlike startTime, shutdownTime is not an
  // absolute time stamp: It's relative to startTime. This is consistent with
  // all other (non-"startTime") times anywhere in the profile JSON.
  if (aIsShuttingDown) {
    aWriter.DoubleProperty("shutdownTime", profiler_time());
  } else {
    aWriter.NullProperty("shutdownTime");
  }

  aWriter.StartArrayProperty("categories");
  StreamCategories(aWriter);
  aWriter.EndArray();

  ActivePS::WriteActiveConfiguration(aLock, aWriter, "configuration");

  if (!NS_IsMainThread()) {
    // Leave the rest of the properties out if we're not on the main thread.
    // At the moment, the only case in which this function is called on a
    // background thread is if we're in a content process and are going to
    // send this profile to the parent process. In that case, the parent
    // process profile's "meta" object already has the rest of the properties,
    // and the parent process profile is dumped on that process's main thread.
    return;
  }

  aWriter.DoubleProperty("interval", ActivePS::Interval(aLock));
  aWriter.IntProperty("stackwalk", ActivePS::FeatureStackWalk(aLock));

#ifdef DEBUG
  aWriter.IntProperty("debug", 1);
#else
  aWriter.IntProperty("debug", 0);
#endif

  aWriter.IntProperty("gcpoison", JS::IsGCPoisoning() ? 1 : 0);

  bool asyncStacks = Preferences::GetBool("javascript.options.asyncstack");
  aWriter.IntProperty("asyncstack", asyncStacks);

  aWriter.IntProperty("processType", XRE_GetProcessType());

  aWriter.StringProperty("updateChannel", MOZ_STRINGIFY(MOZ_UPDATE_CHANNEL));

  nsresult res;
  nsCOMPtr<nsIHttpProtocolHandler> http =
      do_GetService(NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "http", &res);

  if (!NS_FAILED(res)) {
    nsAutoCString string;

    res = http->GetPlatform(string);
    if (!NS_FAILED(res)) {
      aWriter.StringProperty("platform", string.Data());
    }

    res = http->GetOscpu(string);
    if (!NS_FAILED(res)) {
      aWriter.StringProperty("oscpu", string.Data());
    }

    res = http->GetMisc(string);
    if (!NS_FAILED(res)) {
      aWriter.StringProperty("misc", string.Data());
    }
  }

  nsCOMPtr<nsIXULRuntime> runtime = do_GetService("@mozilla.org/xre/runtime;1");
  if (runtime) {
    nsAutoCString string;

    res = runtime->GetXPCOMABI(string);
    if (!NS_FAILED(res)) aWriter.StringProperty("abi", string.Data());

    res = runtime->GetWidgetToolkit(string);
    if (!NS_FAILED(res)) aWriter.StringProperty("toolkit", string.Data());
  }

  nsCOMPtr<nsIXULAppInfo> appInfo =
      do_GetService("@mozilla.org/xre/app-info;1");

  if (appInfo) {
    nsAutoCString string;
    res = appInfo->GetName(string);
    if (!NS_FAILED(res)) aWriter.StringProperty("product", string.Data());

    res = appInfo->GetAppBuildID(string);
    if (!NS_FAILED(res)) aWriter.StringProperty("appBuildID", string.Data());

    res = appInfo->GetSourceURL(string);
    if (!NS_FAILED(res)) aWriter.StringProperty("sourceURL", string.Data());
  }

  ProcessInfo processInfo;
  processInfo.cpuCount = 0;
  processInfo.cpuCores = 0;
  if (NS_SUCCEEDED(CollectProcessInfo(processInfo))) {
    if (processInfo.cpuCores > 0) {
      aWriter.IntProperty("physicalCPUs", processInfo.cpuCores);
    }
    if (processInfo.cpuCount > 0) {
      aWriter.IntProperty("logicalCPUs", processInfo.cpuCount);
    }
  }

  // We should avoid collecting extension metadata for profiler while XPCOM is
  // shutting down since it cannot create a new ExtensionPolicyService.
  if (!gXPCOMShuttingDown) {
    aWriter.StartObjectProperty("extensions");
    {
      {
        JSONSchemaWriter schema(aWriter);
        schema.WriteField("id");
        schema.WriteField("name");
        schema.WriteField("baseURL");
      }

      aWriter.StartArrayProperty("data");
      {
        nsTArray<RefPtr<WebExtensionPolicy>> exts;
        ExtensionPolicyService::GetSingleton().GetAll(exts);

        for (auto& ext : exts) {
          aWriter.StartArrayElement(JSONWriter::SingleLineStyle);

          nsAutoString id;
          ext->GetId(id);
          aWriter.StringElement(NS_ConvertUTF16toUTF8(id).get());

          aWriter.StringElement(NS_ConvertUTF16toUTF8(ext->Name()).get());

          auto url = ext->GetURL(NS_LITERAL_STRING(""));
          if (url.isOk()) {
            aWriter.StringElement(NS_ConvertUTF16toUTF8(url.unwrap()).get());
          }

          aWriter.EndArray();
        }
      }
      aWriter.EndArray();
    }
    aWriter.EndObject();
  }
}

static void StreamPages(PSLockRef aLock, SpliceableJSONWriter& aWriter) {
  MOZ_RELEASE_ASSERT(CorePS::Exists());
  ActivePS::DiscardExpiredPages(aLock);
  for (const auto& page : ActivePS::ProfiledPages(aLock)) {
    page->StreamJSON(aWriter);
  }
}

#if defined(GP_OS_android)
static UniquePtr<ProfileBuffer> CollectJavaThreadProfileData(
    BlocksRingBuffer& bufferManager) {
  // locked_profiler_start uses sample count is 1000 for Java thread.
  // This entry size is enough now, but we might have to estimate it
  // if we can customize it
  auto buffer = MakeUnique<ProfileBuffer>(bufferManager,
                                          MakePowerOfTwo32<8 * 1024 * 1024>());

  int sampleId = 0;
  while (true) {
    // Gets the data from the java main thread only.
    double sampleTime = java::GeckoJavaSampler::GetSampleTime(sampleId);
    if (sampleTime == 0.0) {
      break;
    }

    buffer->AddThreadIdEntry(0);
    buffer->AddEntry(ProfileBufferEntry::Time(sampleTime));
    bool parentFrameWasIdleFrame = false;
    int frameId = 0;
    while (true) {
      jni::String::LocalRef frameName =
          java::GeckoJavaSampler::GetFrameName(sampleId, frameId++);
      if (!frameName) {
        break;
      }
      nsCString frameNameString = frameName->ToCString();

      // Compute a category pair for the frame:
      //  - IDLE for the wait function android.os.MessageQueue.nativePollOnce()
      //  - OTHER for any function that's directly called by that wait function
      //  - no category on everything else
      Maybe<JS::ProfilingCategoryPair> categoryPair;
      if (frameNameString.EqualsLiteral(
              "android.os.MessageQueue.nativePollOnce()")) {
        categoryPair = Some(JS::ProfilingCategoryPair::IDLE);
        parentFrameWasIdleFrame = true;
      } else if (parentFrameWasIdleFrame) {
        categoryPair = Some(JS::ProfilingCategoryPair::OTHER);
        parentFrameWasIdleFrame = false;
      }

      buffer->CollectCodeLocation("", frameNameString.get(), 0, 0, Nothing(),
                                  Nothing(), categoryPair);
    }
    sampleId++;
  }
  return buffer;
}
#endif

UniquePtr<ProfilerCodeAddressService>
profiler_code_address_service_for_presymbolication() {
  static const bool preSymbolicate = []() {
    const char* symbolicate = getenv("MOZ_PROFILER_SYMBOLICATE");
    return symbolicate && symbolicate[0] != '\0';
  }();
  return preSymbolicate ? MakeUnique<ProfilerCodeAddressService>() : nullptr;
}

static void locked_profiler_stream_json_for_this_process(
    PSLockRef aLock, SpliceableJSONWriter& aWriter, double aSinceTime,
    bool aIsShuttingDown, ProfilerCodeAddressService* aService) {
  LOG("locked_profiler_stream_json_for_this_process");

  MOZ_RELEASE_ASSERT(CorePS::Exists() && ActivePS::Exists(aLock));

  AUTO_PROFILER_STATS(locked_profiler_stream_json_for_this_process);

  const double collectionStartMs = profiler_time();

  ProfileBuffer& buffer = ActivePS::Buffer(aLock);

  // If there is a set "Window length", discard older data.
  Maybe<double> durationS = ActivePS::Duration(aLock);
  if (durationS.isSome()) {
    const double durationStartMs = collectionStartMs - *durationS * 1000;
    buffer.DiscardSamplesBeforeTime(durationStartMs);
  }

  // Put shared library info
  aWriter.StartArrayProperty("libs");
  AppendSharedLibraries(aWriter);
  aWriter.EndArray();

  // Put meta data
  aWriter.StartObjectProperty("meta");
  { StreamMetaJSCustomObject(aLock, aWriter, aIsShuttingDown); }
  aWriter.EndObject();

  // Put page data
  aWriter.StartArrayProperty("pages");
  { StreamPages(aLock, aWriter); }
  aWriter.EndArray();

  buffer.StreamProfilerOverheadToJSON(aWriter, CorePS::ProcessStartTime(),
                                      aSinceTime);
  buffer.StreamCountersToJSON(aWriter, CorePS::ProcessStartTime(), aSinceTime);

  // Data of TaskTracer doesn't belong in the circular buffer.
  if (ActivePS::FeatureTaskTracer(aLock)) {
    aWriter.StartObjectProperty("tasktracer");
    StreamTaskTracer(aLock, aWriter);
    aWriter.EndObject();
  }

  // Lists the samples for each thread profile
  aWriter.StartArrayProperty("threads");
  {
    ActivePS::DiscardExpiredDeadProfiledThreads(aLock);
    Vector<std::pair<RegisteredThread*, ProfiledThreadData*>> threads =
        ActivePS::ProfiledThreads(aLock);
    for (auto& thread : threads) {
      RegisteredThread* registeredThread = thread.first;
      JSContext* cx =
          registeredThread ? registeredThread->GetJSContext() : nullptr;
      ProfiledThreadData* profiledThreadData = thread.second;
      profiledThreadData->StreamJSON(
          buffer, cx, aWriter, CorePS::ProcessName(aLock),
          CorePS::ProcessStartTime(), aSinceTime,
          ActivePS::FeatureJSTracer(aLock), aService);
    }

#if defined(GP_OS_android)
    if (ActivePS::FeatureJava(aLock)) {
      java::GeckoJavaSampler::Pause();

      BlocksRingBuffer bufferManager(
          BlocksRingBuffer::ThreadSafety::WithoutMutex);
      UniquePtr<ProfileBuffer> javaBuffer =
          CollectJavaThreadProfileData(bufferManager);

      // Thread id of java Main thread is 0, if we support profiling of other
      // java thread, we have to get thread id and name via JNI.
      RefPtr<ThreadInfo> threadInfo = new ThreadInfo(
          "Java Main Thread", 0, false, CorePS::ProcessStartTime());
      ProfiledThreadData profiledThreadData(threadInfo, nullptr);
      profiledThreadData.StreamJSON(*javaBuffer.get(), nullptr, aWriter,
                                    CorePS::ProcessName(aLock),
                                    CorePS::ProcessStartTime(), aSinceTime,
                                    ActivePS::FeatureJSTracer(aLock), nullptr);

      java::GeckoJavaSampler::Unpause();
    }
#endif

#ifdef MOZ_BASE_PROFILER
    UniquePtr<char[]> baseProfileThreads =
        ActivePS::MoveBaseProfileThreads(aLock);
    if (baseProfileThreads) {
      aWriter.Splice(baseProfileThreads.get());
    }
#endif
  }
  aWriter.EndArray();

  if (ActivePS::FeatureJSTracer(aLock)) {
    aWriter.StartArrayProperty("jsTracerDictionary");
    {
      JS::AutoTraceLoggerLockGuard lockGuard;
      // Collect Event Dictionary
      JS::TraceLoggerDictionaryBuffer collectionBuffer(lockGuard);
      while (collectionBuffer.NextChunk()) {
        aWriter.StringElement(collectionBuffer.internalBuffer());
      }
    }
    aWriter.EndArray();
  }

  aWriter.StartArrayProperty("pausedRanges");
  { buffer.StreamPausedRangesToJSON(aWriter, aSinceTime); }
  aWriter.EndArray();

  const double collectionEndMs = profiler_time();

  // Record timestamps for the collection into the buffer, so that consumers
  // know why we didn't collect any samples for its duration.
  // We put these entries into the buffer after we've collected the profile,
  // so they'll be visible for the *next* profile collection (if they haven't
  // been overwritten due to buffer wraparound by then).
  buffer.AddEntry(ProfileBufferEntry::CollectionStart(collectionStartMs));
  buffer.AddEntry(ProfileBufferEntry::CollectionEnd(collectionEndMs));
}

bool profiler_stream_json_for_this_process(
    SpliceableJSONWriter& aWriter, double aSinceTime, bool aIsShuttingDown,
    ProfilerCodeAddressService* aService) {
  LOG("profiler_stream_json_for_this_process");

  MOZ_RELEASE_ASSERT(CorePS::Exists());

  PSAutoLock lock(gPSMutex);

  if (!ActivePS::Exists(lock)) {
    return false;
  }

  locked_profiler_stream_json_for_this_process(lock, aWriter, aSinceTime,
                                               aIsShuttingDown, aService);
  return true;
}

// END saving/streaming code
////////////////////////////////////////////////////////////////////////

static char FeatureCategory(uint32_t aFeature) {
  if (aFeature & DefaultFeatures()) {
    if (aFeature & AvailableFeatures()) {
      return 'D';
    }
    return 'd';
  }

  if (aFeature & StartupExtraDefaultFeatures()) {
    if (aFeature & AvailableFeatures()) {
      return 'S';
    }
    return 's';
  }

  if (aFeature & AvailableFeatures()) {
    return '-';
  }
  return 'x';
}

// Doesn't exist if aExitCode is 0
static void PrintUsageThenExit(int aExitCode) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  printf(
      "\n"
      "Profiler environment variable usage:\n"
      "\n"
      "  MOZ_PROFILER_HELP\n"
      "  If set to any value, prints this message.\n"
#ifdef MOZ_BASE_PROFILER
      "  Use MOZ_BASE_PROFILER_HELP for BaseProfiler help.\n"
#endif
      "\n"
      "  MOZ_LOG\n"
      "  Enables logging. The levels of logging available are\n"
      "  'prof:3' (least verbose), 'prof:4', 'prof:5' (most verbose).\n"
      "\n"
      "  MOZ_PROFILER_STARTUP\n"
      "  If set to any value other than '' or '0'/'N'/'n', starts the\n"
      "  profiler immediately on start-up.\n"
      "  Useful if you want profile code that runs very early.\n"
      "\n"
      "  MOZ_PROFILER_STARTUP_ENTRIES=<1..>\n"
      "  If MOZ_PROFILER_STARTUP is set, specifies the number of entries per\n"
      "  process in the profiler's circular buffer when the profiler is first\n"
      "  started.\n"
      "  If unset, the platform default is used:\n"
      "  %u entries per process, or %u when MOZ_PROFILER_STARTUP is set.\n"
      "  (8 bytes per entry -> %u or %u total bytes per process)\n"
      "\n"
      "  MOZ_PROFILER_STARTUP_DURATION=<1..>\n"
      "  If MOZ_PROFILER_STARTUP is set, specifies the maximum life time of\n"
      "  entries in the the profiler's circular buffer when the profiler is\n"
      "  first started, in seconds.\n"
      "  If unset, the life time of the entries will only be restricted by\n"
      "  MOZ_PROFILER_STARTUP_ENTRIES (or its default value), and no\n"
      "  additional time duration restriction will be applied.\n"
      "\n"
      "  MOZ_PROFILER_STARTUP_INTERVAL=<1..%d>\n"
      "  If MOZ_PROFILER_STARTUP is set, specifies the sample interval,\n"
      "  measured in milliseconds, when the profiler is first started.\n"
      "  If unset, the platform default is used.\n"
      "\n"
      "  MOZ_PROFILER_STARTUP_FEATURES_BITFIELD=<Number>\n"
      "  If MOZ_PROFILER_STARTUP is set, specifies the profiling features, as\n"
      "  the integer value of the features bitfield.\n"
      "  If unset, the value from MOZ_PROFILER_STARTUP_FEATURES is used.\n"
      "\n"
      "  MOZ_PROFILER_STARTUP_FEATURES=<Features>\n"
      "  If MOZ_PROFILER_STARTUP is set, specifies the profiling features, as\n"
      "  a comma-separated list of strings.\n"
      "  Ignored if  MOZ_PROFILER_STARTUP_FEATURES_BITFIELD is set.\n"
      "  If unset, the platform default is used.\n"
      "\n"
      "    Features: (x=unavailable, D/d=default/unavailable,\n"
      "               S/s=MOZ_PROFILER_STARTUP extra default/unavailable)\n",
      unsigned(PROFILER_DEFAULT_ENTRIES.Value()),
      unsigned(PROFILER_DEFAULT_STARTUP_ENTRIES.Value()),
      unsigned(PROFILER_DEFAULT_ENTRIES.Value() * 8),
      unsigned(PROFILER_DEFAULT_STARTUP_ENTRIES.Value() * 8),
      PROFILER_MAX_INTERVAL);

#define PRINT_FEATURE(n_, str_, Name_, desc_)                                  \
  printf("    %c %6u: \"%s\" (%s)\n", FeatureCategory(ProfilerFeature::Name_), \
         ProfilerFeature::Name_, str_, desc_);

  PROFILER_FOR_EACH_FEATURE(PRINT_FEATURE)

#undef PRINT_FEATURE

  printf(
      "    -        \"default\" (All above D+S defaults)\n"
      "\n"
      "  MOZ_PROFILER_STARTUP_FILTERS=<Filters>\n"
      "  If MOZ_PROFILER_STARTUP is set, specifies the thread filters, as a\n"
      "  comma-separated list of strings. A given thread will be sampled if\n"
      "  any of the filters is a case-insensitive substring of the thread\n"
      "  name. If unset, a default is used.\n"
      "\n"
      "  MOZ_PROFILER_SHUTDOWN\n"
      "  If set, the profiler saves a profile to the named file on shutdown.\n"
      "\n"
      "  MOZ_PROFILER_SYMBOLICATE\n"
      "  If set, the profiler will pre-symbolicate profiles.\n"
      "  *Note* This will add a significant pause when gathering data, and\n"
      "  is intended mainly for local development.\n"
      "\n"
      "  MOZ_PROFILER_LUL_TEST\n"
      "  If set to any value, runs LUL unit tests at startup.\n"
      "\n"
      "  This platform %s native unwinding.\n"
      "\n",
#if defined(HAVE_NATIVE_UNWIND)
      "supports"
#else
      "does not support"
#endif
  );

  if (aExitCode != 0) {
    exit(aExitCode);
  }
}

////////////////////////////////////////////////////////////////////////
// BEGIN Sampler

#if defined(GP_OS_linux) || defined(GP_OS_android)
struct SigHandlerCoordinator;
#endif

// Sampler performs setup and teardown of the state required to sample with the
// profiler. Sampler may exist when ActivePS is not present.
//
// SuspendAndSampleAndResumeThread must only be called from a single thread,
// and must not sample the thread it is being called from. A separate Sampler
// instance must be used for each thread which wants to capture samples.

// WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
//
// With the exception of SamplerThread, all Sampler objects must be Disable-d
// before releasing the lock which was used to create them. This avoids races
// on linux with the SIGPROF signal handler.

class Sampler {
 public:
  // Sets up the profiler such that it can begin sampling.
  explicit Sampler(PSLockRef aLock);

  // Disable the sampler, restoring it to its previous state. This must be
  // called once, and only once, before the Sampler is destroyed.
  void Disable(PSLockRef aLock);

  // This method suspends and resumes the samplee thread. It calls the passed-in
  // function-like object aProcessRegs (passing it a populated |const
  // Registers&| arg) while the samplee thread is suspended.  Note that
  // the aProcessRegs function must be very careful not to do anything that
  // requires a lock, since we may have interrupted the thread at any point.
  // As an example, you can't call TimeStamp::Now() since on windows it
  // takes a lock on the performance counter.
  //
  // Func must be a function-like object of type `void()`.
  template <typename Func>
  void SuspendAndSampleAndResumeThread(
      PSLockRef aLock, const RegisteredThread& aRegisteredThread,
      const TimeStamp& aNow, const Func& aProcessRegs);

 private:
#if defined(GP_OS_linux) || defined(GP_OS_android)
  // Used to restore the SIGPROF handler when ours is removed.
  struct sigaction mOldSigprofHandler;

  // This process' ID. Needed as an argument for tgkill in
  // SuspendAndSampleAndResumeThread.
  int mMyPid;

  // The sampler thread's ID.  Used to assert that it is not sampling itself,
  // which would lead to deadlock.
  int mSamplerTid;

 public:
  // This is the one-and-only variable used to communicate between the sampler
  // thread and the samplee thread's signal handler. It's static because the
  // samplee thread's signal handler is static.
  static struct SigHandlerCoordinator* sSigHandlerCoordinator;
#endif
};

// END Sampler
////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////
// BEGIN SamplerThread

// The sampler thread controls sampling and runs whenever the profiler is
// active. It periodically runs through all registered threads, finds those
// that should be sampled, then pauses and samples them.

class SamplerThread {
 public:
  // Creates a sampler thread, but doesn't start it.
  SamplerThread(PSLockRef aLock, uint32_t aActivityGeneration,
                double aIntervalMilliseconds);
  ~SamplerThread();

  // This runs on (is!) the sampler thread.
  void Run();

  // This runs on the main thread.
  void Stop(PSLockRef aLock);

  void AppendPostSamplingCallback(PSLockRef, PostSamplingCallback&& aCallback) {
    // We are under lock, so it's safe to just modify the list pointer.
    // Also this means the sampler has not started its run yet, so any callback
    // added now will be invoked at the end of the next loop; this guarantees
    // that the callback will be invoked after at least one full sampling loop.
    mPostSamplingCallbackList = MakeUnique<PostSamplingCallbackListItem>(
        std::move(mPostSamplingCallbackList), std::move(aCallback));
  }

 private:
  // Item containing a post-sampling callback, and a tail-list of more items.
  // Using a linked list means no need to move items when adding more, and
  // "stealing" the whole list is one pointer move.
  struct PostSamplingCallbackListItem {
    UniquePtr<PostSamplingCallbackListItem> mPrev;
    PostSamplingCallback mCallback;

    PostSamplingCallbackListItem(UniquePtr<PostSamplingCallbackListItem> aPrev,
                                 PostSamplingCallback&& aCallback)
        : mPrev(std::move(aPrev)), mCallback(std::move(aCallback)) {}
  };

  [[nodiscard]] UniquePtr<PostSamplingCallbackListItem>
  TakePostSamplingCallbacks(PSLockRef) {
    return std::move(mPostSamplingCallbackList);
  }

  static void InvokePostSamplingCallbacks(
      UniquePtr<PostSamplingCallbackListItem> aCallbacks,
      SamplingState aSamplingState) {
    if (!aCallbacks) {
      return;
    }
    // We want to drill down to the last element in this list, which is the
    // oldest one, so that we invoke them in FIFO order.
    // We don't expect many callbacks, so it's safe to recurse. Note that we're
    // moving-from the UniquePtr, so the tail will implicitly get destroyed.
    InvokePostSamplingCallbacks(std::move(aCallbacks->mPrev), aSamplingState);
    // We are going to destroy this item, so we can safely move-from the
    // callback before calling it (in case it has an rvalue-ref-qualified call
    // operator).
    std::move(aCallbacks->mCallback)(aSamplingState);
    // It may be tempting for a future maintainer to change aCallbacks into an
    // rvalue reference; this will remind them not to do that!
    static_assert(
        std::is_same_v<decltype(aCallbacks),
                       UniquePtr<PostSamplingCallbackListItem>>,
        "We need to capture the list by-value, to implicitly destroy it");
  }

  // This suspends the calling thread for the given number of microseconds.
  // Best effort timing.
  void SleepMicro(uint32_t aMicroseconds);

  // The sampler used to suspend and sample threads.
  Sampler mSampler;

  // The activity generation, for detecting when the sampler thread must stop.
  const uint32_t mActivityGeneration;

  // The interval between samples, measured in microseconds.
  const int mIntervalMicroseconds;

  // The OS-specific handle for the sampler thread.
#if defined(GP_OS_windows)
  HANDLE mThread;
#elif defined(GP_OS_darwin) || defined(GP_OS_linux) || defined(GP_OS_android)
  pthread_t mThread;
#endif

  // Post-sampling callbacks are kept in a simple linked list, which will be
  // stolen by the sampler thread at the end of its next run.
  UniquePtr<PostSamplingCallbackListItem> mPostSamplingCallbackList;

  SamplerThread(const SamplerThread&) = delete;
  void operator=(const SamplerThread&) = delete;
};

// [[nodiscard]] static
bool ActivePS::AppendPostSamplingCallback(PSLockRef aLock,
                                          PostSamplingCallback&& aCallback) {
  if (!sInstance || !sInstance->mSamplerThread) {
    return false;
  }
  sInstance->mSamplerThread->AppendPostSamplingCallback(aLock,
                                                        std::move(aCallback));
  return true;
}

// This function is required because we need to create a SamplerThread within
// ActivePS's constructor, but SamplerThread is defined after ActivePS. It
// could probably be removed by moving some code around.
static SamplerThread* NewSamplerThread(PSLockRef aLock, uint32_t aGeneration,
                                       double aInterval) {
  return new SamplerThread(aLock, aGeneration, aInterval);
}

// This function is the sampler thread.  This implementation is used for all
// targets.
void SamplerThread::Run() {
  PR_SetCurrentThreadName("SamplerThread");

  // Features won't change during this SamplerThread's lifetime, so we can
  // determine now whether stack sampling is required.
  const bool noStackSampling = []() {
    PSAutoLock lock(gPSMutex);
    if (!ActivePS::Exists(lock)) {
      // If there is no active profiler, it doesn't matter what we return,
      // because this thread will exit before any stack sampling is attempted.
      return false;
    }
    return ActivePS::FeatureNoStackSampling(lock);
  }();

  // Use local BlocksRingBuffer&ProfileBuffer to capture the stack.
  // (This is to avoid touching the CorePS::BlocksRingBuffer lock while
  // a thread is suspended, because that thread could be working with
  // the CorePS::BlocksRingBuffer as well.)
  BlocksRingBuffer localBlocksRingBuffer(
      BlocksRingBuffer::ThreadSafety::WithoutMutex);
  ProfileBuffer localProfileBuffer(localBlocksRingBuffer,
                                   MakePowerOfTwo32<65536>());

  // Will be kept between collections, to know what each collection does.
  auto previousState = localBlocksRingBuffer.GetState();

  // This will be positive if we are running behind schedule (sampling less
  // frequently than desired) and negative if we are ahead of schedule.
  TimeDuration lastSleepOvershoot = 0;
  TimeStamp sampleStart = TimeStamp::NowUnfuzzed();

  // This will be set inside the loop, from inside the lock scope, to capture
  // all callbacks added before that, but none after the lock is released.
  UniquePtr<PostSamplingCallbackListItem> postSamplingCallbacks;
  // This will be set inside the loop, before invoking callbacks outside.
  SamplingState samplingState{};

  while (true) {
    // This scope is for |lock|. It ends before we sleep below.
    {
      // There should be no local callbacks left from a previous loop.
      MOZ_ASSERT(!postSamplingCallbacks);

      PSAutoLock lock(gPSMutex);
      TimeStamp lockAcquired = TimeStamp::NowUnfuzzed();

      // Move all the post-sampling callbacks locally, so that new ones cannot
      // sneak in between the end of the lock scope and the invocation after it.
      postSamplingCallbacks = TakePostSamplingCallbacks(lock);

      if (!ActivePS::Exists(lock)) {
        // Exit the `while` loop, including the lock scope, before invoking
        // callbacks and returning.
        samplingState = SamplingState::JustStopped;
        break;
      }

      // At this point profiler_stop() might have been called, and
      // profiler_start() might have been called on another thread. If this
      // happens the generation won't match.
      if (ActivePS::Generation(lock) != mActivityGeneration) {
        samplingState = SamplingState::JustStopped;
        // Exit the `while` loop, including the lock scope, before invoking
        // callbacks and returning.
        break;
      }

      ActivePS::ClearExpiredExitProfiles(lock);

      TimeStamp expiredMarkersCleaned = TimeStamp::NowUnfuzzed();

      if (!ActivePS::IsPaused(lock)) {
        TimeDuration delta = sampleStart - CorePS::ProcessStartTime();
        ProfileBuffer& buffer = ActivePS::Buffer(lock);

        // handle per-process generic counters
        const Vector<BaseProfilerCount*>& counters = CorePS::Counters(lock);
        for (auto& counter : counters) {
          // create Buffer entries for each counter
          buffer.AddEntry(ProfileBufferEntry::CounterId(counter));
          buffer.AddEntry(ProfileBufferEntry::Time(delta.ToMilliseconds()));
          // XXX support keyed maps of counts
          // In the future, we'll support keyed counters - for example, counters
          // with a key which is a thread ID. For "simple" counters we'll just
          // use a key of 0.
          int64_t count;
          uint64_t number;
          counter->Sample(count, number);
          buffer.AddEntry(ProfileBufferEntry::CounterKey(0));
          buffer.AddEntry(ProfileBufferEntry::Count(count));
          if (number) {
            buffer.AddEntry(ProfileBufferEntry::Number(number));
          }
        }
        TimeStamp countersSampled = TimeStamp::NowUnfuzzed();

        if (!noStackSampling) {
          samplingState = SamplingState::SamplingCompleted;

          const Vector<LiveProfiledThreadData>& liveThreads =
              ActivePS::LiveProfiledThreads(lock);

          for (auto& thread : liveThreads) {
            RegisteredThread* registeredThread = thread.mRegisteredThread;
            ProfiledThreadData* profiledThreadData =
                thread.mProfiledThreadData.get();
            RefPtr<ThreadInfo> info = registeredThread->Info();

            // If the thread is asleep and has been sampled before in the same
            // sleep episode, find and copy the previous sample, as that's
            // cheaper than taking a new sample.
            if (registeredThread->RacyRegisteredThread()
                    .CanDuplicateLastSampleDueToSleep()) {
              bool dup_ok = ActivePS::Buffer(lock).DuplicateLastSample(
                  info->ThreadId(), CorePS::ProcessStartTime(),
                  profiledThreadData->LastSample());
              if (dup_ok) {
                continue;
              }
            }

            AUTO_PROFILER_STATS(gecko_SamplerThread_Run_DoPeriodicSample);

            TimeStamp now = TimeStamp::NowUnfuzzed();

            // Add the thread ID now, so we know its position in the main
            // buffer, which is used by some JS data.
            // (DoPeriodicSample only knows about the temporary local buffer.)
            uint64_t samplePos =
                buffer.AddThreadIdEntry(registeredThread->Info()->ThreadId());
            profiledThreadData->LastSample() = Some(samplePos);

            // Also add the time, so it's always there after the thread ID, as
            // expected by the parser. (Other stack data is optional.)
            TimeDuration delta = now - CorePS::ProcessStartTime();
            buffer.AddEntry(ProfileBufferEntry::TimeBeforeCompactStack(
                delta.ToMilliseconds()));

            Maybe<double> unresponsiveDuration_ms;

            // Suspend the thread and collect its stack data in the local
            // buffer.
            mSampler.SuspendAndSampleAndResumeThread(
                lock, *registeredThread, now,
                [&](const Registers& aRegs, const TimeStamp& aNow) {
                  DoPeriodicSample(lock, *registeredThread, *profiledThreadData,
                                   now, aRegs, samplePos, localProfileBuffer);

                  // For "eventDelay", we want the input delay - but if
                  // there are no events in the input queue (or even if there
                  // are), we're interested in how long the delay *would* be for
                  // an input event now, which would be the time to finish the
                  // current event + the delay caused by any events already in
                  // the input queue (plus any High priority events).  Events at
                  // lower priorities (in a PrioritizedEventQueue) than Input
                  // count for input delay only for the duration that they're
                  // running, since when they finish, any queued input event
                  // would run.
                  //
                  // Unless we record the time state of all events and queue
                  // states at all times, this is hard to precisely calculate,
                  // but we can approximate it well in post-processing with
                  // RunningEventDelay and RunningEventStart.
                  //
                  // RunningEventDelay is the time duration the event was queued
                  // before starting execution.  RunningEventStart is the time
                  // the event started. (Note: since we care about Input event
                  // delays on MainThread, for PrioritizedEventQueues we return
                  // 0 for RunningEventDelay if the currently running event has
                  // a lower priority than Input (since Input events won't queue
                  // behind them).
                  //
                  // To directly measure this we would need to record the time
                  // at which the newest event currently in each queue at time X
                  // (the sample time) finishes running.  This of course would
                  // require looking into the future, or recording all this
                  // state and then post-processing it later. If we were to
                  // trace every event start and end we could do this, but it
                  // would have significant overhead to do so (and buffer
                  // usage).  From a recording of RunningEventDelays and
                  // RunningEventStarts we can infer the actual delay:
                  //
                  // clang-format off
                  // Event queue: <tail> D  :  C  :  B  : A <head>
                  // Time inserted (ms): 40 :  20 : 10  : 0
                  // Run Time (ms):      30 : 100 : 40  : 30
                  //
                  // 0    10   20   30   40   50   60   70   80   90  100  110  120  130  140  150  160  170
                  // [A||||||||||||]
                  //      ----------[B|||||||||||||||||]
                  //           -------------------------[C|||||||||||||||||||||||||||||||||||||||||||||||]
                  //                     -----------------------------------------------------------------[D|||||||||...]
                  //
                  // Calculate the delay of a new event added at time t: (run every sample)
                  //    TimeSinceRunningEventBlockedInputEvents = RunningEventDelay + (now - RunningEventStart);
                  //    effective_submission = now - TimeSinceRunningEventBlockedInputEvents;
                  //    delta = (now - last_sample_time);
                  //    last_sample_time = now;
                  //    for (t=effective_submission to now) {
                  //       delay[t] += delta;
                  //    }
                  //
                  // Can be reduced in overhead by:
                  //    TimeSinceRunningEventBlockedInputEvents = RunningEventDelay + (now - RunningEventStart);
                  //    effective_submission = now - TimeSinceRunningEventBlockedInputEvents;
                  //    if (effective_submission != last_submission) {
                  //      delta = (now - last_submision);
                  //      // this loop should be made to match each sample point in the range
                  //      // intead of assuming 1ms sampling as this pseudocode does
                  //      for (t=last_submission to effective_submission-1) {
                  //         delay[t] += delta;
                  //         delta -= 1; // assumes 1ms; adjust as needed to match for()
                  //      }
                  //      last_submission = effective_submission;
                  //    }
                  //
                  // Time  Head of queue   Running Event  RunningEventDelay  Delay of       Effective     Started    Calc (submission->now add 10ms)  Final
                  //                                                         hypothetical   Submission    Running @                                   result
                  //                                                         event E
                  // 0        Empty            A                0                30              0           0       @0=10                             30
                  // 10         B              A                0                60              0           0       @0=20, @10=10                     60
                  // 20         B              A                0               150              0           0       @0=30, @10=20, @20=10            150
                  // 30         C              B               20               140             10          30       @10=20, @20=10, @30=0            140
                  // 40         C              B               20               160                                  @10=30, @20=20...                160
                  // 50         C              B               20               150                                                                   150
                  // 60         C              B               20               140                                  @10=50, @20=40...                140
                  // 70         D              C               50               130             20          70       @20=50, @30=40...                130
                  // ...
                  // 160        D              C               50                40                                  @20=140, @30=130...               40
                  // 170      <empty>          D              140                30             40                   @40=140, @50=130... (rounding)    30
                  // 180      <empty>          D              140                20             40                   @40=150                           20
                  // 190      <empty>          D              140                10             40                   @40=160                           10
                  // 200      <empty>        <empty>            0                 0             NA                                                      0
                  //
                  // Function Delay(t) = the time between t and the time at which a hypothetical
                  // event e would start executing, if e was enqueued at time t.
                  //
                  // Delay(-1) = 0 // Before A was enqueued. No wait time, can start running
                  //               // instantly.
                  // Delay(0) = 30 // The hypothetical event e got enqueued just after A got
                  //               // enqueued. It can start running at 30, when A is done.
                  // Delay(5) = 25
                  // Delay(10) = 60 // Can start running at 70, after both A and B are done.
                  // Delay(19) = 51
                  // Delay(20) = 150 // Can start running at 170, after A, B & C.
                  // Delay(25) = 145
                  // Delay(30) = 170 // Can start running at 200, after A, B, C & D.
                  // Delay(120) = 80
                  // Delay(200) = 0 // (assuming nothing was enqueued after D)
                  //
                  // For every event that gets enqueued, the Delay time will go up by the
                  // event's running time at the time at which the event is enqueued.
                  // The Delay function will be a sawtooth of the following shape:
                  //
                  //             |\           |...
                  //             | \          |
                  //        |\   |  \         |
                  //        | \  |   \        |
                  //     |\ |  \ |    \       |
                  //  |\ | \|   \|     \      |
                  //  | \|              \     |
                  // _|                  \____|
                  //
                  //
                  // A more complex example with a PrioritizedEventQueue:
                  //
                  // Event queue: <tail> D  :  C  :  B  : A <head>
                  // Time inserted (ms): 40 :  20 : 10  : 0
                  // Run Time (ms):      30 : 100 : 40  : 30
                  // Priority:         Input: Norm: Norm: Norm
                  //
                  // 0    10   20   30   40   50   60   70   80   90  100  110  120  130  140  150  160  170
                  // [A||||||||||||]
                  //      ----------[B|||||||||||||||||]
                  //           ----------------------------------------[C|||||||||||||||||||||||||||||||||||||||||||||||]
                  //                     ---------------[D||||||||||||]
                  //
                  //
                  // Time  Head of queue   Running Event  RunningEventDelay  Delay of       Effective   Started    Calc (submission->now add 10ms)   Final
                  //                                                         hypothetical   Submission  Running @                                    result
                  //                                                         event
                  // 0        Empty            A                0                30              0           0       @0=10                             30
                  // 10         B              A                0                20              0           0       @0=20, @10=10                     20
                  // 20         B              A                0                10              0           0       @0=30, @10=20, @20=10             10
                  // 30         C              B                0                40             30          30       @30=10                            40
                  // 40         C              B                0                60             30                   @40=10, @30=20                    60
                  // 50         C              B                0                50             30                   @50=10, @40=20, @30=30            50
                  // 60         C              B                0                40             30                   @60=10, @50=20, @40=30, @30=40    40
                  // 70         C              D               30                30             40          70       @60=20, @50=30, @40=40            30
                  // 80         C              D               30                20             40          70       ...@50=40, @40=50                 20
                  // 90         C              D               30                10             40          70       ...@60=40, @50=50, @40=60         10
                  // 100      <empty>          C                0               100             100        100       @100=10                          100
                  // 110      <empty>          C                0                90             100        100       @110=10, @100=20                  90

                  //
                  // For PrioritizedEventQueue, the definition of the Delay(t) function is adjusted: the hypothetical event e has Input priority.
                  // Delay(-1) = 0 // Before A was enqueued. No wait time, can start running
                  //               // instantly.
                  // Delay(0) = 30 // The hypothetical input event e got enqueued just after A got
                  //               // enqueued. It can start running at 30, when A is done.
                  // Delay(5) = 25
                  // Delay(10) = 20
                  // Delay(25) = 5 // B has been queued, but e does not need to wait for B because e has Input priority and B does not.
                  //               // So e can start running at 30, when A is done.
                  // Delay(30) = 40 // Can start running at 70, after B is done.
                  // Delay(40) = 60 // Can start at 100, after B and D are done (D is Input Priority)
                  // Delay(80) = 20
                  // Delay(100) = 100 // Wait for C to finish

                  // clang-format on
                  //
                  // Alternatively we could insert (recycled instead of
                  // allocated/freed) input events at every sample period
                  // (1ms...), and use them to back-calculate the delay.  This
                  // might also be somewhat expensive, and would require
                  // guessing at the maximum delay, which would likely be in the
                  // seconds, and so you'd need 1000's of pre-allocated events
                  // per queue per thread - so there would be a memory impact as
                  // well.

                  TimeDuration currentEventDelay;
                  TimeDuration currentEventRunning;
                  registeredThread->GetRunningEventDelay(
                      aNow, currentEventDelay, currentEventRunning);

                  // Note: eventDelay is a different definition of
                  // responsiveness than the 16ms event injection.

                  // Don't suppress 0's for now; that can be a future
                  // optimization.  We probably want one zero to be stored
                  // before we start suppressing, which would be more
                  // complex.
                  unresponsiveDuration_ms =
                      Some(currentEventDelay.ToMilliseconds() +
                           currentEventRunning.ToMilliseconds());
                });

            // If we got eventDelay data, store it before the CompactStack.
            // Note: It is not stored inside the CompactStack so that it doesn't
            // get incorrectly duplicated when the thread is sleeping.
            if (unresponsiveDuration_ms.isSome()) {
              CorePS::CoreBlocksRingBuffer().PutObjects(
                  ProfileBufferEntry::Kind::UnresponsiveDurationMs,
                  *unresponsiveDuration_ms);
            }

            // There *must* be a CompactStack after a TimeBeforeCompactStack;
            // but note that other entries may have been concurrently inserted
            // between the TimeBeforeCompactStack above and now. If the captured
            // sample from `DoPeriodicSample` is complete, copy it into the
            // global buffer, otherwise add an empty one to satisfy the parser
            // that expects one.
            auto state = localBlocksRingBuffer.GetState();
            if (NS_WARN_IF(state.mClearedBlockCount !=
                           previousState.mClearedBlockCount)) {
              LOG("Stack sample too big for local storage, needed %u bytes",
                  unsigned(
                      state.mRangeEnd.ConvertToProfileBufferIndex() -
                      previousState.mRangeEnd.ConvertToProfileBufferIndex()));
              // There *must* be a CompactStack after a TimeBeforeCompactStack,
              // even an empty one.
              CorePS::CoreBlocksRingBuffer().PutObjects(
                  ProfileBufferEntry::Kind::CompactStack,
                  UniquePtr<BlocksRingBuffer>(nullptr));
            } else if (state.mRangeEnd.ConvertToProfileBufferIndex() -
                           previousState.mRangeEnd
                               .ConvertToProfileBufferIndex() >=
                       CorePS::CoreBlocksRingBuffer().BufferLength()->Value()) {
              LOG("Stack sample too big for profiler storage, needed %u bytes",
                  unsigned(
                      state.mRangeEnd.ConvertToProfileBufferIndex() -
                      previousState.mRangeEnd.ConvertToProfileBufferIndex()));
              // There *must* be a CompactStack after a TimeBeforeCompactStack,
              // even an empty one.
              CorePS::CoreBlocksRingBuffer().PutObjects(
                  ProfileBufferEntry::Kind::CompactStack,
                  UniquePtr<BlocksRingBuffer>(nullptr));
            } else {
              CorePS::CoreBlocksRingBuffer().PutObjects(
                  ProfileBufferEntry::Kind::CompactStack,
                  localBlocksRingBuffer);
            }

            // Clean up for the next run.
            localBlocksRingBuffer.Clear();
            previousState = localBlocksRingBuffer.GetState();
          }
        } else {
          samplingState = SamplingState::NoStackSamplingCompleted;
        }

#if defined(USE_LUL_STACKWALK)
        // The LUL unwind object accumulates frame statistics. Periodically we
        // should poke it to give it a chance to print those statistics.  This
        // involves doing I/O (fprintf, __android_log_print, etc.) and so
        // can't safely be done from the critical section inside
        // SuspendAndSampleAndResumeThread, which is why it is done here.
        CorePS::Lul(lock)->MaybeShowStats();
#endif
        TimeStamp threadsSampled = TimeStamp::NowUnfuzzed();

        buffer.CollectOverheadStats(delta, lockAcquired - sampleStart,
                                    expiredMarkersCleaned - lockAcquired,
                                    countersSampled - expiredMarkersCleaned,
                                    threadsSampled - countersSampled);
      } else {
        samplingState = SamplingState::SamplingPaused;
      }
    }
    // gPSMutex is not held after this point.

    // Invoke end-of-sampling callbacks outside of the locked scope.
    InvokePostSamplingCallbacks(std::move(postSamplingCallbacks),
                                samplingState);

    // Calculate how long a sleep to request.  After the sleep, measure how
    // long we actually slept and take the difference into account when
    // calculating the sleep interval for the next iteration.  This is an
    // attempt to keep "to schedule" in the presence of inaccuracy of the
    // actual sleep intervals.
    TimeStamp targetSleepEndTime =
        sampleStart + TimeDuration::FromMicroseconds(mIntervalMicroseconds);
    TimeStamp beforeSleep = TimeStamp::NowUnfuzzed();
    TimeDuration targetSleepDuration = targetSleepEndTime - beforeSleep;
    double sleepTime = std::max(
        0.0, (targetSleepDuration - lastSleepOvershoot).ToMicroseconds());
    SleepMicro(static_cast<uint32_t>(sleepTime));
    sampleStart = TimeStamp::NowUnfuzzed();
    lastSleepOvershoot =
        sampleStart - (beforeSleep + TimeDuration::FromMicroseconds(sleepTime));
  }

  // End of `while` loop. We can only be here from a `break` inside the loop.
  InvokePostSamplingCallbacks(std::move(postSamplingCallbacks), samplingState);
}

// We #include these files directly because it means those files can use
// declarations from this file trivially.  These provide target-specific
// implementations of all SamplerThread methods except Run().
#if defined(GP_OS_windows)
#  include "platform-win32.cpp"
#elif defined(GP_OS_darwin)
#  include "platform-macos.cpp"
#elif defined(GP_OS_linux) || defined(GP_OS_android)
#  include "platform-linux-android.cpp"
#else
#  error "bad platform"
#endif

UniquePlatformData AllocPlatformData(int aThreadId) {
  return UniquePlatformData(new PlatformData(aThreadId));
}

void PlatformDataDestructor::operator()(PlatformData* aData) { delete aData; }

// END SamplerThread
////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////
// BEGIN externally visible functions

MOZ_DEFINE_MALLOC_SIZE_OF(GeckoProfilerMallocSizeOf)

NS_IMETHODIMP
GeckoProfilerReporter::CollectReports(nsIHandleReportCallback* aHandleReport,
                                      nsISupports* aData, bool aAnonymize) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  size_t profSize = 0;
  size_t lulSize = 0;

  {
    PSAutoLock lock(gPSMutex);

    if (CorePS::Exists()) {
      CorePS::AddSizeOf(lock, GeckoProfilerMallocSizeOf, profSize, lulSize);
    }

    if (ActivePS::Exists(lock)) {
      profSize += ActivePS::SizeOf(lock, GeckoProfilerMallocSizeOf);
    }
  }

  MOZ_COLLECT_REPORT(
      "explicit/profiler/profiler-state", KIND_HEAP, UNITS_BYTES, profSize,
      "Memory used by the Gecko Profiler's global state (excluding memory used "
      "by LUL).");

#if defined(USE_LUL_STACKWALK)
  MOZ_COLLECT_REPORT(
      "explicit/profiler/lul", KIND_HEAP, UNITS_BYTES, lulSize,
      "Memory used by LUL, a stack unwinder used by the Gecko Profiler.");
#endif

  return NS_OK;
}

NS_IMPL_ISUPPORTS(GeckoProfilerReporter, nsIMemoryReporter)

static uint32_t ParseFeature(const char* aFeature, bool aIsStartup) {
  if (strcmp(aFeature, "default") == 0) {
    return (aIsStartup ? (DefaultFeatures() | StartupExtraDefaultFeatures())
                       : DefaultFeatures()) &
           AvailableFeatures();
  }

#define PARSE_FEATURE_BIT(n_, str_, Name_, desc_) \
  if (strcmp(aFeature, str_) == 0) {              \
    return ProfilerFeature::Name_;                \
  }

  PROFILER_FOR_EACH_FEATURE(PARSE_FEATURE_BIT)

#undef PARSE_FEATURE_BIT

  printf("\nUnrecognized feature \"%s\".\n\n", aFeature);
  // Since we may have an old feature we don't implement anymore, don't exit
  PrintUsageThenExit(0);
  return 0;
}

uint32_t ParseFeaturesFromStringArray(const char** aFeatures,
                                      uint32_t aFeatureCount,
                                      bool aIsStartup /* = false */) {
  uint32_t features = 0;
  for (size_t i = 0; i < aFeatureCount; i++) {
    features |= ParseFeature(aFeatures[i], aIsStartup);
  }
  return features;
}

// Find the RegisteredThread for the current thread. This should only be called
// in places where TLSRegisteredThread can't be used.
static RegisteredThread* FindCurrentThreadRegisteredThread(PSLockRef aLock) {
  int id = profiler_current_thread_id();
  const Vector<UniquePtr<RegisteredThread>>& registeredThreads =
      CorePS::RegisteredThreads(aLock);
  for (auto& registeredThread : registeredThreads) {
    if (registeredThread->Info()->ThreadId() == id) {
      return registeredThread.get();
    }
  }

  return nullptr;
}

static ProfilingStack* locked_register_thread(PSLockRef aLock,
                                              const char* aName,
                                              void* aStackTop) {
  MOZ_RELEASE_ASSERT(CorePS::Exists());

  MOZ_RELEASE_ASSERT(!FindCurrentThreadRegisteredThread(aLock));

  VTUNE_REGISTER_THREAD(aName);

  if (!TLSRegisteredThread::Init(aLock)) {
    return nullptr;
  }

  RefPtr<ThreadInfo> info =
      new ThreadInfo(aName, profiler_current_thread_id(), NS_IsMainThread());
  UniquePtr<RegisteredThread> registeredThread = MakeUnique<RegisteredThread>(
      info, NS_GetCurrentThreadNoCreate(), aStackTop);

  TLSRegisteredThread::SetRegisteredThreadAndAutoProfilerLabelProfilingStack(
      aLock, registeredThread.get());

  if (ActivePS::Exists(aLock) && ActivePS::ShouldProfileThread(aLock, info)) {
    registeredThread->RacyRegisteredThread().SetIsBeingProfiled(true);
    nsCOMPtr<nsIEventTarget> eventTarget = registeredThread->GetEventTarget();
    ProfiledThreadData* profiledThreadData = ActivePS::AddLiveProfiledThread(
        aLock, registeredThread.get(),
        MakeUnique<ProfiledThreadData>(info, eventTarget));

    if (ActivePS::FeatureJS(aLock)) {
      // This StartJSSampling() call is on-thread, so we can poll manually to
      // start JS sampling immediately.
      registeredThread->StartJSSampling(ActivePS::JSFlags(aLock));
      registeredThread->PollJSSampling();
      if (registeredThread->GetJSContext()) {
        profiledThreadData->NotifyReceivedJSContext(
            ActivePS::Buffer(aLock).BufferRangeEnd());
      }
    }
  }

  ProfilingStack* profilingStack =
      &registeredThread->RacyRegisteredThread().ProfilingStack();

  CorePS::AppendRegisteredThread(aLock, std::move(registeredThread));

  return profilingStack;
}

static void NotifyObservers(const char* aTopic,
                            nsISupports* aSubject = nullptr) {
  if (!NS_IsMainThread()) {
    // Dispatch a task to the main thread that notifies observers.
    // If NotifyObservers is called both on and off the main thread within a
    // short time, the order of the notifications can be different from the
    // order of the calls to NotifyObservers.
    // Getting the order 100% right isn't that important at the moment, because
    // these notifications are only observed in the parent process, where the
    // profiler_* functions are currently only called on the main thread.
    nsCOMPtr<nsISupports> subject = aSubject;
    NS_DispatchToMainThread(NS_NewRunnableFunction(
        "NotifyObservers", [=] { NotifyObservers(aTopic, subject); }));
    return;
  }

  if (nsCOMPtr<nsIObserverService> os = services::GetObserverService()) {
    os->NotifyObservers(aSubject, aTopic, nullptr);
  }
}

static void NotifyProfilerStarted(const PowerOfTwo32& aCapacity,
                                  const Maybe<double>& aDuration,
                                  double aInterval, uint32_t aFeatures,
                                  const char** aFilters, uint32_t aFilterCount,
                                  uint64_t aActiveBrowsingContextID) {
  nsTArray<nsCString> filtersArray;
  for (size_t i = 0; i < aFilterCount; ++i) {
    filtersArray.AppendElement(aFilters[i]);
  }

  nsCOMPtr<nsIProfilerStartParams> params = new nsProfilerStartParams(
      aCapacity.Value(), aDuration, aInterval, aFeatures,
      std::move(filtersArray), aActiveBrowsingContextID);

  ProfilerParent::ProfilerStarted(params);
  NotifyObservers("profiler-started", params);
}

static void locked_profiler_start(PSLockRef aLock, PowerOfTwo32 aCapacity,
                                  double aInterval, uint32_t aFeatures,
                                  const char** aFilters, uint32_t aFilterCount,
                                  uint64_t aActiveBrowsingContextID,
                                  const Maybe<double>& aDuration);

// This basically duplicates AutoProfilerLabel's constructor.
static void* MozGlueLabelEnter(const char* aLabel, const char* aDynamicString,
                               void* aSp) {
  ProfilingStackOwner* profilingStackOwner =
      AutoProfilerLabel::sProfilingStackOwnerTLS.get();
  if (profilingStackOwner) {
    profilingStackOwner->ProfilingStack().pushLabelFrame(
        aLabel, aDynamicString, aSp, JS::ProfilingCategoryPair::OTHER);
  }
  return profilingStackOwner;
}

// This basically duplicates AutoProfilerLabel's destructor.
static void MozGlueLabelExit(void* aProfilingStackOwner) {
  if (aProfilingStackOwner) {
    reinterpret_cast<ProfilingStackOwner*>(aProfilingStackOwner)
        ->ProfilingStack()
        .pop();
  }
}

static Vector<const char*> SplitAtCommas(const char* aString,
                                         UniquePtr<char[]>& aStorage) {
  size_t len = strlen(aString);
  aStorage = MakeUnique<char[]>(len + 1);
  PodCopy(aStorage.get(), aString, len + 1);

  // Iterate over all characters in aStorage and split at commas, by
  // overwriting commas with the null char.
  Vector<const char*> array;
  size_t currentElementStart = 0;
  for (size_t i = 0; i <= len; i++) {
    if (aStorage[i] == ',') {
      aStorage[i] = '\0';
    }
    if (aStorage[i] == '\0') {
      MOZ_RELEASE_ASSERT(array.append(&aStorage[currentElementStart]));
      currentElementStart = i + 1;
    }
  }
  return array;
}

void profiler_init_threadmanager() {
  LOG("profiler_init_threadmanager");

  PSAutoLock lock(gPSMutex);
  RegisteredThread* registeredThread =
      TLSRegisteredThread::RegisteredThread(lock);
  if (!registeredThread->GetEventTarget()) {
    registeredThread->ResetMainThread(NS_GetCurrentThreadNoCreate());
  }
}

void profiler_init(void* aStackTop) {
  LOG("profiler_init");

  VTUNE_INIT();

  MOZ_RELEASE_ASSERT(!CorePS::Exists());

  if (getenv("MOZ_PROFILER_HELP")) {
    PrintUsageThenExit(1);  // terminates execution
  }

  SharedLibraryInfo::Initialize();

  uint32_t features = DefaultFeatures() & AvailableFeatures();

  UniquePtr<char[]> filterStorage;

  Vector<const char*> filters;
  MOZ_RELEASE_ASSERT(filters.append("GeckoMain"));
  MOZ_RELEASE_ASSERT(filters.append("Compositor"));
  MOZ_RELEASE_ASSERT(filters.append("DOM Worker"));

  PowerOfTwo32 capacity = PROFILER_DEFAULT_ENTRIES;
  Maybe<double> duration = Nothing();
  double interval = PROFILER_DEFAULT_INTERVAL;

  {
    PSAutoLock lock(gPSMutex);

    // We've passed the possible failure point. Instantiate CorePS, which
    // indicates that the profiler has initialized successfully.
    CorePS::Create(lock);

    // profiler_init implicitly registers this thread as main thread.
    locked_register_thread(lock, kMainThreadName, aStackTop);

    // Platform-specific initialization.
    PlatformInit(lock);

#ifdef MOZ_TASK_TRACER
    tasktracer::InitTaskTracer();
#endif

#if defined(GP_OS_android)
    if (jni::IsAvailable()) {
      GeckoJavaSampler::Init();
    }
#endif

    // (Linux-only) We could create CorePS::mLul and read unwind info into it
    // at this point. That would match the lifetime implied by destruction of
    // it in profiler_shutdown() just below. However, that gives a big delay on
    // startup, even if no profiling is actually to be done. So, instead, it is
    // created on demand at the first call to PlatformStart().

    const char* startupEnv = getenv("MOZ_PROFILER_STARTUP");
    if (!startupEnv || startupEnv[0] == '\0' ||
        ((startupEnv[0] == '0' || startupEnv[0] == 'N' ||
          startupEnv[0] == 'n') &&
         startupEnv[1] == '\0')) {
      return;
    }

    LOG("- MOZ_PROFILER_STARTUP is set");

    // Startup default capacity may be different.
    capacity = PROFILER_DEFAULT_STARTUP_ENTRIES;

    const char* startupCapacity = getenv("MOZ_PROFILER_STARTUP_ENTRIES");
    if (startupCapacity && startupCapacity[0] != '\0') {
      errno = 0;
      long capacityLong = strtol(startupCapacity, nullptr, 10);
      // `long` could be 32 or 64 bits, so we force a 64-bit comparison with
      // the maximum 32-bit signed number (as more than that is clamped down to
      // 2^31 anyway).
      if (errno == 0 && capacityLong > 0 &&
          static_cast<uint64_t>(capacityLong) <=
              static_cast<uint64_t>(INT32_MAX)) {
        capacity = PowerOfTwo32(static_cast<uint32_t>(capacityLong));
        LOG("- MOZ_PROFILER_STARTUP_ENTRIES = %u", unsigned(capacity.Value()));
      } else {
        LOG("- MOZ_PROFILER_STARTUP_ENTRIES not a valid integer: %s",
            startupCapacity);
        PrintUsageThenExit(1);
      }
    }

    const char* startupDuration = getenv("MOZ_PROFILER_STARTUP_DURATION");
    if (startupDuration && startupDuration[0] != '\0') {
      errno = 0;
      double durationVal = PR_strtod(startupDuration, nullptr);
      if (errno == 0 && durationVal >= 0.0) {
        if (durationVal > 0.0) {
          duration = Some(durationVal);
        }
        LOG("- MOZ_PROFILER_STARTUP_DURATION = %f", durationVal);
      } else {
        LOG("- MOZ_PROFILER_STARTUP_DURATION not a valid float: %s",
            startupDuration);
        PrintUsageThenExit(1);
      }
    }

    const char* startupInterval = getenv("MOZ_PROFILER_STARTUP_INTERVAL");
    if (startupInterval && startupInterval[0] != '\0') {
      errno = 0;
      interval = PR_strtod(startupInterval, nullptr);
      if (errno == 0 && interval > 0.0 && interval <= PROFILER_MAX_INTERVAL) {
        LOG("- MOZ_PROFILER_STARTUP_INTERVAL = %f", interval);
      } else {
        LOG("- MOZ_PROFILER_STARTUP_INTERVAL not a valid float: %s",
            startupInterval);
        PrintUsageThenExit(1);
      }
    }

    features |= StartupExtraDefaultFeatures() & AvailableFeatures();

    const char* startupFeaturesBitfield =
        getenv("MOZ_PROFILER_STARTUP_FEATURES_BITFIELD");
    if (startupFeaturesBitfield && startupFeaturesBitfield[0] != '\0') {
      errno = 0;
      features = strtol(startupFeaturesBitfield, nullptr, 10);
      if (errno == 0 && features != 0) {
        LOG("- MOZ_PROFILER_STARTUP_FEATURES_BITFIELD = %d", features);
      } else {
        LOG("- MOZ_PROFILER_STARTUP_FEATURES_BITFIELD not a valid integer: %s",
            startupFeaturesBitfield);
        PrintUsageThenExit(1);
      }
    } else {
      const char* startupFeatures = getenv("MOZ_PROFILER_STARTUP_FEATURES");
      if (startupFeatures && startupFeatures[0] != '\0') {
        // Interpret startupFeatures as a list of feature strings, separated by
        // commas.
        UniquePtr<char[]> featureStringStorage;
        Vector<const char*> featureStringArray =
            SplitAtCommas(startupFeatures, featureStringStorage);
        features = ParseFeaturesFromStringArray(featureStringArray.begin(),
                                                featureStringArray.length(),
                                                /* aIsStartup */ true);
        LOG("- MOZ_PROFILER_STARTUP_FEATURES = %d", features);
      }
    }

    const char* startupFilters = getenv("MOZ_PROFILER_STARTUP_FILTERS");
    if (startupFilters && startupFilters[0] != '\0') {
      filters = SplitAtCommas(startupFilters, filterStorage);
      LOG("- MOZ_PROFILER_STARTUP_FILTERS = %s", startupFilters);
    }

    locked_profiler_start(lock, capacity, interval, features, filters.begin(),
                          filters.length(), 0, duration);
  }

#if defined(MOZ_REPLACE_MALLOC) && defined(MOZ_PROFILER_MEMORY)
  // Start counting memory allocations (outside of lock because this may call
  // profiler_add_sampled_counter which would attempt to take the lock.)
  mozilla::profiler::install_memory_hooks();
#endif

  // We do this with gPSMutex unlocked. The comment in profiler_stop() explains
  // why.
  NotifyProfilerStarted(capacity, duration, interval, features, filters.begin(),
                        filters.length(), 0);
}

static void locked_profiler_save_profile_to_file(PSLockRef aLock,
                                                 const char* aFilename,
                                                 bool aIsShuttingDown);

static SamplerThread* locked_profiler_stop(PSLockRef aLock);

void profiler_shutdown(IsFastShutdown aIsFastShutdown) {
  LOG("profiler_shutdown");

  VTUNE_SHUTDOWN();

  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  MOZ_RELEASE_ASSERT(CorePS::Exists());

  // If the profiler is active we must get a handle to the SamplerThread before
  // ActivePS is destroyed, in order to delete it.
  SamplerThread* samplerThread = nullptr;
  {
    PSAutoLock lock(gPSMutex);

    // Save the profile on shutdown if requested.
    if (ActivePS::Exists(lock)) {
      const char* filename = getenv("MOZ_PROFILER_SHUTDOWN");
      if (filename) {
        locked_profiler_save_profile_to_file(lock, filename,
                                             /* aIsShuttingDown */ true);
      }
      if (aIsFastShutdown == IsFastShutdown::Yes) {
        return;
      }

      samplerThread = locked_profiler_stop(lock);
    } else if (aIsFastShutdown == IsFastShutdown::Yes) {
      return;
    }

    CorePS::Destroy(lock);

    // We just destroyed CorePS and the ThreadInfos it contains, so we can
    // clear this thread's TLSRegisteredThread.
    TLSRegisteredThread::ResetRegisteredThread(lock);
    // We can also clear the AutoProfilerLabel's ProfilingStack because the
    // main thread should not use labels after profiler_shutdown.
    TLSRegisteredThread::ResetAutoProfilerLabelProfilingStack(lock);

#ifdef MOZ_TASK_TRACER
    tasktracer::ShutdownTaskTracer();
#endif
  }

  // We do these operations with gPSMutex unlocked. The comments in
  // profiler_stop() explain why.
  if (samplerThread) {
    ProfilerParent::ProfilerStopped();
    NotifyObservers("profiler-stopped");
    delete samplerThread;
  }
}

static bool WriteProfileToJSONWriter(SpliceableChunkedJSONWriter& aWriter,
                                     double aSinceTime, bool aIsShuttingDown,
                                     ProfilerCodeAddressService* aService) {
  LOG("WriteProfileToJSONWriter");

  MOZ_RELEASE_ASSERT(CorePS::Exists());

  aWriter.Start();
  {
    if (!profiler_stream_json_for_this_process(aWriter, aSinceTime,
                                               aIsShuttingDown, aService)) {
      return false;
    }

    // Don't include profiles from other processes because this is a
    // synchronous function.
    aWriter.StartArrayProperty("processes");
    aWriter.EndArray();
  }
  aWriter.End();
  return true;
}

void profiler_set_process_name(const nsACString& aProcessName) {
  LOG("profiler_set_process_name(\"%s\")", aProcessName.Data());
  PSAutoLock lock(gPSMutex);
  CorePS::SetProcessName(lock, aProcessName);
}

UniquePtr<char[]> profiler_get_profile(double aSinceTime,
                                       bool aIsShuttingDown) {
  LOG("profiler_get_profile");

  UniquePtr<ProfilerCodeAddressService> service =
      profiler_code_address_service_for_presymbolication();

  SpliceableChunkedJSONWriter b;
  if (!WriteProfileToJSONWriter(b, aSinceTime, aIsShuttingDown,
                                service.get())) {
    return nullptr;
  }
  return b.WriteFunc()->CopyData();
}

void profiler_get_profile_json_into_lazily_allocated_buffer(
    const std::function<char*(size_t)>& aAllocator, double aSinceTime,
    bool aIsShuttingDown) {
  LOG("profiler_get_profile_json_into_lazily_allocated_buffer");

  UniquePtr<ProfilerCodeAddressService> service =
      profiler_code_address_service_for_presymbolication();

  SpliceableChunkedJSONWriter b;
  if (!WriteProfileToJSONWriter(b, aSinceTime, aIsShuttingDown,
                                service.get())) {
    return;
  }

  b.WriteFunc()->CopyDataIntoLazilyAllocatedBuffer(aAllocator);
}

void profiler_get_start_params(int* aCapacity, Maybe<double>* aDuration,
                               double* aInterval, uint32_t* aFeatures,
                               Vector<const char*>* aFilters,
                               uint64_t* aActiveBrowsingContextID) {
  MOZ_RELEASE_ASSERT(CorePS::Exists());

  if (NS_WARN_IF(!aCapacity) || NS_WARN_IF(!aDuration) ||
      NS_WARN_IF(!aInterval) || NS_WARN_IF(!aFeatures) ||
      NS_WARN_IF(!aFilters)) {
    return;
  }

  PSAutoLock lock(gPSMutex);

  if (!ActivePS::Exists(lock)) {
    *aCapacity = 0;
    *aDuration = Nothing();
    *aInterval = 0;
    *aFeatures = 0;
    *aActiveBrowsingContextID = 0;
    aFilters->clear();
    return;
  }

  *aCapacity = ActivePS::Capacity(lock).Value();
  *aDuration = ActivePS::Duration(lock);
  *aInterval = ActivePS::Interval(lock);
  *aFeatures = ActivePS::Features(lock);
  *aActiveBrowsingContextID = ActivePS::ActiveBrowsingContextID(lock);

  const Vector<std::string>& filters = ActivePS::Filters(lock);
  MOZ_ALWAYS_TRUE(aFilters->resize(filters.length()));
  for (uint32_t i = 0; i < filters.length(); ++i) {
    (*aFilters)[i] = filters[i].c_str();
  }
}

namespace mozilla {

void GetProfilerEnvVarsForChildProcess(
    std::function<void(const char* key, const char* value)>&& aSetEnv) {
  MOZ_RELEASE_ASSERT(CorePS::Exists());

  PSAutoLock lock(gPSMutex);

  if (!ActivePS::Exists(lock)) {
    aSetEnv("MOZ_PROFILER_STARTUP", "");
    return;
  }

  aSetEnv("MOZ_PROFILER_STARTUP", "1");
  auto capacityString =
      Smprintf("%u", unsigned(ActivePS::Capacity(lock).Value()));
  aSetEnv("MOZ_PROFILER_STARTUP_ENTRIES", capacityString.get());

  // Use AppendFloat instead of Smprintf with %f because the decimal
  // separator used by %f is locale-dependent. But the string we produce needs
  // to be parseable by strtod, which only accepts the period character as a
  // decimal separator. AppendFloat always uses the period character.
  nsCString intervalString;
  intervalString.AppendFloat(ActivePS::Interval(lock));
  aSetEnv("MOZ_PROFILER_STARTUP_INTERVAL", intervalString.get());

  auto featuresString = Smprintf("%d", ActivePS::Features(lock));
  aSetEnv("MOZ_PROFILER_STARTUP_FEATURES_BITFIELD", featuresString.get());

  std::string filtersString;
  const Vector<std::string>& filters = ActivePS::Filters(lock);
  for (uint32_t i = 0; i < filters.length(); ++i) {
    filtersString += filters[i];
    if (i != filters.length() - 1) {
      filtersString += ",";
    }
  }
  aSetEnv("MOZ_PROFILER_STARTUP_FILTERS", filtersString.c_str());

#ifdef MOZ_BASE_PROFILER
  // Blindly copy MOZ_BASE_PROFILER_STARTUP* env-vars.
  auto copyEnv = [&](const char* aName) {
    const char* env = getenv(aName);
    if (!env) {
      return;
    }
    aSetEnv(aName, env);
  };
  copyEnv("MOZ_BASE_PROFILER_STARTUP");
  copyEnv("MOZ_BASE_PROFILER_STARTUP_ENTRIES");
  copyEnv("MOZ_BASE_PROFILER_STARTUP_DURATION");
  copyEnv("MOZ_BASE_PROFILER_STARTUP_INTERVAL");
  copyEnv("MOZ_BASE_PROFILER_STARTUP_FEATURES_BITFIELD");
  copyEnv("MOZ_BASE_PROFILER_STARTUP_FEATURES");
  copyEnv("MOZ_BASE_PROFILER_STARTUP_FILTERS");
#endif
}

}  // namespace mozilla

void profiler_received_exit_profile(const nsCString& aExitProfile) {
  MOZ_RELEASE_ASSERT(CorePS::Exists());
  PSAutoLock lock(gPSMutex);
  if (!ActivePS::Exists(lock)) {
    return;
  }
  ActivePS::AddExitProfile(lock, aExitProfile);
}

Vector<nsCString> profiler_move_exit_profiles() {
  MOZ_RELEASE_ASSERT(CorePS::Exists());
  PSAutoLock lock(gPSMutex);
  Vector<nsCString> profiles;
  if (ActivePS::Exists(lock)) {
    profiles = ActivePS::MoveExitProfiles(lock);
  }
  return profiles;
}

static void locked_profiler_save_profile_to_file(PSLockRef aLock,
                                                 const char* aFilename,
                                                 bool aIsShuttingDown = false) {
  LOG("locked_profiler_save_profile_to_file(%s)", aFilename);

  MOZ_RELEASE_ASSERT(CorePS::Exists() && ActivePS::Exists(aLock));

  std::ofstream stream;
  stream.open(aFilename);
  if (stream.is_open()) {
    SpliceableJSONWriter w(MakeUnique<OStreamJSONWriteFunc>(stream));
    w.Start();
    {
      locked_profiler_stream_json_for_this_process(aLock, w, /* sinceTime */ 0,
                                                   aIsShuttingDown, nullptr);

      w.StartArrayProperty("processes");
      Vector<nsCString> exitProfiles = ActivePS::MoveExitProfiles(aLock);
      for (auto& exitProfile : exitProfiles) {
        if (!exitProfile.IsEmpty()) {
          w.Splice(exitProfile.get());
        }
      }
      w.EndArray();
    }
    w.End();

    stream.close();
  }
}

void profiler_save_profile_to_file(const char* aFilename) {
  LOG("profiler_save_profile_to_file(%s)", aFilename);

  MOZ_RELEASE_ASSERT(CorePS::Exists());

  PSAutoLock lock(gPSMutex);

  if (!ActivePS::Exists(lock)) {
    return;
  }

  locked_profiler_save_profile_to_file(lock, aFilename);
}

uint32_t profiler_get_available_features() {
  MOZ_RELEASE_ASSERT(CorePS::Exists());
  return AvailableFeatures();
}

Maybe<ProfilerBufferInfo> profiler_get_buffer_info() {
  MOZ_RELEASE_ASSERT(CorePS::Exists());

  PSAutoLock lock(gPSMutex);

  if (!ActivePS::Exists(lock)) {
    return Nothing();
  }

  return Some(ActivePS::Buffer(lock).GetProfilerBufferInfo());
}

static void PollJSSamplingForCurrentThread() {
  MOZ_RELEASE_ASSERT(CorePS::Exists());

  PSAutoLock lock(gPSMutex);

  RegisteredThread* registeredThread =
      TLSRegisteredThread::RegisteredThread(lock);
  if (!registeredThread) {
    return;
  }

  registeredThread->PollJSSampling();
}

// When the profiler is started on a background thread, we can't synchronously
// call PollJSSampling on the main thread's ThreadInfo. And the next regular
// call to PollJSSampling on the main thread would only happen once the main
// thread triggers a JS interrupt callback.
// This means that all the JS execution between profiler_start() and the first
// JS interrupt would happen with JS sampling disabled, and we wouldn't get any
// JS function information for that period of time.
// So in order to start JS sampling as soon as possible, we dispatch a runnable
// to the main thread which manually calls PollJSSamplingForCurrentThread().
// In some cases this runnable will lose the race with the next JS interrupt.
// That's fine; PollJSSamplingForCurrentThread() is immune to redundant calls.
static void TriggerPollJSSamplingOnMainThread() {
  nsCOMPtr<nsIThread> mainThread;
  nsresult rv = NS_GetMainThread(getter_AddRefs(mainThread));
  if (NS_SUCCEEDED(rv) && mainThread) {
    nsCOMPtr<nsIRunnable> task =
        NS_NewRunnableFunction("TriggerPollJSSamplingOnMainThread",
                               []() { PollJSSamplingForCurrentThread(); });
    SchedulerGroup::Dispatch(TaskCategory::Other, task.forget());
  }
}

#ifdef MOZ_BASE_PROFILER
static bool HasMinimumLength(const char* aString, size_t aMinimumLength) {
  if (!aString) {
    return false;
  }
  for (size_t i = 0; i < aMinimumLength; ++i) {
    if (aString[i] == '\0') {
      return false;
    }
  }
  return true;
}
#endif  // MOZ_BASE_PROFILER

static void locked_profiler_start(PSLockRef aLock, PowerOfTwo32 aCapacity,
                                  double aInterval, uint32_t aFeatures,
                                  const char** aFilters, uint32_t aFilterCount,
                                  uint64_t aActiveBrowsingContextID,
                                  const Maybe<double>& aDuration) {
  if (LOG_TEST) {
    LOG("locked_profiler_start");
    LOG("- capacity  = %u", unsigned(aCapacity.Value()));
    LOG("- duration  = %.2f", aDuration ? *aDuration : -1);
    LOG("- interval = %.2f", aInterval);
    LOG("- browsing context ID = %" PRIu64, aActiveBrowsingContextID);

#define LOG_FEATURE(n_, str_, Name_, desc_)     \
  if (ProfilerFeature::Has##Name_(aFeatures)) { \
    LOG("- feature  = %s", str_);               \
  }

    PROFILER_FOR_EACH_FEATURE(LOG_FEATURE)

#undef LOG_FEATURE

    for (uint32_t i = 0; i < aFilterCount; i++) {
      LOG("- threads  = %s", aFilters[i]);
    }
  }

  MOZ_RELEASE_ASSERT(CorePS::Exists() && !ActivePS::Exists(aLock));

#ifdef MOZ_BASE_PROFILER
  UniquePtr<char[]> baseprofile;
  if (baseprofiler::profiler_is_active()) {
    // Note that we still hold the lock, so the sampler cannot run yet and
    // interact negatively with the still-active BaseProfiler sampler.
    // Assume that Base Profiler is active because of MOZ_BASE_PROFILER_STARTUP.
    // Capture the Base Profiler startup profile threads (if any).
    baseprofile = baseprofiler::profiler_get_profile(
        /* aSinceTime */ 0, /* aIsShuttingDown */ false,
        /* aOnlyThreads */ true);

    // Now stop Base Profiler (BP), as further recording will be ignored anyway,
    // and so that it won't clash with Gecko Profiler (GP) sampling starting
    // after the lock is dropped.
    // On Linux this is especially important to do before creating the GP
    // sampler, because the BP sampler may send a signal (to stop threads to be
    // sampled), which the GP would intercept before its own initialization is
    // complete and ready to handle such signals.
    // Note that even though `profiler_stop()` doesn't immediately destroy and
    // join the sampler thread, it safely deactivates it in such a way that the
    // thread will soon exit without doing any actual work.
    // TODO: Allow non-sampling profiling to continue.
    // TODO: Re-start BP after GP shutdown, to capture post-XPCOM shutdown.
    baseprofiler::profiler_stop();
  }
#endif

#if defined(GP_PLAT_amd64_windows)
  InitializeWin64ProfilerHooks();
#endif

  // Fall back to the default values if the passed-in values are unreasonable.
  // Less than 8192 entries (65536 bytes) may not be enough for the most complex
  // stack, so we should be able to store at least one full stack.
  // TODO: Review magic numbers.
  PowerOfTwo32 capacity =
      (aCapacity.Value() >= 8192u) ? aCapacity : PROFILER_DEFAULT_ENTRIES;
  Maybe<double> duration = aDuration;

  if (aDuration && *aDuration <= 0) {
    duration = Nothing();
  }

  double interval = aInterval > 0 ? aInterval : PROFILER_DEFAULT_INTERVAL;

  ActivePS::Create(aLock, capacity, interval, aFeatures, aFilters, aFilterCount,
                   aActiveBrowsingContextID, duration);

  // ActivePS::Create can only succeed or crash.
  MOZ_ASSERT(ActivePS::Exists(aLock));

#ifdef MOZ_BASE_PROFILER
  // An "empty" profile string may in fact contain 1 character (a newline), so
  // we want at least 2 characters to register a profile.
  if (HasMinimumLength(baseprofile.get(), 2)) {
    // The BaseProfiler startup profile will be stored as a separate "process"
    // in the Gecko Profiler profile, and shown as a new track under the
    // corresponding Gecko Profiler thread.
    ActivePS::AddBaseProfileThreads(aLock, std::move(baseprofile));
  }
#endif

  // Set up profiling for each registered thread, if appropriate.
  Maybe<int> mainThreadId;
  int tid = profiler_current_thread_id();
  const Vector<UniquePtr<RegisteredThread>>& registeredThreads =
      CorePS::RegisteredThreads(aLock);
  for (auto& registeredThread : registeredThreads) {
    RefPtr<ThreadInfo> info = registeredThread->Info();

    if (ActivePS::ShouldProfileThread(aLock, info)) {
      registeredThread->RacyRegisteredThread().SetIsBeingProfiled(true);
      nsCOMPtr<nsIEventTarget> eventTarget = registeredThread->GetEventTarget();
      ProfiledThreadData* profiledThreadData = ActivePS::AddLiveProfiledThread(
          aLock, registeredThread.get(),
          MakeUnique<ProfiledThreadData>(info, eventTarget));
      if (ActivePS::FeatureJS(aLock)) {
        registeredThread->StartJSSampling(ActivePS::JSFlags(aLock));
        if (info->ThreadId() == tid) {
          // We can manually poll the current thread so it starts sampling
          // immediately.
          registeredThread->PollJSSampling();
        } else if (info->IsMainThread()) {
          // Dispatch a runnable to the main thread to call PollJSSampling(),
          // so that we don't have wait for the next JS interrupt callback in
          // order to start profiling JS.
          TriggerPollJSSamplingOnMainThread();
        }
      }
      if (info->IsMainThread()) {
        mainThreadId = Some(info->ThreadId());
      }
      registeredThread->RacyRegisteredThread().ReinitializeOnResume();
      if (registeredThread->GetJSContext()) {
        profiledThreadData->NotifyReceivedJSContext(0);
      }
    }
  }

  // Setup support for pushing/popping labels in mozglue.
  RegisterProfilerLabelEnterExit(MozGlueLabelEnter, MozGlueLabelExit);

#ifdef MOZ_TASK_TRACER
  if (ActivePS::FeatureTaskTracer(aLock)) {
    tasktracer::StartLogging();
  }
#endif

#if defined(GP_OS_android)
  if (ActivePS::FeatureJava(aLock)) {
    int javaInterval = interval;
    // Java sampling doesn't accurately keep up with 1ms sampling.
    if (javaInterval < 10) {
      javaInterval = 10;
    }
    // Send the interval-relative entry count, but we have 100000 hard cap in
    // the java code, it can't be more than that.
    java::GeckoJavaSampler::Start(
        javaInterval, std::round((double)(capacity.Value()) * interval /
                                 (double)(javaInterval)));
  }
#endif

#if defined(MOZ_REPLACE_MALLOC) && defined(MOZ_PROFILER_MEMORY)
  if (ActivePS::FeatureNativeAllocations(aLock)) {
    if (mainThreadId.isSome()) {
      mozilla::profiler::enable_native_allocations(mainThreadId.value());
    } else {
      NS_WARNING(
          "The nativeallocations feature is turned on, but the main thread is "
          "not being profiled. The allocations are only stored on the main "
          "thread.");
    }
  }
#endif

  // At the very end, set up RacyFeatures.
  RacyFeatures::SetActive(ActivePS::Features(aLock));
}

void profiler_start(PowerOfTwo32 aCapacity, double aInterval,
                    uint32_t aFeatures, const char** aFilters,
                    uint32_t aFilterCount, uint64_t aActiveBrowsingContextID,
                    const Maybe<double>& aDuration) {
  LOG("profiler_start");

  SamplerThread* samplerThread = nullptr;
  {
    PSAutoLock lock(gPSMutex);

    // Initialize if necessary.
    if (!CorePS::Exists()) {
      profiler_init(nullptr);
    }

    // Reset the current state if the profiler is running.
    if (ActivePS::Exists(lock)) {
      samplerThread = locked_profiler_stop(lock);
    }

    locked_profiler_start(lock, aCapacity, aInterval, aFeatures, aFilters,
                          aFilterCount, aActiveBrowsingContextID, aDuration);
  }

#if defined(MOZ_REPLACE_MALLOC) && defined(MOZ_PROFILER_MEMORY)
  // Start counting memory allocations (outside of lock because this may call
  // profiler_add_sampled_counter which would attempt to take the lock.)
  mozilla::profiler::install_memory_hooks();
#endif

  // We do these operations with gPSMutex unlocked. The comments in
  // profiler_stop() explain why.
  if (samplerThread) {
    ProfilerParent::ProfilerStopped();
    NotifyObservers("profiler-stopped");
    delete samplerThread;
  }
  NotifyProfilerStarted(aCapacity, aDuration, aInterval, aFeatures, aFilters,
                        aFilterCount, aActiveBrowsingContextID);
}

void profiler_ensure_started(PowerOfTwo32 aCapacity, double aInterval,
                             uint32_t aFeatures, const char** aFilters,
                             uint32_t aFilterCount,
                             uint64_t aActiveBrowsingContextID,
                             const Maybe<double>& aDuration) {
  LOG("profiler_ensure_started");

  bool startedProfiler = false;
  SamplerThread* samplerThread = nullptr;
  {
    PSAutoLock lock(gPSMutex);

    // Initialize if necessary.
    if (!CorePS::Exists()) {
      profiler_init(nullptr);
    }

    if (ActivePS::Exists(lock)) {
      // The profiler is active.
      if (!ActivePS::Equals(lock, aCapacity, aDuration, aInterval, aFeatures,
                            aFilters, aFilterCount, aActiveBrowsingContextID)) {
        // Stop and restart with different settings.
        samplerThread = locked_profiler_stop(lock);
        locked_profiler_start(lock, aCapacity, aInterval, aFeatures, aFilters,
                              aFilterCount, aActiveBrowsingContextID,
                              aDuration);
        startedProfiler = true;
      }
    } else {
      // The profiler is stopped.
      locked_profiler_start(lock, aCapacity, aInterval, aFeatures, aFilters,
                            aFilterCount, aActiveBrowsingContextID, aDuration);
      startedProfiler = true;
    }
  }

  // We do these operations with gPSMutex unlocked. The comments in
  // profiler_stop() explain why.
  if (samplerThread) {
    ProfilerParent::ProfilerStopped();
    NotifyObservers("profiler-stopped");
    delete samplerThread;
  }

  if (startedProfiler) {
    NotifyProfilerStarted(aCapacity, aDuration, aInterval, aFeatures, aFilters,
                          aFilterCount, aActiveBrowsingContextID);
  }
}

[[nodiscard]] static SamplerThread* locked_profiler_stop(PSLockRef aLock) {
  LOG("locked_profiler_stop");

  MOZ_RELEASE_ASSERT(CorePS::Exists() && ActivePS::Exists(aLock));

  // At the very start, clear RacyFeatures.
  RacyFeatures::SetInactive();

#if defined(GP_OS_android)
  if (ActivePS::FeatureJava(aLock)) {
    java::GeckoJavaSampler::Stop();
  }
#endif

#ifdef MOZ_TASK_TRACER
  if (ActivePS::FeatureTaskTracer(aLock)) {
    tasktracer::StopLogging();
  }
#endif

  // Remove support for pushing/popping labels in mozglue.
  RegisterProfilerLabelEnterExit(nullptr, nullptr);

  // Stop sampling live threads.
  int tid = profiler_current_thread_id();
  const Vector<LiveProfiledThreadData>& liveProfiledThreads =
      ActivePS::LiveProfiledThreads(aLock);
  for (auto& thread : liveProfiledThreads) {
    RegisteredThread* registeredThread = thread.mRegisteredThread;
    registeredThread->RacyRegisteredThread().SetIsBeingProfiled(false);
    if (ActivePS::FeatureJS(aLock)) {
      registeredThread->StopJSSampling();
      RefPtr<ThreadInfo> info = registeredThread->Info();
      if (info->ThreadId() == tid) {
        // We can manually poll the current thread so it stops profiling
        // immediately.
        registeredThread->PollJSSampling();
      } else if (info->IsMainThread()) {
        // Dispatch a runnable to the main thread to call PollJSSampling(),
        // so that we don't have wait for the next JS interrupt callback in
        // order to start profiling JS.
        TriggerPollJSSamplingOnMainThread();
      }
    }
  }

#if defined(MOZ_REPLACE_MALLOC) && defined(MOZ_PROFILER_MEMORY)
  if (ActivePS::FeatureNativeAllocations(aLock)) {
    mozilla::profiler::disable_native_allocations();
  }
#endif

  // The Stop() call doesn't actually stop Run(); that happens in this
  // function's caller when the sampler thread is destroyed. Stop() just gives
  // the SamplerThread a chance to do some cleanup with gPSMutex locked.
  SamplerThread* samplerThread = ActivePS::Destroy(aLock);
  samplerThread->Stop(aLock);

  return samplerThread;
}

void profiler_stop() {
  LOG("profiler_stop");

  MOZ_RELEASE_ASSERT(CorePS::Exists());

#if defined(MOZ_REPLACE_MALLOC) && defined(MOZ_PROFILER_MEMORY)
  // Remove the hooks early, as native allocations (if they are on) can be
  // quite expensive.
  mozilla::profiler::remove_memory_hooks();
#endif

  SamplerThread* samplerThread;
  {
    PSAutoLock lock(gPSMutex);

    if (!ActivePS::Exists(lock)) {
      return;
    }

    samplerThread = locked_profiler_stop(lock);
  }

  // We notify observers with gPSMutex unlocked. Otherwise we might get a
  // deadlock, if code run by these functions calls a profiler function that
  // locks gPSMutex, for example when it wants to insert a marker.
  // (This has been seen in practise in bug 1346356, when we were still firing
  // these notifications synchronously.)
  ProfilerParent::ProfilerStopped();
  NotifyObservers("profiler-stopped");

  // We delete with gPSMutex unlocked. Otherwise we would get a deadlock: we
  // would be waiting here with gPSMutex locked for SamplerThread::Run() to
  // return so the join operation within the destructor can complete, but Run()
  // needs to lock gPSMutex to return.
  //
  // Because this call occurs with gPSMutex unlocked, it -- including the final
  // iteration of Run()'s loop -- must be able detect deactivation and return
  // in a way that's safe with respect to other gPSMutex-locking operations
  // that may have occurred in the meantime.
  delete samplerThread;
}

bool profiler_is_paused() {
  MOZ_RELEASE_ASSERT(CorePS::Exists());

  PSAutoLock lock(gPSMutex);

  if (!ActivePS::Exists(lock)) {
    return false;
  }

  return ActivePS::IsPaused(lock);
}

/* [[nodiscard]] */ bool profiler_callback_after_sampling(
    PostSamplingCallback&& aCallback) {
  LOG("profiler_callback_after_sampling");

  MOZ_RELEASE_ASSERT(CorePS::Exists());

  PSAutoLock lock(gPSMutex);

  return ActivePS::AppendPostSamplingCallback(lock, std::move(aCallback));
}

void profiler_pause() {
  LOG("profiler_pause");

  MOZ_RELEASE_ASSERT(CorePS::Exists());

  {
    PSAutoLock lock(gPSMutex);

    if (!ActivePS::Exists(lock)) {
      return;
    }

    RacyFeatures::SetPaused();
    ActivePS::SetIsPaused(lock, true);
    ActivePS::Buffer(lock).AddEntry(ProfileBufferEntry::Pause(profiler_time()));
  }

  // gPSMutex must be unlocked when we notify, to avoid potential deadlocks.
  ProfilerParent::ProfilerPaused();
  NotifyObservers("profiler-paused");
}

void profiler_resume() {
  LOG("profiler_resume");

  MOZ_RELEASE_ASSERT(CorePS::Exists());

  {
    PSAutoLock lock(gPSMutex);

    if (!ActivePS::Exists(lock)) {
      return;
    }

    ActivePS::Buffer(lock).AddEntry(
        ProfileBufferEntry::Resume(profiler_time()));
    ActivePS::SetIsPaused(lock, false);
    RacyFeatures::SetUnpaused();
  }

  // gPSMutex must be unlocked when we notify, to avoid potential deadlocks.
  ProfilerParent::ProfilerResumed();
  NotifyObservers("profiler-resumed");
}

bool profiler_feature_active(uint32_t aFeature) {
  // This function runs both on and off the main thread.

  MOZ_RELEASE_ASSERT(CorePS::Exists());

  // This function is hot enough that we use RacyFeatures, not ActivePS.
  return RacyFeatures::IsActiveWithFeature(aFeature);
}

void profiler_write_active_configuration(JSONWriter& aWriter) {
  MOZ_RELEASE_ASSERT(CorePS::Exists());
  PSAutoLock lock(gPSMutex);
  ActivePS::WriteActiveConfiguration(lock, aWriter);
}

void profiler_add_sampled_counter(BaseProfilerCount* aCounter) {
  DEBUG_LOG("profiler_add_sampled_counter(%s)", aCounter->mLabel);
  PSAutoLock lock(gPSMutex);
  CorePS::AppendCounter(lock, aCounter);
}

void profiler_remove_sampled_counter(BaseProfilerCount* aCounter) {
  DEBUG_LOG("profiler_remove_sampled_counter(%s)", aCounter->mLabel);
  PSAutoLock lock(gPSMutex);
  // Note: we don't enforce a final sample, though we could do so if the
  // profiler was active
  CorePS::RemoveCounter(lock, aCounter);
}

ProfilingStack* profiler_register_thread(const char* aName,
                                         void* aGuessStackTop) {
  DEBUG_LOG("profiler_register_thread(%s)", aName);

  MOZ_RELEASE_ASSERT(CorePS::Exists());

  // Make sure we have a nsThread wrapper for the current thread, and that NSPR
  // knows its name.
  (void)NS_GetCurrentThread();
  NS_SetCurrentThreadName(aName);

  PSAutoLock lock(gPSMutex);

  void* stackTop = GetStackTop(aGuessStackTop);
  return locked_register_thread(lock, aName, stackTop);
}

void profiler_unregister_thread() {
  PSAutoLock lock(gPSMutex);

  if (!CorePS::Exists()) {
    // This function can be called after the main thread has already shut down.
    // We want to reset the AutoProfilerLabel's ProfilingStack pointer (if
    // needed), because a thread could stay registered after the profiler has
    // shut down.
    TLSRegisteredThread::ResetAutoProfilerLabelProfilingStack(lock);
    return;
  }

  // We don't call RegisteredThread::StopJSSampling() here; there's no point
  // doing that for a JS thread that is in the process of disappearing.

  RegisteredThread* registeredThread = FindCurrentThreadRegisteredThread(lock);
  MOZ_RELEASE_ASSERT(registeredThread ==
                     TLSRegisteredThread::RegisteredThread(lock));
  if (registeredThread) {
    RefPtr<ThreadInfo> info = registeredThread->Info();

    DEBUG_LOG("profiler_unregister_thread: %s", info->Name());

    if (ActivePS::Exists(lock)) {
      ActivePS::UnregisterThread(lock, registeredThread);
    }

    // Clear the pointer to the RegisteredThread object that we're about to
    // destroy, as well as the AutoProfilerLabel's ProfilingStack because the
    // thread is unregistering itself and won't need the ProfilingStack anymore.
    TLSRegisteredThread::ResetRegisteredThread(lock);
    TLSRegisteredThread::ResetAutoProfilerLabelProfilingStack(lock);

    // Remove the thread from the list of registered threads. This deletes the
    // registeredThread object.
    CorePS::RemoveRegisteredThread(lock, registeredThread);
  } else {
    // There are two ways FindCurrentThreadRegisteredThread() might have failed.
    //
    // - TLSRegisteredThread::Init() failed in locked_register_thread().
    //
    // - We've already called profiler_unregister_thread() for this thread.
    //   (Whether or not it should, this does happen in practice.)
    //
    // Either way, TLSRegisteredThread should be empty.
    MOZ_RELEASE_ASSERT(!TLSRegisteredThread::RegisteredThread(lock));
  }
}

void profiler_register_page(uint64_t aBrowsingContextID,
                            uint64_t aInnerWindowID, const nsCString& aUrl,
                            uint64_t aEmbedderInnerWindowID) {
  DEBUG_LOG("profiler_register_page(%" PRIu64 ", %" PRIu64 ", %s, %" PRIu64 ")",
            aBrowsingContextID, aInnerWindowID, aUrl.get(),
            aEmbedderInnerWindowID);

  MOZ_RELEASE_ASSERT(CorePS::Exists());

  PSAutoLock lock(gPSMutex);

  // When a Browsing context is first loaded, the first url loaded in it will be
  // about:blank. Because of that, this call keeps the first non-about:blank
  // registration of window and discards the previous one.
  RefPtr<PageInformation> pageInfo = new PageInformation(
      aBrowsingContextID, aInnerWindowID, aUrl, aEmbedderInnerWindowID);
  CorePS::AppendRegisteredPage(lock, std::move(pageInfo));

  // After appending the given page to CorePS, look for the expired
  // pages and remove them if there are any.
  if (ActivePS::Exists(lock)) {
    ActivePS::DiscardExpiredPages(lock);
  }
}

void profiler_unregister_page(uint64_t aRegisteredInnerWindowID) {
  PSAutoLock lock(gPSMutex);

  if (!CorePS::Exists()) {
    // This function can be called after the main thread has already shut down.
    return;
  }

  // During unregistration, if the profiler is active, we have to keep the
  // page information since there may be some markers associated with the given
  // page. But if profiler is not active. we have no reason to keep the
  // page information here because there can't be any marker associated with it.
  if (ActivePS::Exists(lock)) {
    ActivePS::UnregisterPage(lock, aRegisteredInnerWindowID);
  } else {
    CorePS::RemoveRegisteredPage(lock, aRegisteredInnerWindowID);
  }
}

void profiler_clear_all_pages() {
  {
    PSAutoLock lock(gPSMutex);

    if (!CorePS::Exists()) {
      // This function can be called after the main thread has already shut
      // down.
      return;
    }

    CorePS::ClearRegisteredPages(lock);
    if (ActivePS::Exists(lock)) {
      ActivePS::ClearUnregisteredPages(lock);
    }
  }

  // gPSMutex must be unlocked when we notify, to avoid potential deadlocks.
  ProfilerParent::ClearAllPages();
}

Maybe<uint64_t> profiler_get_inner_window_id_from_docshell(
    nsIDocShell* aDocshell) {
  Maybe<uint64_t> innerWindowID = Nothing();
  if (aDocshell) {
    auto outerWindow = aDocshell->GetWindow();
    if (outerWindow) {
      auto innerWindow = outerWindow->GetCurrentInnerWindow();
      if (innerWindow) {
        innerWindowID = Some(innerWindow->WindowID());
      }
    }
  }
  return innerWindowID;
}

void profiler_thread_sleep() {
  // This function runs both on and off the main thread.

  MOZ_RELEASE_ASSERT(CorePS::Exists());

  RacyRegisteredThread* racyRegisteredThread =
      TLSRegisteredThread::RacyRegisteredThread();
  if (!racyRegisteredThread) {
    return;
  }

  racyRegisteredThread->SetSleeping();
}

void profiler_thread_wake() {
  // This function runs both on and off the main thread.

  MOZ_RELEASE_ASSERT(CorePS::Exists());

  RacyRegisteredThread* racyRegisteredThread =
      TLSRegisteredThread::RacyRegisteredThread();
  if (!racyRegisteredThread) {
    return;
  }

  racyRegisteredThread->SetAwake();
}

bool mozilla::profiler::detail::IsThreadBeingProfiled() {
  MOZ_RELEASE_ASSERT(CorePS::Exists());

  const RacyRegisteredThread* racyRegisteredThread =
      TLSRegisteredThread::RacyRegisteredThread();
  return racyRegisteredThread && racyRegisteredThread->IsBeingProfiled();
}

bool profiler_thread_is_sleeping() {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  MOZ_RELEASE_ASSERT(CorePS::Exists());

  RacyRegisteredThread* racyRegisteredThread =
      TLSRegisteredThread::RacyRegisteredThread();
  if (!racyRegisteredThread) {
    return false;
  }
  return racyRegisteredThread->IsSleeping();
}

void profiler_js_interrupt_callback() {
  // This function runs on JS threads being sampled.
  PollJSSamplingForCurrentThread();
}

double profiler_time() {
  MOZ_RELEASE_ASSERT(CorePS::Exists());

  TimeDuration delta = TimeStamp::NowUnfuzzed() - CorePS::ProcessStartTime();
  return delta.ToMilliseconds();
}

UniqueProfilerBacktrace profiler_get_backtrace() {
  MOZ_RELEASE_ASSERT(CorePS::Exists());

  // Fast racy early return.
  if (!profiler_is_active()) {
    return nullptr;
  }

  PSAutoLock lock(gPSMutex);

  if (!ActivePS::Exists(lock) || ActivePS::FeaturePrivacy(lock)) {
    return nullptr;
  }

  RegisteredThread* registeredThread =
      TLSRegisteredThread::RegisteredThread(lock);
  if (!registeredThread) {
    // If this was called from a non-registered thread, return a nullptr
    // and do no more work. This can happen from a memory hook. Before
    // the allocation tracking there was a MOZ_ASSERT() here checking
    // for the existence of a registeredThread.
    return nullptr;
  }

  int tid = profiler_current_thread_id();

  TimeStamp now = TimeStamp::NowUnfuzzed();

  Registers regs;
#if defined(HAVE_NATIVE_UNWIND)
  regs.SyncPopulate();
#else
  regs.Clear();
#endif

  // 65536 bytes should be plenty for a single backtrace.
  auto bufferManager = MakeUnique<BlocksRingBuffer>(
      BlocksRingBuffer::ThreadSafety::WithoutMutex);
  auto buffer =
      MakeUnique<ProfileBuffer>(*bufferManager, MakePowerOfTwo32<65536>());

  DoSyncSample(lock, *registeredThread, now, regs, *buffer.get());

  return UniqueProfilerBacktrace(new ProfilerBacktrace(
      "SyncProfile", tid, std::move(bufferManager), std::move(buffer)));
}

void ProfilerBacktraceDestructor::operator()(ProfilerBacktrace* aBacktrace) {
  delete aBacktrace;
}

static void racy_profiler_add_marker(const char* aMarkerName,
                                     JS::ProfilingCategoryPair aCategoryPair,
                                     const ProfilerMarkerPayload* aPayload) {
  MOZ_RELEASE_ASSERT(CorePS::Exists());

  // This function is hot enough that we use RacyFeatures, not ActivePS.
  if (!profiler_can_accept_markers()) {
    return;
  }

  // Note that it's possible that the above test would change again before we
  // actually record the marker. Because of this imprecision it's possible to
  // miss a marker or record one we shouldn't. Either way is not a big deal.

  RacyRegisteredThread* racyRegisteredThread =
      TLSRegisteredThread::RacyRegisteredThread();
  if (!racyRegisteredThread || !racyRegisteredThread->IsBeingProfiled()) {
    return;
  }

  TimeStamp origin = (aPayload && !aPayload->GetStartTime().IsNull())
                         ? aPayload->GetStartTime()
                         : TimeStamp::NowUnfuzzed();
  TimeDuration delta = origin - CorePS::ProcessStartTime();
  CorePS::CoreBlocksRingBuffer().PutObjects(
      ProfileBufferEntry::Kind::MarkerData, racyRegisteredThread->ThreadId(),
      WrapProfileBufferUnownedCString(aMarkerName),
      static_cast<uint32_t>(aCategoryPair), aPayload, delta.ToMilliseconds());
}

void profiler_add_marker(const char* aMarkerName,
                         JS::ProfilingCategoryPair aCategoryPair,
                         const ProfilerMarkerPayload& aPayload) {
  racy_profiler_add_marker(aMarkerName, aCategoryPair, &aPayload);
}

void profiler_add_marker(const char* aMarkerName,
                         JS::ProfilingCategoryPair aCategoryPair) {
  racy_profiler_add_marker(aMarkerName, aCategoryPair, nullptr);
}

// This is a simplified version of profiler_add_marker that can be easily passed
// into the JS engine.
void profiler_add_js_marker(const char* aMarkerName) {
  AUTO_PROFILER_STATS(add_marker);
  profiler_add_marker(aMarkerName, JS::ProfilingCategoryPair::JS);
}

void profiler_add_js_allocation_marker(JS::RecordAllocationInfo&& info) {
  if (!profiler_can_accept_markers()) {
    return;
  }
  AUTO_PROFILER_STATS(add_marker_with_JsAllocationMarkerPayload);
  profiler_add_marker(
      "JS allocation", JS::ProfilingCategoryPair::JS,
      JsAllocationMarkerPayload(TimeStamp::Now(), std::move(info),
                                profiler_get_backtrace()));
}

bool profiler_is_locked_on_current_thread() {
  return gPSMutex.IsLockedOnCurrentThread();
}

bool profiler_add_native_allocation_marker(int aMainThreadId, int64_t aSize,
                                           uintptr_t aMemoryAddress) {
  if (!profiler_can_accept_markers()) {
    return false;
  }
  AUTO_PROFILER_STATS(add_marker_with_NativeAllocationMarkerPayload);
  profiler_add_marker_for_thread(
      aMainThreadId, JS::ProfilingCategoryPair::OTHER, "Native allocation",
      MakeUnique<NativeAllocationMarkerPayload>(
          TimeStamp::Now(), aSize, aMemoryAddress, profiler_current_thread_id(),
          profiler_get_backtrace()));
  return true;
}

void profiler_add_network_marker(
    nsIURI* aURI, int32_t aPriority, uint64_t aChannelId, NetworkLoadType aType,
    mozilla::TimeStamp aStart, mozilla::TimeStamp aEnd, int64_t aCount,
    mozilla::net::CacheDisposition aCacheDisposition, uint64_t aInnerWindowID,
    const mozilla::net::TimingStruct* aTimings, nsIURI* aRedirectURI,
    UniqueProfilerBacktrace aSource) {
  if (!profiler_can_accept_markers()) {
    return;
  }
  // These do allocations/frees/etc; avoid if not active
  nsAutoCString spec;
  nsAutoCString redirect_spec;
  if (aURI) {
    aURI->GetAsciiSpec(spec);
  }
  if (aRedirectURI) {
    aRedirectURI->GetAsciiSpec(redirect_spec);
  }
  // top 32 bits are process id of the load
  uint32_t id = static_cast<uint32_t>(aChannelId & 0xFFFFFFFF);
  char name[2048];
  SprintfLiteral(name, "Load %d: %s", id, PromiseFlatCString(spec).get());
  AUTO_PROFILER_STATS(add_marker_with_NetworkMarkerPayload);
  profiler_add_marker(
      name, JS::ProfilingCategoryPair::NETWORK,
      NetworkMarkerPayload(static_cast<int64_t>(aChannelId),
                           PromiseFlatCString(spec).get(), aType, aStart, aEnd,
                           aPriority, aCount, aCacheDisposition, aInnerWindowID,
                           aTimings, PromiseFlatCString(redirect_spec).get(),
                           std::move(aSource)));
}

void profiler_add_marker_for_thread(int aThreadId,
                                    JS::ProfilingCategoryPair aCategoryPair,
                                    const char* aMarkerName,
                                    UniquePtr<ProfilerMarkerPayload> aPayload) {
  MOZ_RELEASE_ASSERT(CorePS::Exists());

  if (!profiler_can_accept_markers()) {
    return;
  }

#ifdef DEBUG
  {
    PSAutoLock lock(gPSMutex);
    if (!ActivePS::Exists(lock)) {
      return;
    }

    // Assert that our thread ID makes sense
    bool realThread = false;
    const Vector<UniquePtr<RegisteredThread>>& registeredThreads =
        CorePS::RegisteredThreads(lock);
    for (auto& thread : registeredThreads) {
      RefPtr<ThreadInfo> info = thread->Info();
      if (info->ThreadId() == aThreadId) {
        realThread = true;
        break;
      }
    }
    MOZ_ASSERT(realThread, "Invalid thread id");
  }
#endif

  TimeStamp origin = (aPayload && !aPayload->GetStartTime().IsNull())
                         ? aPayload->GetStartTime()
                         : TimeStamp::NowUnfuzzed();
  TimeDuration delta = origin - CorePS::ProcessStartTime();
  CorePS::CoreBlocksRingBuffer().PutObjects(
      ProfileBufferEntry::Kind::MarkerData, aThreadId,
      WrapProfileBufferUnownedCString(aMarkerName),
      static_cast<uint32_t>(aCategoryPair), aPayload, delta.ToMilliseconds());
}

void profiler_tracing_marker(const char* aCategoryString,
                             const char* aMarkerName,
                             JS::ProfilingCategoryPair aCategoryPair,
                             TracingKind aKind,
                             const Maybe<uint64_t>& aInnerWindowID) {
  MOZ_RELEASE_ASSERT(CorePS::Exists());

  VTUNE_TRACING(aMarkerName, aKind);

  // This function is hot enough that we use RacyFeatures, notActivePS.
  if (!profiler_can_accept_markers()) {
    return;
  }

  AUTO_PROFILER_STATS(add_marker_with_TracingMarkerPayload);
  profiler_add_marker(
      aMarkerName, aCategoryPair,
      TracingMarkerPayload(aCategoryString, aKind, aInnerWindowID));
}

void profiler_tracing_marker(const char* aCategoryString,
                             const char* aMarkerName,
                             JS::ProfilingCategoryPair aCategoryPair,
                             TracingKind aKind, UniqueProfilerBacktrace aCause,
                             const Maybe<uint64_t>& aInnerWindowID) {
  MOZ_RELEASE_ASSERT(CorePS::Exists());

  VTUNE_TRACING(aMarkerName, aKind);

  // This function is hot enough that we use RacyFeatures, notActivePS.
  if (!profiler_can_accept_markers()) {
    return;
  }

  profiler_add_marker(aMarkerName, aCategoryPair,
                      TracingMarkerPayload(aCategoryString, aKind,
                                           aInnerWindowID, std::move(aCause)));
}

void profiler_add_text_marker(const char* aMarkerName, const nsACString& aText,
                              JS::ProfilingCategoryPair aCategoryPair,
                              const mozilla::TimeStamp& aStartTime,
                              const mozilla::TimeStamp& aEndTime,
                              const mozilla::Maybe<uint64_t>& aInnerWindowID,
                              UniqueProfilerBacktrace aCause) {
  AUTO_PROFILER_STATS(add_marker_with_TextMarkerPayload);
  profiler_add_marker(aMarkerName, aCategoryPair,
                      TextMarkerPayload(aText, aStartTime, aEndTime,
                                        aInnerWindowID, std::move(aCause)));
}

void profiler_set_js_context(JSContext* aCx) {
  MOZ_ASSERT(aCx);

  PSAutoLock lock(gPSMutex);

  RegisteredThread* registeredThread =
      TLSRegisteredThread::RegisteredThread(lock);
  if (!registeredThread) {
    return;
  }

  registeredThread->SetJSContext(aCx);

  // This call is on-thread, so we can call PollJSSampling() to start JS
  // sampling immediately.
  registeredThread->PollJSSampling();

  if (ActivePS::Exists(lock)) {
    ProfiledThreadData* profiledThreadData =
        ActivePS::GetProfiledThreadData(lock, registeredThread);
    if (profiledThreadData) {
      profiledThreadData->NotifyReceivedJSContext(
          ActivePS::Buffer(lock).BufferRangeEnd());
    }
  }
}

void profiler_clear_js_context() {
  MOZ_RELEASE_ASSERT(CorePS::Exists());

  PSAutoLock lock(gPSMutex);

  RegisteredThread* registeredThread =
      TLSRegisteredThread::RegisteredThread(lock);
  if (!registeredThread) {
    return;
  }

  JSContext* cx = registeredThread->GetJSContext();
  if (!cx) {
    return;
  }

  if (ActivePS::Exists(lock) && ActivePS::FeatureJS(lock)) {
    ProfiledThreadData* profiledThreadData =
        ActivePS::GetProfiledThreadData(lock, registeredThread);
    if (profiledThreadData) {
      profiledThreadData->NotifyAboutToLoseJSContext(
          cx, CorePS::ProcessStartTime(), ActivePS::Buffer(lock));

      // Notify the JS context that profiling for this context has stopped.
      // Do this by calling StopJSSampling and PollJSSampling before
      // nulling out the JSContext.
      registeredThread->StopJSSampling();
      registeredThread->PollJSSampling();

      registeredThread->ClearJSContext();

      // Tell the thread that we'd like to have JS sampling on this
      // thread again, once it gets a new JSContext (if ever).
      registeredThread->StartJSSampling(ActivePS::JSFlags(lock));
      return;
    }
  }

  registeredThread->ClearJSContext();
}

// NOTE: aCollector's methods will be called while the target thread is paused.
// Doing things in those methods like allocating -- which may try to claim
// locks -- is a surefire way to deadlock.
void profiler_suspend_and_sample_thread(int aThreadId, uint32_t aFeatures,
                                        ProfilerStackCollector& aCollector,
                                        bool aSampleNative /* = true */) {
  // Lock the profiler mutex
  PSAutoLock lock(gPSMutex);

  const Vector<UniquePtr<RegisteredThread>>& registeredThreads =
      CorePS::RegisteredThreads(lock);
  for (auto& thread : registeredThreads) {
    RefPtr<ThreadInfo> info = thread->Info();
    RegisteredThread& registeredThread = *thread.get();

    if (info->ThreadId() == aThreadId) {
      if (info->IsMainThread()) {
        aCollector.SetIsMainThread();
      }

      // Allocate the space for the native stack
      NativeStack nativeStack;

      // Suspend, sample, and then resume the target thread.
      Sampler sampler(lock);
      TimeStamp now = TimeStamp::Now();
      sampler.SuspendAndSampleAndResumeThread(
          lock, registeredThread, now,
          [&](const Registers& aRegs, const TimeStamp& aNow) {
            // The target thread is now suspended. Collect a native backtrace,
            // and call the callback.
            bool isSynchronous = false;
#if defined(HAVE_FASTINIT_NATIVE_UNWIND)
            if (aSampleNative) {
          // We can only use FramePointerStackWalk or MozStackWalk from
          // suspend_and_sample_thread as other stackwalking methods may not be
          // initialized.
#  if defined(USE_FRAME_POINTER_STACK_WALK)
              DoFramePointerBacktrace(lock, registeredThread, aRegs,
                                      nativeStack);
#  elif defined(USE_MOZ_STACK_WALK)
              DoMozStackWalkBacktrace(lock, registeredThread, aRegs,
                                      nativeStack);
#  else
#    error "Invalid configuration"
#  endif

              MergeStacks(aFeatures, isSynchronous, registeredThread, aRegs,
                          nativeStack, aCollector, CorePS::JsFrames(lock));
            } else
#endif
            {
              MergeStacks(aFeatures, isSynchronous, registeredThread, aRegs,
                          nativeStack, aCollector, CorePS::JsFrames(lock));

              if (ProfilerFeature::HasLeaf(aFeatures)) {
                aCollector.CollectNativeLeafAddr((void*)aRegs.mPC);
              }
            }
          });

      // NOTE: Make sure to disable the sampler before it is destroyed, in case
      // the profiler is running at the same time.
      sampler.Disable(lock);
      break;
    }
  }
}

// END externally visible functions
////////////////////////////////////////////////////////////////////////
