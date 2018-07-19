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

#include <algorithm>
#include <ostream>
#include <fstream>
#include <sstream>
#include <errno.h>

#include "platform.h"
#include "PlatformMacros.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/Atomics.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Vector.h"
#include "GeckoProfiler.h"
#include "VTuneProfiler.h"
#include "GeckoProfilerReporter.h"
#include "ProfilerIOInterposeObserver.h"
#include "mozilla/AutoProfilerLabel.h"
#include "mozilla/ExtensionPolicyService.h"
#include "mozilla/Scheduler.h"
#include "mozilla/StackWalk.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/SystemGroup.h"
#include "mozilla/ThreadLocal.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/Tuple.h"
#include "mozilla/extensions/WebExtensionPolicy.h"
#include "ThreadInfo.h"
#include "nsIHttpProtocolHandler.h"
#include "nsIObserverService.h"
#include "nsIPropertyBag2.h"
#include "nsIXULAppInfo.h"
#include "nsIXULRuntime.h"
#include "nsDirectoryServiceUtils.h"
#include "nsDirectoryServiceDefs.h"
#include "nsJSPrincipals.h"
#include "nsMemoryReporterManager.h"
#include "nsScriptSecurityManager.h"
#include "nsXULAppAPI.h"
#include "nsProfilerStartParams.h"
#include "ProfilerParent.h"
#include "mozilla/Services.h"
#include "nsThreadUtils.h"
#include "ProfilerMarkerPayload.h"
#include "shared-libraries.h"
#include "prdtoa.h"
#include "prtime.h"

#if defined(XP_WIN)
#include <processthreadsapi.h>  // for GetCurrentProcessId()
#else
#include <unistd.h> // for getpid()
#endif // defined(XP_WIN)

#ifdef MOZ_TASK_TRACER
#include "GeckoTaskTracer.h"
#endif

#if defined(GP_OS_android)
# include "FennecJNINatives.h"
# include "FennecJNIWrappers.h"
#endif

// Win32 builds always have frame pointers, so FramePointerStackWalk() always
// works.
#if defined(GP_PLAT_x86_windows)
# define HAVE_NATIVE_UNWIND
# define USE_FRAME_POINTER_STACK_WALK
#endif

// Win64 builds always omit frame pointers, so we use the slower
// MozStackWalk(), which works in that case.
#if defined(GP_PLAT_amd64_windows)
# define HAVE_NATIVE_UNWIND
# define USE_MOZ_STACK_WALK
#endif

// Mac builds only have frame pointers when MOZ_PROFILING is specified, so
// FramePointerStackWalk() only works in that case. We don't use MozStackWalk()
// on Mac.
#if defined(GP_OS_darwin) && defined(MOZ_PROFILING)
# define HAVE_NATIVE_UNWIND
# define USE_FRAME_POINTER_STACK_WALK
#endif

// Android builds use the ARM Exception Handling ABI to unwind.
#if defined(GP_PLAT_arm_linux) || defined(GP_PLAT_arm_android)
# define HAVE_NATIVE_UNWIND
# define USE_EHABI_STACKWALK
# include "EHABIStackWalk.h"
#endif

// Linux builds use LUL, which uses DWARF info to unwind stacks.
#if defined(GP_PLAT_amd64_linux) || \
    defined(GP_PLAT_x86_linux) || defined(GP_PLAT_x86_android) || \
    defined(GP_PLAT_mips64_linux) || \
    defined(GP_PLAT_arm64_linux) || defined(GP_PLAT_arm64_android)
# define HAVE_NATIVE_UNWIND
# define USE_LUL_STACKWALK
# include "lul/LulMain.h"
# include "lul/platform-linux-lul.h"

// On linux we use LUL for periodic samples and synchronous samples, but we use
// FramePointerStackWalk for backtrace samples when MOZ_PROFILING is enabled.
// (See the comment at the top of the file for a definition of
// periodic/synchronous/backtrace.).
//
// FramePointerStackWalk can produce incomplete stacks when the current entry is
// in a shared library without framepointers, however LUL can take a long time
// to initialize, which is undesirable for consumers of
// profiler_suspend_and_sample_thread like the Background Hang Reporter.
# if defined(MOZ_PROFILING)
#  define USE_FRAME_POINTER_STACK_WALK
# endif
#endif

// We can only stackwalk without expensive initialization on platforms which
// support FramePointerStackWalk or MozStackWalk. LUL Stackwalking requires
// initializing LUL, and EHABIStackWalk requires initializing EHABI, both of
// which can be expensive.
#if defined(USE_FRAME_POINTER_STACK_WALK) || defined(USE_MOZ_STACK_WALK)
# define HAVE_FASTINIT_NATIVE_UNWIND
#endif

#ifdef MOZ_VALGRIND
# include <valgrind/memcheck.h>
#else
# define VALGRIND_MAKE_MEM_DEFINED(_addr,_len)   ((void)0)
#endif

#if defined(GP_OS_linux) || defined(GP_OS_android)
#include <ucontext.h>
#endif

using namespace mozilla;
using mozilla::profiler::detail::RacyFeatures;

LazyLogModule gProfilerLog("prof");

#if defined(GP_OS_android)
class GeckoJavaSampler : public java::GeckoJavaSampler::Natives<GeckoJavaSampler>
{
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

class PSMutex : public StaticMutex {};

typedef BaseAutoLock<PSMutex> PSAutoLock;

// Only functions that take a PSLockRef arg can access CorePS's and ActivePS's
// fields.
typedef const PSAutoLock& PSLockRef;

#define PS_GET(type_, name_) \
  static type_ name_(PSLockRef) { return sInstance->m##name_; } \

#define PS_GET_LOCKLESS(type_, name_) \
  static type_ name_() { return sInstance->m##name_; } \

#define PS_GET_AND_SET(type_, name_) \
  PS_GET(type_, name_) \
  static void Set##name_(PSLockRef, type_ a##name_) \
    { sInstance->m##name_ = a##name_; }

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
class CorePS
{
private:
  CorePS()
    : mProcessStartTime(TimeStamp::ProcessCreation())
#ifdef USE_LUL_STACKWALK
    , mLul(nullptr)
#endif
  {}

  ~CorePS()
  {
  }

public:
  static void Create(PSLockRef aLock) { sInstance = new CorePS(); }

  static void Destroy(PSLockRef aLock)
  {
    delete sInstance;
    sInstance = nullptr;
  }

  // Unlike ActivePS::Exists(), CorePS::Exists() can be called without gPSMutex
  // being locked. This is because CorePS is instantiated so early on the main
  // thread that we don't have to worry about it being racy.
  static bool Exists() { return !!sInstance; }

  static void AddSizeOf(PSLockRef, MallocSizeOf aMallocSizeOf,
                        size_t& aProfSize, size_t& aLulSize)
  {
    aProfSize += aMallocSizeOf(sInstance);

    for (auto& registeredThread : sInstance->mRegisteredThreads) {
      aProfSize += registeredThread->SizeOfIncludingThis(aMallocSizeOf);
    }

    // Measurement of the following things may be added later if DMD finds it
    // is worthwhile:
    // - CorePS::mRegisteredThreads itself (its elements' children are measured
    //   above)
    // - CorePS::mInterposeObserver

#if defined(USE_LUL_STACKWALK)
    if (sInstance->mLul) {
      aLulSize += sInstance->mLul->SizeOfIncludingThis(aMallocSizeOf);
    }
#endif
  }

  // No PSLockRef is needed for this field because it's immutable.
  PS_GET_LOCKLESS(TimeStamp, ProcessStartTime)

  PS_GET(const nsTArray<UniquePtr<RegisteredThread>>&, RegisteredThreads)

  static void AppendRegisteredThread(PSLockRef, UniquePtr<RegisteredThread>&& aRegisteredThread)
  {
    sInstance->mRegisteredThreads.AppendElement(std::move(aRegisteredThread));
  }

  static void RemoveRegisteredThread(PSLockRef, RegisteredThread* aRegisteredThread)
  {
    // Remove aRegisteredThread from mRegisteredThreads.
    // Can't use RemoveElement() because we can't equality-compare a UniquePtr
    // to a raw pointer.
    sInstance->mRegisteredThreads.RemoveElementsBy(
      [&](UniquePtr<RegisteredThread>& rt) { return rt.get() == aRegisteredThread; });
  }

#ifdef USE_LUL_STACKWALK
  static lul::LUL* Lul(PSLockRef) { return sInstance->mLul.get(); }
  static void SetLul(PSLockRef, UniquePtr<lul::LUL> aLul)
  {
    sInstance->mLul = std::move(aLul);
  }
#endif

private:
  // The singleton instance
  static CorePS* sInstance;

  // The time that the process started.
  const TimeStamp mProcessStartTime;

  // Info on all the registered threads.
  // ThreadIds in mRegisteredThreads are unique.
  nsTArray<UniquePtr<RegisteredThread>> mRegisteredThreads;

#ifdef USE_LUL_STACKWALK
  // LUL's state. Null prior to the first activation, non-null thereafter.
  UniquePtr<lul::LUL> mLul;
#endif
};

CorePS* CorePS::sInstance = nullptr;

class SamplerThread;

static SamplerThread*
NewSamplerThread(PSLockRef aLock, uint32_t aGeneration, double aInterval);

struct LiveProfiledThreadData
{
  RegisteredThread* mRegisteredThread;
  UniquePtr<ProfiledThreadData> mProfiledThreadData;
};

// This class contains the profiler's global state that is valid only when the
// profiler is active. When not instantiated, the profiler is inactive.
//
// Accesses to ActivePS are guarded by gPSMutex, in much the same fashion as
// CorePS.
//
class ActivePS
{
private:
  static uint32_t AdjustFeatures(uint32_t aFeatures, uint32_t aFilterCount)
  {
    // Filter out any features unavailable in this platform/configuration.
    aFeatures &= profiler_get_available_features();

#if defined(GP_OS_android)
    if (!jni::IsFennec()) {
      aFeatures &= ~ProfilerFeature::Java;
    }
#endif

    // Always enable ProfilerFeature::Threads if we have a filter, because
    // users sometimes ask to filter by a list of threads but forget to
    // explicitly specify ProfilerFeature::Threads.
    if (aFilterCount > 0) {
      aFeatures |= ProfilerFeature::Threads;
    }

    return aFeatures;
  }

  ActivePS(PSLockRef aLock, uint32_t aEntries, double aInterval,
           uint32_t aFeatures, const char** aFilters, uint32_t aFilterCount)
    : mGeneration(sNextGeneration++)
    , mEntries(aEntries)
    , mInterval(aInterval)
    , mFeatures(AdjustFeatures(aFeatures, aFilterCount))
    , mBuffer(MakeUnique<ProfileBuffer>(aEntries))
      // The new sampler thread doesn't start sampling immediately because the
      // main loop within Run() is blocked until this function's caller unlocks
      // gPSMutex.
    , mSamplerThread(NewSamplerThread(aLock, mGeneration, aInterval))
    , mInterposeObserver(ProfilerFeature::HasMainThreadIO(aFeatures)
                         ? new ProfilerIOInterposeObserver()
                         : nullptr)
#undef HAS_FEATURE
    , mIsPaused(false)
#if defined(GP_OS_linux)
    , mWasPaused(false)
#endif
  {
    // Deep copy aFilters.
    MOZ_ALWAYS_TRUE(mFilters.resize(aFilterCount));
    for (uint32_t i = 0; i < aFilterCount; ++i) {
      mFilters[i] = aFilters[i];
    }

    if (mInterposeObserver) {
      // We need to register the observer on the main thread, because we want
      // to observe IO that happens on the main thread.
      if (NS_IsMainThread()) {
        IOInterposer::Register(IOInterposeObserver::OpAll, mInterposeObserver);
      } else {
        RefPtr<ProfilerIOInterposeObserver> observer = mInterposeObserver;
        NS_DispatchToMainThread(
          NS_NewRunnableFunction("ActivePS::ActivePS", [=]() {
            IOInterposer::Register(IOInterposeObserver::OpAll, observer);
          }));
      }
    }
  }

  ~ActivePS()
  {
    if (mInterposeObserver) {
      // We need to unregister the observer on the main thread, because that's
      // where we've registered it.
      if (NS_IsMainThread()) {
        IOInterposer::Unregister(IOInterposeObserver::OpAll, mInterposeObserver);
      } else {
        RefPtr<ProfilerIOInterposeObserver> observer = mInterposeObserver;
        NS_DispatchToMainThread(
          NS_NewRunnableFunction("ActivePS::~ActivePS", [=]() {
            IOInterposer::Unregister(IOInterposeObserver::OpAll, observer);
          }));
      }
    }
  }

  bool ThreadSelected(const char* aThreadName)
  {
    MOZ_RELEASE_ASSERT(sInstance);

    if (mFilters.empty()) {
      return true;
    }

    std::string name = aThreadName;
    std::transform(name.begin(), name.end(), name.begin(), ::tolower);

    for (uint32_t i = 0; i < mFilters.length(); ++i) {
      std::string filter = mFilters[i];
      std::transform(filter.begin(), filter.end(), filter.begin(), ::tolower);

      // Crude, non UTF-8 compatible, case insensitive substring search
      if (name.find(filter) != std::string::npos) {
        return true;
      }

      // If the filter starts with pid:, check for a pid match
      if (filter.find("pid:") == 0) {
        std::string mypid = std::to_string(
#ifdef XP_WIN
          GetCurrentProcessId()
#else
          getpid()
#endif
        );
        if (filter.compare(4, std::string::npos, mypid) == 0) {
          return true;
        }
      }
    }

    return false;
  }

public:
  static void Create(PSLockRef aLock, uint32_t aEntries, double aInterval,
                     uint32_t aFeatures,
                     const char** aFilters, uint32_t aFilterCount)
  {
    sInstance = new ActivePS(aLock, aEntries, aInterval, aFeatures,
                             aFilters, aFilterCount);
  }

  static MOZ_MUST_USE SamplerThread* Destroy(PSLockRef aLock)
  {
    auto samplerThread = sInstance->mSamplerThread;
    delete sInstance;
    sInstance = nullptr;

    return samplerThread;
  }

  static bool Exists(PSLockRef) { return !!sInstance; }

  static bool Equals(PSLockRef,
                     uint32_t aEntries, double aInterval, uint32_t aFeatures,
                     const char** aFilters, uint32_t aFilterCount)
  {
    if (sInstance->mEntries != aEntries ||
        sInstance->mInterval != aInterval ||
        sInstance->mFeatures != aFeatures ||
        sInstance->mFilters.length() != aFilterCount) {
      return false;
    }

    for (uint32_t i = 0; i < sInstance->mFilters.length(); ++i) {
      if (strcmp(sInstance->mFilters[i].c_str(), aFilters[i]) != 0) {
        return false;
      }
    }
    return true;
  }

  static size_t SizeOf(PSLockRef, MallocSizeOf aMallocSizeOf)
  {
    size_t n = aMallocSizeOf(sInstance);

    n += sInstance->mBuffer->SizeOfIncludingThis(aMallocSizeOf);

    // Measurement of the following members may be added later if DMD finds it
    // is worthwhile:
    // - mLiveProfiledThreads (both the array itself, and the contents)
    // - mDeadProfiledThreads (both the array itself, and the contents)
    //

    return n;
  }

  static bool ShouldProfileThread(PSLockRef aLock, ThreadInfo* aInfo)
  {
    MOZ_RELEASE_ASSERT(sInstance);

    return ((aInfo->IsMainThread() || FeatureThreads(aLock)) &&
            sInstance->ThreadSelected(aInfo->Name()));
  }

  PS_GET(uint32_t, Generation)

  PS_GET(uint32_t, Entries)

  PS_GET(double, Interval)

  PS_GET(uint32_t, Features)

  #define PS_GET_FEATURE(n_, str_, Name_) \
    static bool Feature##Name_(PSLockRef) \
    { \
      return ProfilerFeature::Has##Name_(sInstance->mFeatures); \
    }

  PROFILER_FOR_EACH_FEATURE(PS_GET_FEATURE)

  #undef PS_GET_FEATURE

  PS_GET(const Vector<std::string>&, Filters)

  static ProfileBuffer& Buffer(PSLockRef) { return *sInstance->mBuffer.get(); }

  static const nsTArray<LiveProfiledThreadData>& LiveProfiledThreads(PSLockRef)
  {
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
  static nsTArray<Pair<RegisteredThread*, ProfiledThreadData*>> ProfiledThreads(PSLockRef)
  {
    nsTArray<Pair<RegisteredThread*, ProfiledThreadData*>> array;
    for (auto& t : sInstance->mLiveProfiledThreads) {
      array.AppendElement(MakePair(t.mRegisteredThread, t.mProfiledThreadData.get()));
    }
    for (auto& t : sInstance->mDeadProfiledThreads) {
      array.AppendElement(MakePair((RegisteredThread*)nullptr, t.get()));
    }

    class ThreadRegisterTimeComparator {
    public:
      bool Equals(const Pair<RegisteredThread*, ProfiledThreadData*>& a,
                  const Pair<RegisteredThread*, ProfiledThreadData*>& b) const
      {
        return a.second()->Info()->RegisterTime() == b.second()->Info()->RegisterTime();
      }

      bool LessThan(const Pair<RegisteredThread*, ProfiledThreadData*>& a,
                    const Pair<RegisteredThread*, ProfiledThreadData*>& b) const
      {
        return a.second()->Info()->RegisterTime() < b.second()->Info()->RegisterTime();
      }
    };
    array.Sort(ThreadRegisterTimeComparator());
    return array;
  }

  // Do a linear search through mLiveProfiledThreads to find the
  // ProfiledThreadData object for a RegisteredThread.
  static ProfiledThreadData* GetProfiledThreadData(PSLockRef,
                                                   RegisteredThread* aRegisteredThread)
  {
    for (size_t i = 0; i < sInstance->mLiveProfiledThreads.Length(); i++) {
      LiveProfiledThreadData& thread = sInstance->mLiveProfiledThreads[i];
      if (thread.mRegisteredThread == aRegisteredThread) {
        return thread.mProfiledThreadData.get();
      }
    }
    return nullptr;
  }

  static ProfiledThreadData*
  AddLiveProfiledThread(PSLockRef, RegisteredThread* aRegisteredThread,
                        UniquePtr<ProfiledThreadData>&& aProfiledThreadData)
  {
    sInstance->mLiveProfiledThreads.AppendElement(
      LiveProfiledThreadData{ aRegisteredThread, std::move(aProfiledThreadData) });

    // Return a weak pointer to the ProfiledThreadData object.
    return sInstance->mLiveProfiledThreads.LastElement().mProfiledThreadData.get();
  }

  static void UnregisterThread(PSLockRef aLockRef, RegisteredThread* aRegisteredThread)
  {
    DiscardExpiredDeadProfiledThreads(aLockRef);

    // Find the right entry in the mLiveProfiledThreads array and remove the
    // element, moving the ProfiledThreadData object for the thread into the
    // mDeadProfiledThreads array.
    // The thread's RegisteredThread object gets destroyed here.
    for (size_t i = 0; i < sInstance->mLiveProfiledThreads.Length(); i++) {
      LiveProfiledThreadData& thread = sInstance->mLiveProfiledThreads[i];
      if (thread.mRegisteredThread == aRegisteredThread) {
        thread.mProfiledThreadData->NotifyUnregistered(sInstance->mBuffer->mRangeEnd);
        sInstance->mDeadProfiledThreads.AppendElement(std::move(thread.mProfiledThreadData));
        sInstance->mLiveProfiledThreads.RemoveElementAt(i);
        return;
      }
    }
  }

  PS_GET_AND_SET(bool, IsPaused)

#if defined(GP_OS_linux)
  PS_GET_AND_SET(bool, WasPaused)
#endif

  static void DiscardExpiredDeadProfiledThreads(PSLockRef)
  {
    uint64_t bufferRangeStart = sInstance->mBuffer->mRangeStart;
    // Discard any dead threads that were unregistered before bufferRangeStart.
    sInstance->mDeadProfiledThreads.RemoveElementsBy(
      [bufferRangeStart](UniquePtr<ProfiledThreadData>& aProfiledThreadData) {
        Maybe<uint64_t> bufferPosition =
          aProfiledThreadData->BufferPositionWhenUnregistered();
        MOZ_RELEASE_ASSERT(bufferPosition, "should have unregistered this thread");
        return *bufferPosition < bufferRangeStart;
      });
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

  // The number of entries in mBuffer.
  const uint32_t mEntries;

  // The interval between samples, measured in milliseconds.
  const double mInterval;

  // The profile features that are enabled.
  const uint32_t mFeatures;

  // Substrings of names of threads we want to profile.
  Vector<std::string> mFilters;

  // The buffer into which all samples are recorded. Always non-null. Always
  // used in conjunction with CorePS::m{Live,Dead}Threads.
  const UniquePtr<ProfileBuffer> mBuffer;

  // ProfiledThreadData objects for any threads that were profiled at any point
  // during this run of the profiler:
  //  - mLiveProfiledThreads contains all threads that are still registered, and
  //  - mDeadProfiledThreads contains all threads that have already been
  //    unregistered but for which there is still data in the profile buffer.
  nsTArray<LiveProfiledThreadData> mLiveProfiledThreads;
  nsTArray<UniquePtr<ProfiledThreadData>> mDeadProfiledThreads;

  // The current sampler thread. This class is not responsible for destroying
  // the SamplerThread object; the Destroy() method returns it so the caller
  // can destroy it.
  SamplerThread* const mSamplerThread;

  // The interposer that records main thread I/O.
  const RefPtr<ProfilerIOInterposeObserver> mInterposeObserver;

  // Is the profiler paused?
  bool mIsPaused;

#if defined(GP_OS_linux)
  // Used to record whether the profiler was paused just before forking. False
  // at all times except just before/after forking.
  bool mWasPaused;
#endif
};

ActivePS* ActivePS::sInstance = nullptr;
uint32_t ActivePS::sNextGeneration = 0;

#undef PS_GET
#undef PS_GET_LOCKLESS
#undef PS_GET_AND_SET

// The mutex that guards accesses to CorePS and ActivePS.
static PSMutex gPSMutex;

Atomic<uint32_t, MemoryOrdering::Relaxed> RacyFeatures::sActiveAndFeatures(0);

// Each live thread has a RegisteredThread, and we store a reference to it in TLS.
// This class encapsulates that TLS.
class TLSRegisteredThread
{
public:
  static bool Init(PSLockRef)
  {
    bool ok1 = sRegisteredThread.init();
    bool ok2 = AutoProfilerLabel::sProfilingStack.init();
    return ok1 && ok2;
  }

  // Get the entire RegisteredThread. Accesses are guarded by gPSMutex.
  static class RegisteredThread* RegisteredThread(PSLockRef)
  {
    return sRegisteredThread.get();
  }

  // Get only the RacyRegisteredThread. Accesses are not guarded by gPSMutex.
  static class RacyRegisteredThread* RacyRegisteredThread()
  {
    class RegisteredThread* registeredThread = sRegisteredThread.get();
    return registeredThread ? &registeredThread->RacyRegisteredThread()
                            : nullptr;
  }

  // Get only the ProfilingStack. Accesses are not guarded by gPSMutex.
  // RacyRegisteredThread() can also be used to get the ProfilingStack, but that
  // is marginally slower because it requires an extra pointer indirection.
  static ProfilingStack* Stack() { return AutoProfilerLabel::sProfilingStack.get(); }

  static void SetRegisteredThread(PSLockRef,
                                  class RegisteredThread* aRegisteredThread)
  {
    sRegisteredThread.set(aRegisteredThread);
    AutoProfilerLabel::sProfilingStack.set(
      aRegisteredThread
        ? &aRegisteredThread->RacyRegisteredThread().ProfilingStack()
        : nullptr);
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
MOZ_THREAD_LOCAL(ProfilingStack*) AutoProfilerLabel::sProfilingStack;

// The name of the main thread.
static const char* const kMainThreadName = "GeckoMain";

////////////////////////////////////////////////////////////////////////
// BEGIN sampling/unwinding code

// The registers used for stack unwinding and a few other sampling purposes.
// The ctor does nothing; users are responsible for filling in the fields.
class Registers
{
public:
  Registers() : mPC{nullptr}, mSP{nullptr}, mFP{nullptr}, mLR{nullptr} {}

#if defined(HAVE_NATIVE_UNWIND)
  // Fills in mPC, mSP, mFP, mLR, and mContext for a synchronous sample.
  void SyncPopulate();
#endif

  void Clear() { memset(this, 0, sizeof(*this)); }

  // These fields are filled in by
  // SamplerThread::SuspendAndSampleAndResumeThread() for periodic and
  // backtrace samples, and by SyncPopulate() for synchronous samples.
  Address mPC;    // Instruction pointer.
  Address mSP;    // Stack pointer.
  Address mFP;    // Frame pointer.
  Address mLR;    // ARM link register.
#if defined(GP_OS_linux) || defined(GP_OS_android)
  // This contains all the registers, which means it duplicates the four fields
  // above. This is ok.
  ucontext_t* mContext; // The context from the signal handler.
#endif
};

// Setting MAX_NATIVE_FRAMES too high risks the unwinder wasting a lot of time
// looping on corrupted stacks.
static const size_t MAX_NATIVE_FRAMES = 1024;
static const size_t MAX_JS_FRAMES     = 1024;

struct NativeStack
{
  void* mPCs[MAX_NATIVE_FRAMES];
  void* mSPs[MAX_NATIVE_FRAMES];
  size_t mCount;  // Number of frames filled.

  NativeStack()
    : mPCs(), mSPs(), mCount(0)
  {}
};

Atomic<bool> WALKING_JS_STACK(false);

struct AutoWalkJSStack
{
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
static void
MergeStacks(uint32_t aFeatures, bool aIsSynchronous,
            const RegisteredThread& aRegisteredThread, const Registers& aRegs,
            const NativeStack& aNativeStack,
            ProfilerStackCollector& aCollector)
{
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
  JS::ProfilingFrameIterator::Frame jsFrames[MAX_JS_FRAMES];

  // Only walk jit stack if profiling frame iterator is turned on.
  if (context && JS::IsProfilingEnabledForContext(context)) {
    AutoWalkJSStack autoWalkJSStack;
    const uint32_t maxFrames = ArrayLength(jsFrames);

    if (autoWalkJSStack.walkAllowed) {
      JS::ProfilingFrameIterator::RegisterState registerState;
      registerState.pc = aRegs.mPC;
      registerState.sp = aRegs.mSP;
      registerState.lr = aRegs.mLR;
      registerState.fp = aRegs.mFP;

      JS::ProfilingFrameIterator jsIter(context, registerState, samplePosInBuffer);
      for (; jsCount < maxFrames && !jsIter.done(); ++jsIter) {
        if (aIsSynchronous || jsIter.isWasm()) {
          uint32_t extracted =
            jsIter.extractStack(jsFrames, jsCount, maxFrames);
          jsCount += extracted;
          if (jsCount == maxFrames) {
            break;
          }
        } else {
          Maybe<JS::ProfilingFrameIterator::Frame> frame =
            jsIter.getPhysicalFrameWithoutLabel();
          if (frame.isSome()) {
            jsFrames[jsCount++] = frame.value();
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
        lastLabelFrameStackAddr = (uint8_t*) profilingStackFrame.stackAddress();
      }

      // Skip any JS_OSR frames. Such frames are used when the JS interpreter
      // enters a jit frame on a loop edge (via on-stack-replacement, or OSR).
      // To avoid both the profiling stack frame and jit frame being recorded
      // (and showing up twice), the interpreter marks the interpreter
      // profiling stack frame as JS_OSR to ensure that it doesn't get counted.
      if (profilingStackFrame.kind() == js::ProfilingStackFrame::Kind::JS_OSR) {
          profilingStackIndex++;
          continue;
      }

      MOZ_ASSERT(lastLabelFrameStackAddr);
      profilingStackAddr = lastLabelFrameStackAddr;
    }

    if (jsIndex >= 0) {
      jsStackAddr = (uint8_t*) jsFrames[jsIndex].stackAddress;
      jsActivationAddr = (uint8_t*) jsFrames[jsIndex].activation;
    }

    if (nativeIndex >= 0) {
      nativeStackAddr = (uint8_t*) aNativeStack.mSPs[nativeIndex];
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
    MOZ_ASSERT_IF(profilingStackAddr, profilingStackAddr != jsStackAddr &&
                                      profilingStackAddr != nativeStackAddr);
    MOZ_ASSERT_IF(jsStackAddr, jsStackAddr != profilingStackAddr &&
                               jsStackAddr != nativeStackAddr);
    MOZ_ASSERT_IF(nativeStackAddr, nativeStackAddr != profilingStackAddr &&
                                   nativeStackAddr != jsStackAddr);

    // Check to see if profiling stack frame is top-most.
    if (profilingStackAddr > jsStackAddr && profilingStackAddr > nativeStackAddr) {
      MOZ_ASSERT(profilingStackIndex < profilingStackFrameCount);
      const js::ProfilingStackFrame& profilingStackFrame =
        profilingStackFrames[profilingStackIndex];

      // Sp marker frames are just annotations and should not be recorded in
      // the profile.
      if (!profilingStackFrame.isSpMarkerFrame()) {
        // The JIT only allows the top-most frame to have a nullptr pc.
        MOZ_ASSERT_IF(profilingStackFrame.isJsFrame() &&
                      profilingStackFrame.script() && !profilingStackFrame.pc(),
                      &profilingStackFrame == &profilingStack.frames[profilingStack.stackSize() - 1]);
        aCollector.CollectProfilingStackFrame(profilingStackFrame);
      }
      profilingStackIndex++;
      continue;
    }

    // Check to see if JS jit stack frame is top-most
    if (jsStackAddr > nativeStackAddr) {
      MOZ_ASSERT(jsIndex >= 0);
      const JS::ProfilingFrameIterator::Frame& jsFrame = jsFrames[jsIndex];
      jitEndStackAddr = (uint8_t*) jsFrame.endStackAddress;
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
      } else {
        MOZ_ASSERT(jsFrame.kind == JS::ProfilingFrameIterator::Frame_Ion ||
                   jsFrame.kind == JS::ProfilingFrameIterator::Frame_Baseline);
        aCollector.CollectJitReturnAddr(jsFrame.returnAddress);
      }

      jsIndex--;
      continue;
    }

    // If we reach here, there must be a native stack frame and it must be the
    // greatest frame.
    if (nativeStackAddr &&
        // If the latest JS frame was JIT, this could be the native frame that
        // corresponds to it. In that case, skip the native frame, because there's
        // no need for the same frame to be present twice in the stack. The JS
        // frame can be considered the symbolicated version of the native frame.
        (!jitEndStackAddr || nativeStackAddr < jitEndStackAddr ) &&
        // This might still be a JIT operation, check to make sure that is not in range
        // of the NEXT JavaScript's stacks' activation address.
        (!jsActivationAddr || nativeStackAddr > jsActivationAddr)
      ) {
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

#if defined(GP_OS_windows)
static HANDLE GetThreadHandle(PlatformData* aData);
#endif

#if defined(USE_FRAME_POINTER_STACK_WALK) || defined(USE_MOZ_STACK_WALK)
static void
StackWalkCallback(uint32_t aFrameNumber, void* aPC, void* aSP, void* aClosure)
{
  NativeStack* nativeStack = static_cast<NativeStack*>(aClosure);
  MOZ_ASSERT(nativeStack->mCount < MAX_NATIVE_FRAMES);
  nativeStack->mSPs[nativeStack->mCount] = aSP;
  nativeStack->mPCs[nativeStack->mCount] = aPC;
  nativeStack->mCount++;
}
#endif

#if defined(USE_FRAME_POINTER_STACK_WALK)
static void
DoFramePointerBacktrace(PSLockRef aLock, const RegisteredThread& aRegisteredThread,
                        const Registers& aRegs, NativeStack& aNativeStack)
{
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
static void
DoMozStackWalkBacktrace(PSLockRef aLock, const RegisteredThread& aRegisteredThread,
                        const Registers& aRegs, NativeStack& aNativeStack)
{
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
static void
DoEHABIBacktrace(PSLockRef aLock, const RegisteredThread& aRegisteredThread,
                 const Registers& aRegs, NativeStack& aNativeStack)
{
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
      savedContext.arm_r4  = *vSP++;
      savedContext.arm_r5  = *vSP++;
      savedContext.arm_r6  = *vSP++;
      savedContext.arm_r7  = *vSP++;
      savedContext.arm_r8  = *vSP++;
      savedContext.arm_r9  = *vSP++;
      savedContext.arm_r10 = *vSP++;
      savedContext.arm_fp  = *vSP++;
      savedContext.arm_lr  = *vSP++;
      savedContext.arm_sp  = reinterpret_cast<uint32_t>(vSP);
      savedContext.arm_pc  = savedContext.arm_lr;
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
#if defined(MOZ_HAVE_ASAN_BLACKLIST)
MOZ_ASAN_BLACKLIST static void
ASAN_memcpy(void* aDst, const void* aSrc, size_t aLen)
{
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
#endif

static void
DoLULBacktrace(PSLockRef aLock, const RegisteredThread& aRegisteredThread,
               const Registers& aRegs, NativeStack& aNativeStack)
{
  // WARNING: this function runs within the profiler's "critical section".
  // WARNING: this function might be called while the profiler is inactive, and
  //          cannot rely on ActivePS.

  const mcontext_t* mc = &aRegs.mContext->uc_mcontext;

  lul::UnwindRegs startRegs;
  memset(&startRegs, 0, sizeof(startRegs));

#if defined(GP_PLAT_amd64_linux)
  startRegs.xip = lul::TaggedUWord(mc->gregs[REG_RIP]);
  startRegs.xsp = lul::TaggedUWord(mc->gregs[REG_RSP]);
  startRegs.xbp = lul::TaggedUWord(mc->gregs[REG_RBP]);
#elif defined(GP_PLAT_arm_linux) || defined(GP_PLAT_arm_android)
  startRegs.r15 = lul::TaggedUWord(mc->arm_pc);
  startRegs.r14 = lul::TaggedUWord(mc->arm_lr);
  startRegs.r13 = lul::TaggedUWord(mc->arm_sp);
  startRegs.r12 = lul::TaggedUWord(mc->arm_ip);
  startRegs.r11 = lul::TaggedUWord(mc->arm_fp);
  startRegs.r7  = lul::TaggedUWord(mc->arm_r7);
#elif defined(GP_PLAT_arm64_linux) || defined(GP_PLAT_arm64_android)
  startRegs.pc  = lul::TaggedUWord(mc->pc);
  startRegs.x29 = lul::TaggedUWord(mc->regs[29]);
  startRegs.x30 = lul::TaggedUWord(mc->regs[30]);
  startRegs.sp  = lul::TaggedUWord(mc->sp);
#elif defined(GP_PLAT_x86_linux) || defined(GP_PLAT_x86_android)
  startRegs.xip = lul::TaggedUWord(mc->gregs[REG_EIP]);
  startRegs.xsp = lul::TaggedUWord(mc->gregs[REG_ESP]);
  startRegs.xbp = lul::TaggedUWord(mc->gregs[REG_EBP]);
#elif defined(GP_PLAT_mips64_linux)
  startRegs.pc = lul::TaggedUWord(mc->pc);
  startRegs.sp = lul::TaggedUWord(mc->gregs[29]);
  startRegs.fp = lul::TaggedUWord(mc->gregs[30]);
#else
# error "Unknown plat"
#endif

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
#if defined(GP_PLAT_amd64_linux)
    uintptr_t rEDZONE_SIZE = 128;
    uintptr_t start = startRegs.xsp.Value() - rEDZONE_SIZE;
#elif defined(GP_PLAT_arm_linux) || defined(GP_PLAT_arm_android)
    uintptr_t rEDZONE_SIZE = 0;
    uintptr_t start = startRegs.r13.Value() - rEDZONE_SIZE;
#elif defined(GP_PLAT_arm64_linux) || defined(GP_PLAT_arm64_android)
    uintptr_t rEDZONE_SIZE = 0;
    uintptr_t start = startRegs.sp.Value() - rEDZONE_SIZE;
#elif defined(GP_PLAT_x86_linux) || defined(GP_PLAT_x86_android)
    uintptr_t rEDZONE_SIZE = 0;
    uintptr_t start = startRegs.xsp.Value() - rEDZONE_SIZE;
#elif defined(GP_PLAT_mips64_linux)
    uintptr_t rEDZONE_SIZE = 0;
    uintptr_t start = startRegs.sp.Value() - rEDZONE_SIZE;
#else
#   error "Unknown plat"
#endif
    uintptr_t end = reinterpret_cast<uintptr_t>(aRegisteredThread.StackTop());
    uintptr_t ws  = sizeof(void*);
    start &= ~(ws-1);
    end   &= ~(ws-1);
    uintptr_t nToCopy = 0;
    if (start < end) {
      nToCopy = end - start;
      if (nToCopy > lul::N_STACK_BYTES)
        nToCopy = lul::N_STACK_BYTES;
    }
    MOZ_ASSERT(nToCopy <= lul::N_STACK_BYTES);
    stackImg.mLen       = nToCopy;
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
#if defined(MOZ_HAVE_ASAN_BLACKLIST)
      ASAN_memcpy(&stackImg.mContents[0], (void*)start, nToCopy);
#else
      memcpy(&stackImg.mContents[0], (void*)start, nToCopy);
#endif
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
  lul->mStats.mCFI     += aNativeStack.mCount - 1 - framePointerFramesAcquired;
  lul->mStats.mFP      += framePointerFramesAcquired;
}

#endif

#ifdef HAVE_NATIVE_UNWIND
static void
DoNativeBacktrace(PSLockRef aLock, const RegisteredThread& aRegisteredThread,
                  const Registers& aRegs, NativeStack& aNativeStack)
{
  // This method determines which stackwalker is used for periodic and
  // synchronous samples. (Backtrace samples are treated differently, see
  // profiler_suspend_and_sample_thread() for details). The only part of the
  // ordering that matters is that LUL must precede FRAME_POINTER, because on
  // Linux they can both be present.
#if defined(USE_LUL_STACKWALK)
  DoLULBacktrace(aLock, aRegisteredThread, aRegs, aNativeStack);
#elif defined(USE_EHABI_STACKWALK)
  DoEHABIBacktrace(aLock, aRegisteredThread, aRegs, aNativeStack);
#elif defined(USE_FRAME_POINTER_STACK_WALK)
  DoFramePointerBacktrace(aLock, aRegisteredThread, aRegs, aNativeStack);
#elif defined(USE_MOZ_STACK_WALK)
  DoMozStackWalkBacktrace(aLock, aRegisteredThread, aRegs, aNativeStack);
#else
  #error "Invalid configuration"
#endif
}
#endif

// Writes some components shared by periodic and synchronous profiles to
// ActivePS's ProfileBuffer. (This should only be called from DoSyncSample()
// and DoPeriodicSample().)
//
// The grammar for entry sequences is in a comment above
// ProfileBuffer::StreamSamplesToJSON.
static inline void
DoSharedSample(PSLockRef aLock, bool aIsSynchronous,
               RegisteredThread& aRegisteredThread, const TimeStamp& aNow,
               const Registers& aRegs, Maybe<uint64_t>* aLastSample,
               ProfileBuffer& aBuffer)
{
  // WARNING: this function runs within the profiler's "critical section".

  MOZ_RELEASE_ASSERT(ActivePS::Exists(aLock));

  uint64_t samplePos = aBuffer.AddThreadIdEntry(aRegisteredThread.Info()->ThreadId());
  if (aLastSample) {
    *aLastSample = Some(samplePos);
  }

  TimeDuration delta = aNow - CorePS::ProcessStartTime();
  aBuffer.AddEntry(ProfileBufferEntry::Time(delta.ToMilliseconds()));

  ProfileBufferCollector collector(aBuffer, ActivePS::Features(aLock),
                                   samplePos);
  NativeStack nativeStack;
#if defined(HAVE_NATIVE_UNWIND)
  if (ActivePS::FeatureStackWalk(aLock)) {
    DoNativeBacktrace(aLock, aRegisteredThread, aRegs, nativeStack);

    MergeStacks(ActivePS::Features(aLock), aIsSynchronous, aRegisteredThread,
                aRegs, nativeStack, collector);
  } else
#endif
  {
    MergeStacks(ActivePS::Features(aLock), aIsSynchronous, aRegisteredThread,
                aRegs, nativeStack, collector);

    // We can't walk the whole native stack, but we can record the top frame.
    if (ActivePS::FeatureLeaf(aLock)) {
      aBuffer.AddEntry(ProfileBufferEntry::NativeLeafAddr((void*)aRegs.mPC));
    }
  }
}

// Writes the components of a synchronous sample to the given ProfileBuffer.
static void
DoSyncSample(PSLockRef aLock, RegisteredThread& aRegisteredThread,
             const TimeStamp& aNow, const Registers& aRegs,
             ProfileBuffer& aBuffer)
{
  // WARNING: this function runs within the profiler's "critical section".

  DoSharedSample(aLock, /* aIsSynchronous = */ true, aRegisteredThread, aNow,
                 aRegs, /* aLastSample = */ nullptr, aBuffer);
}

// Writes the components of a periodic sample to ActivePS's ProfileBuffer.
static void
DoPeriodicSample(PSLockRef aLock, RegisteredThread& aRegisteredThread,
                 ProfiledThreadData& aProfiledThreadData,
                 const TimeStamp& aNow, const Registers& aRegs,
                 int64_t aRSSMemory, int64_t aUSSMemory)
{
  // WARNING: this function runs within the profiler's "critical section".

  ProfileBuffer& buffer = ActivePS::Buffer(aLock);

  DoSharedSample(aLock, /* aIsSynchronous = */ false, aRegisteredThread, aNow,
                 aRegs, &aProfiledThreadData.LastSample(), buffer);

  ProfilerMarkerLinkedList* pendingMarkersList =
    aRegisteredThread.RacyRegisteredThread().GetPendingMarkers();
  while (pendingMarkersList && pendingMarkersList->peek()) {
    ProfilerMarker* marker = pendingMarkersList->popHead();
    buffer.AddStoredMarker(marker);
    buffer.AddEntry(ProfileBufferEntry::Marker(marker));
  }

  ThreadResponsiveness* resp = aProfiledThreadData.GetThreadResponsiveness();
  if (resp && resp->HasData()) {
    double delta = resp->GetUnresponsiveDuration(
      (aNow - CorePS::ProcessStartTime()).ToMilliseconds());
    buffer.AddEntry(ProfileBufferEntry::Responsiveness(delta));
  }

  if (aRSSMemory != 0) {
    double rssMemory = static_cast<double>(aRSSMemory);
    buffer.AddEntry(ProfileBufferEntry::ResidentMemory(rssMemory));
  }

  if (aUSSMemory != 0) {
    double ussMemory = static_cast<double>(aUSSMemory);
    buffer.AddEntry(ProfileBufferEntry::UnsharedMemory(ussMemory));
  }
}

// END sampling/unwinding code
////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////
// BEGIN saving/streaming code

const static uint64_t kJS_MAX_SAFE_UINTEGER = +9007199254740991ULL;

static int64_t
SafeJSInteger(uint64_t aValue) {
  return aValue <= kJS_MAX_SAFE_UINTEGER ? int64_t(aValue) : -1;
}

static void
AddSharedLibraryInfoToStream(JSONWriter& aWriter, const SharedLibrary& aLib)
{
  aWriter.StartObjectElement();
  aWriter.IntProperty("start", SafeJSInteger(aLib.GetStart()));
  aWriter.IntProperty("end", SafeJSInteger(aLib.GetEnd()));
  aWriter.IntProperty("offset", SafeJSInteger(aLib.GetOffset()));
  aWriter.StringProperty("name", NS_ConvertUTF16toUTF8(aLib.GetModuleName()).get());
  aWriter.StringProperty("path", NS_ConvertUTF16toUTF8(aLib.GetModulePath()).get());
  aWriter.StringProperty("debugName", NS_ConvertUTF16toUTF8(aLib.GetDebugName()).get());
  aWriter.StringProperty("debugPath", NS_ConvertUTF16toUTF8(aLib.GetDebugPath()).get());
  aWriter.StringProperty("breakpadId", aLib.GetBreakpadId().get());
  aWriter.StringProperty("arch", aLib.GetArch().c_str());
  aWriter.EndObject();
}

void
AppendSharedLibraries(JSONWriter& aWriter)
{
  SharedLibraryInfo info = SharedLibraryInfo::GetInfoForSelf();
  info.SortByAddress();
  for (size_t i = 0; i < info.GetSize(); i++) {
    AddSharedLibraryInfoToStream(aWriter, info.GetEntry(i));
  }
}

#ifdef MOZ_TASK_TRACER
static void
StreamNameAndThreadId(JSONWriter& aWriter, const char* aName, int aThreadId)
{
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

static void
StreamTaskTracer(PSLockRef aLock, SpliceableJSONWriter& aWriter)
{
#ifdef MOZ_TASK_TRACER
  MOZ_RELEASE_ASSERT(CorePS::Exists() && ActivePS::Exists(aLock));

  aWriter.StartArrayProperty("data");
  {
    UniquePtr<nsTArray<nsCString>> data =
      tasktracer::GetLoggedData(CorePS::ProcessStartTime());
    for (uint32_t i = 0; i < data->Length(); ++i) {
      aWriter.StringElement((data->ElementAt(i)).get());
    }
  }
  aWriter.EndArray();

  aWriter.StartArrayProperty("threads");
  {
    ActivePS::DiscardExpiredDeadProfiledThreads(aLock);
    nsTArray<Pair<RegisteredThread*, ProfiledThreadData*>> threads =
      ActivePS::ProfiledThreads(aLock);
    for (auto& thread : threads) {
      RefPtr<ThreadInfo> info = thread.second()->Info();
      StreamNameAndThreadId(aWriter, info->Name(), info->ThreadId());
    }
  }
  aWriter.EndArray();

  aWriter.DoubleProperty(
    "start", static_cast<double>(tasktracer::GetStartTime()));
#endif
}

static void
StreamCategories(SpliceableJSONWriter& aWriter)
{
  // Same order as ProfilingStackFrame::Category.
  // The list of available color names is:
  // transparent, grey, purple, yellow, orange, lightblue, green, blue, magenta
  aWriter.Start();
  aWriter.StringProperty("name", "Idle");
  aWriter.StringProperty("color", "transparent");
  aWriter.EndObject();
  aWriter.Start();
  aWriter.StringProperty("name", "Other");
  aWriter.StringProperty("color", "grey");
  aWriter.EndObject();
  aWriter.Start();
  aWriter.StringProperty("name", "Layout");
  aWriter.StringProperty("color", "purple");
  aWriter.EndObject();
  aWriter.Start();
  aWriter.StringProperty("name", "JavaScript");
  aWriter.StringProperty("color", "yellow");
  aWriter.EndObject();
  aWriter.Start();
  aWriter.StringProperty("name", "GC / CC");
  aWriter.StringProperty("color", "orange");
  aWriter.EndObject();
  aWriter.Start();
  aWriter.StringProperty("name", "Network");
  aWriter.StringProperty("color", "lightblue");
  aWriter.EndObject();
  aWriter.Start();
  aWriter.StringProperty("name", "Graphics");
  aWriter.StringProperty("color", "green");
  aWriter.EndObject();
  aWriter.Start();
  aWriter.StringProperty("name", "DOM");
  aWriter.StringProperty("color", "blue");
  aWriter.EndObject();
}

static void
StreamMetaJSCustomObject(PSLockRef aLock, SpliceableJSONWriter& aWriter,
                         bool aIsShuttingDown)
{
  MOZ_RELEASE_ASSERT(CorePS::Exists() && ActivePS::Exists(aLock));

  aWriter.IntProperty("version", 11);

  // The "startTime" field holds the number of milliseconds since midnight
  // January 1, 1970 GMT. This grotty code computes (Now - (Now -
  // ProcessStartTime)) to convert CorePS::ProcessStartTime() into that form.
  TimeDuration delta = TimeStamp::Now() - CorePS::ProcessStartTime();
  aWriter.DoubleProperty(
    "startTime", static_cast<double>(PR_Now()/1000.0 - delta.ToMilliseconds()));

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
    if (!NS_FAILED(res))
      aWriter.StringProperty("abi", string.Data());

    res = runtime->GetWidgetToolkit(string);
    if (!NS_FAILED(res))
      aWriter.StringProperty("toolkit", string.Data());
  }

  nsCOMPtr<nsIXULAppInfo> appInfo =
    do_GetService("@mozilla.org/xre/app-info;1");

  if (appInfo) {
    nsAutoCString string;
    res = appInfo->GetName(string);
    if (!NS_FAILED(res))
      aWriter.StringProperty("product", string.Data());

    res = appInfo->GetAppBuildID(string);
    if (!NS_FAILED(res))
      aWriter.StringProperty("appBuildID", string.Data());

    res = appInfo->GetSourceURL(string);
    if (!NS_FAILED(res))
      aWriter.StringProperty("sourceURL", string.Data());
  }

  nsCOMPtr<nsIPropertyBag2> systemInfo =
    do_GetService("@mozilla.org/system-info;1");
  if (systemInfo) {
    int32_t cpus;
    res = systemInfo->GetPropertyAsInt32(NS_LITERAL_STRING("cpucores"), &cpus);
    if (!NS_FAILED(res)) {
      aWriter.IntProperty("physicalCPUs", cpus);
    }
    res = systemInfo->GetPropertyAsInt32(NS_LITERAL_STRING("cpucount"), &cpus);
    if (!NS_FAILED(res)) {
      aWriter.IntProperty("logicalCPUs", cpus);
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
  }
  aWriter.EndObject();
}

#if defined(GP_OS_android)
static UniquePtr<ProfileBuffer>
CollectJavaThreadProfileData()
{
  // locked_profiler_start uses sample count is 1000 for Java thread.
  // This entry size is enough now, but we might have to estimate it
  // if we can customize it
  auto buffer = MakeUnique<ProfileBuffer>(1000 * 1000);

  int sampleId = 0;
  while (true) {
    double sampleTime = java::GeckoJavaSampler::GetSampleTime(0, sampleId);
    if (sampleTime == 0.0) {
      break;
    }

    buffer->AddThreadIdEntry(0);
    buffer->AddEntry(ProfileBufferEntry::Time(sampleTime));
    bool parentFrameWasIdleFrame = false;
    int frameId = 0;
    while (true) {
      jni::String::LocalRef frameName =
        java::GeckoJavaSampler::GetFrameName(0, sampleId, frameId++);
      if (!frameName) {
        break;
      }
      nsCString frameNameString = frameName->ToCString();

      // Compute a category for the frame:
      //  - IDLE for the wait function android.os.MessageQueue.nativePollOnce()
      //  - OTHER for any function that's directly called by that wait function
      //  - no category on everything else
      Maybe<js::ProfilingStackFrame::Category> category;
      if (frameNameString.EqualsLiteral("android.os.MessageQueue.nativePollOnce()")) {
        category = Some(js::ProfilingStackFrame::Category::IDLE);
        parentFrameWasIdleFrame = true;
      } else if (parentFrameWasIdleFrame) {
        category = Some(js::ProfilingStackFrame::Category::OTHER);
        parentFrameWasIdleFrame = false;
      }

      buffer->CollectCodeLocation("", frameNameString.get(), -1, category);
    }
    sampleId++;
  }
  return buffer;
}
#endif

static void
locked_profiler_stream_json_for_this_process(PSLockRef aLock,
                                             SpliceableJSONWriter& aWriter,
                                             double aSinceTime,
                                             bool aIsShuttingDown)
{
  LOG("locked_profiler_stream_json_for_this_process");

  MOZ_RELEASE_ASSERT(CorePS::Exists() && ActivePS::Exists(aLock));

  double collectionStart = profiler_time();

  ProfileBuffer& buffer = ActivePS::Buffer(aLock);

  // Put shared library info
  aWriter.StartArrayProperty("libs");
  AppendSharedLibraries(aWriter);
  aWriter.EndArray();

  // Put meta data
  aWriter.StartObjectProperty("meta");
  {
    StreamMetaJSCustomObject(aLock, aWriter, aIsShuttingDown);
  }
  aWriter.EndObject();

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
    nsTArray<Pair<RegisteredThread*, ProfiledThreadData*>> threads =
      ActivePS::ProfiledThreads(aLock);
    for (auto& thread : threads) {
      RegisteredThread* registeredThread = thread.first();
      JSContext* cx =
        registeredThread ? registeredThread->GetJSContext() : nullptr;
      ProfiledThreadData* profiledThreadData = thread.second();
      profiledThreadData->StreamJSON(buffer, cx, aWriter,
                                     CorePS::ProcessStartTime(), aSinceTime);
    }

#if defined(GP_OS_android)
  if (ActivePS::FeatureJava(aLock)) {
     java::GeckoJavaSampler::Pause();

     UniquePtr<ProfileBuffer> javaBuffer = CollectJavaThreadProfileData();

     // Thread id of java Main thread is 0, if we support profiling of other
     // java thread, we have to get thread id and name via JNI.
     RefPtr<ThreadInfo> threadInfo =
       new ThreadInfo("Java Main Thread", 0, false, CorePS::ProcessStartTime());
     ProfiledThreadData profiledThreadData(threadInfo, nullptr,
                                           ActivePS::FeatureResponsiveness(aLock));
     profiledThreadData.StreamJSON(*javaBuffer.get(), nullptr, aWriter,
                                   CorePS::ProcessStartTime(), aSinceTime);

     java::GeckoJavaSampler::Unpause();
  }
#endif

  }
  aWriter.EndArray();

  aWriter.StartArrayProperty("pausedRanges");
  {
    buffer.StreamPausedRangesToJSON(aWriter, aSinceTime);
  }
  aWriter.EndArray();

  double collectionEnd = profiler_time();

  // Record timestamps for the collection into the buffer, so that consumers
  // know why we didn't collect any samples for its duration.
  // We put these entries into the buffer after we've collected the profile,
  // so they'll be visible for the *next* profile collection (if they haven't
  // been overwritten due to buffer wraparound by then).
  buffer.AddEntry(ProfileBufferEntry::CollectionStart(collectionStart));
  buffer.AddEntry(ProfileBufferEntry::CollectionEnd(collectionEnd));
}

bool
profiler_stream_json_for_this_process(SpliceableJSONWriter& aWriter,
                                      double aSinceTime,
                                      bool aIsShuttingDown)
{
  LOG("profiler_stream_json_for_this_process");

  MOZ_RELEASE_ASSERT(CorePS::Exists());

  PSAutoLock lock(gPSMutex);

  if (!ActivePS::Exists(lock)) {
    return false;
  }

  locked_profiler_stream_json_for_this_process(lock, aWriter, aSinceTime,
                                               aIsShuttingDown);

  return true;
}

// END saving/streaming code
////////////////////////////////////////////////////////////////////////

static void
PrintUsageThenExit(int aExitCode)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  printf(
    "\n"
    "Profiler environment variable usage:\n"
    "\n"
    "  MOZ_PROFILER_HELP\n"
    "  If set to any value, prints this message.\n"
    "\n"
    "  MOZ_LOG\n"
    "  Enables logging. The levels of logging available are\n"
    "  'prof:3' (least verbose), 'prof:4', 'prof:5' (most verbose).\n"
    "\n"
    "  MOZ_PROFILER_STARTUP\n"
    "  If set to any value, starts the profiler immediately on start-up.\n"
    "  Useful if you want profile code that runs very early.\n"
    "\n"
    "  MOZ_PROFILER_STARTUP_ENTRIES=<1..>\n"
    "  If MOZ_PROFILER_STARTUP is set, specifies the number of entries in\n"
    "  the profiler's circular buffer when the profiler is first started.\n"
    "  If unset, the platform default is used.\n"
    "\n"
    "  MOZ_PROFILER_STARTUP_INTERVAL=<1..1000>\n"
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
    "  MOZ_PROFILER_STARTUP_FILTERS=<Filters>\n"
    "  If MOZ_PROFILER_STARTUP is set, specifies the thread filters, as a\n"
    "  comma-separated list of strings. A given thread will be sampled if any\n"
    "  of the filters is a case-insensitive substring of the thread name.\n"
    "  If unset, a default is used.\n"
    "\n"
    "  MOZ_PROFILER_SHUTDOWN\n"
    "  If set, the profiler saves a profile to the named file on shutdown.\n"
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

  exit(aExitCode);
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

class Sampler
{
public:
  // Sets up the profiler such that it can begin sampling.
  explicit Sampler(PSLockRef aLock);

  // Disable the sampler, restoring it to its previous state. This must be
  // called once, and only once, before the Sampler is destroyed.
  void Disable(PSLockRef aLock);

  // This method suspends and resumes the samplee thread. It calls the passed-in
  // function-like object aProcessRegs (passing it a populated |const
  // Registers&| arg) while the samplee thread is suspended.
  //
  // Func must be a function-like object of type `void()`.
  template<typename Func>
  void SuspendAndSampleAndResumeThread(PSLockRef aLock,
                                       const RegisteredThread& aRegisteredThread,
                                       const Func& aProcessRegs);

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

class SamplerThread : public Sampler
{
public:
  // Creates a sampler thread, but doesn't start it.
  SamplerThread(PSLockRef aLock, uint32_t aActivityGeneration,
                double aIntervalMilliseconds);
  ~SamplerThread();

  // This runs on (is!) the sampler thread.
  void Run();

  // This runs on the main thread.
  void Stop(PSLockRef aLock);

private:
  // This suspends the calling thread for the given number of microseconds.
  // Best effort timing.
  void SleepMicro(uint32_t aMicroseconds);

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

  SamplerThread(const SamplerThread&) = delete;
  void operator=(const SamplerThread&) = delete;
};

// This function is required because we need to create a SamplerThread within
// ActivePS's constructor, but SamplerThread is defined after ActivePS. It
// could probably be removed by moving some code around.
static SamplerThread*
NewSamplerThread(PSLockRef aLock, uint32_t aGeneration, double aInterval)
{
  return new SamplerThread(aLock, aGeneration, aInterval);
}

// This function is the sampler thread.  This implementation is used for all
// targets.
void
SamplerThread::Run()
{
  PR_SetCurrentThreadName("SamplerThread");

  // This will be positive if we are running behind schedule (sampling less
  // frequently than desired) and negative if we are ahead of schedule.
  TimeDuration lastSleepOvershoot = 0;
  TimeStamp sampleStart = TimeStamp::Now();

  while (true) {
    // This scope is for |lock|. It ends before we sleep below.
    {
      PSAutoLock lock(gPSMutex);

      if (!ActivePS::Exists(lock)) {
        return;
      }

      // At this point profiler_stop() might have been called, and
      // profiler_start() might have been called on another thread. If this
      // happens the generation won't match.
      if (ActivePS::Generation(lock) != mActivityGeneration) {
        return;
      }

      ActivePS::Buffer(lock).DeleteExpiredStoredMarkers();

      if (!ActivePS::IsPaused(lock)) {
        const nsTArray<LiveProfiledThreadData>& liveThreads =
          ActivePS::LiveProfiledThreads(lock);

        int64_t rssMemory = 0;
        int64_t ussMemory = 0;
        if (!liveThreads.IsEmpty() && ActivePS::FeatureMemory(lock)) {
          rssMemory = nsMemoryReporterManager::ResidentFast();
#if defined(GP_OS_linux) || defined(GP_OS_android)
          ussMemory = nsMemoryReporterManager::ResidentUnique();
#endif
        }

        for (auto& thread : liveThreads) {
          RegisteredThread* registeredThread = thread.mRegisteredThread;
          ProfiledThreadData* profiledThreadData =
            thread.mProfiledThreadData.get();
          RefPtr<ThreadInfo> info = registeredThread->Info();

          // If the thread is asleep and has been sampled before in the same
          // sleep episode, find and copy the previous sample, as that's
          // cheaper than taking a new sample.
          if (registeredThread->RacyRegisteredThread().CanDuplicateLastSampleDueToSleep()) {
            bool dup_ok =
              ActivePS::Buffer(lock).DuplicateLastSample(
                info->ThreadId(), CorePS::ProcessStartTime(),
                profiledThreadData->LastSample());
            if (dup_ok) {
              continue;
            }
          }

          ThreadResponsiveness* resp = profiledThreadData->GetThreadResponsiveness();
          if (resp) {
            resp->Update();
          }

          TimeStamp now = TimeStamp::Now();
          SuspendAndSampleAndResumeThread(lock, *registeredThread,
                                          [&](const Registers& aRegs) {
            DoPeriodicSample(lock, *registeredThread, *profiledThreadData, now,
                             aRegs, rssMemory, ussMemory);
          });
        }

#if defined(USE_LUL_STACKWALK)
        // The LUL unwind object accumulates frame statistics. Periodically we
        // should poke it to give it a chance to print those statistics.  This
        // involves doing I/O (fprintf, __android_log_print, etc.) and so
        // can't safely be done from the critical section inside
        // SuspendAndSampleAndResumeThread, which is why it is done here.
        CorePS::Lul(lock)->MaybeShowStats();
#endif
      }
    }
    // gPSMutex is not held after this point.

    // Calculate how long a sleep to request.  After the sleep, measure how
    // long we actually slept and take the difference into account when
    // calculating the sleep interval for the next iteration.  This is an
    // attempt to keep "to schedule" in the presence of inaccuracy of the
    // actual sleep intervals.
    TimeStamp targetSleepEndTime =
      sampleStart + TimeDuration::FromMicroseconds(mIntervalMicroseconds);
    TimeStamp beforeSleep = TimeStamp::Now();
    TimeDuration targetSleepDuration = targetSleepEndTime - beforeSleep;
    double sleepTime = std::max(0.0, (targetSleepDuration -
                                      lastSleepOvershoot).ToMicroseconds());
    SleepMicro(static_cast<uint32_t>(sleepTime));
    sampleStart = TimeStamp::Now();
    lastSleepOvershoot =
      sampleStart - (beforeSleep + TimeDuration::FromMicroseconds(sleepTime));
  }
}

// We #include these files directly because it means those files can use
// declarations from this file trivially.  These provide target-specific
// implementations of all SamplerThread methods except Run().
#if defined(GP_OS_windows)
# include "platform-win32.cpp"
#elif defined(GP_OS_darwin)
# include "platform-macos.cpp"
#elif defined(GP_OS_linux) || defined(GP_OS_android)
# include "platform-linux-android.cpp"
#else
# error "bad platform"
#endif

UniquePlatformData
AllocPlatformData(int aThreadId)
{
  return UniquePlatformData(new PlatformData(aThreadId));
}

void
PlatformDataDestructor::operator()(PlatformData* aData)
{
  delete aData;
}

// END SamplerThread
////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////
// BEGIN externally visible functions

MOZ_DEFINE_MALLOC_SIZE_OF(GeckoProfilerMallocSizeOf)

NS_IMETHODIMP
GeckoProfilerReporter::CollectReports(nsIHandleReportCallback* aHandleReport,
                                      nsISupports* aData, bool aAnonymize)
{
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

static bool
HasFeature(const char** aFeatures, uint32_t aFeatureCount, const char* aFeature)
{
  for (size_t i = 0; i < aFeatureCount; i++) {
    if (strcmp(aFeatures[i], aFeature) == 0) {
      return true;
    }
  }
  return false;
}

uint32_t
ParseFeaturesFromStringArray(const char** aFeatures, uint32_t aFeatureCount)
{
  #define ADD_FEATURE_BIT(n_, str_, Name_) \
    if (HasFeature(aFeatures, aFeatureCount, str_)) { \
      features |= ProfilerFeature::Name_; \
    }

  uint32_t features = 0;
  PROFILER_FOR_EACH_FEATURE(ADD_FEATURE_BIT)

  #undef ADD_FEATURE_BIT

  return features;
}

// Find the RegisteredThread for the current thread. This should only be called
// in places where TLSRegisteredThread can't be used.
static RegisteredThread*
FindCurrentThreadRegisteredThread(PSLockRef aLock)
{
  int id = Thread::GetCurrentId();
  const nsTArray<UniquePtr<RegisteredThread>>& registeredThreads =
    CorePS::RegisteredThreads(aLock);
  for (auto& registeredThread : registeredThreads) {
    if (registeredThread->Info()->ThreadId() == id) {
      return registeredThread.get();
    }
  }

  return nullptr;
}

static void
locked_register_thread(PSLockRef aLock, const char* aName, void* aStackTop)
{
  MOZ_RELEASE_ASSERT(CorePS::Exists());

  MOZ_RELEASE_ASSERT(!FindCurrentThreadRegisteredThread(aLock));

  VTUNE_REGISTER_THREAD(aName);

  if (!TLSRegisteredThread::Init(aLock)) {
    return;
  }

  RefPtr<ThreadInfo> info =
    new ThreadInfo(aName, Thread::GetCurrentId(), NS_IsMainThread());
  UniquePtr<RegisteredThread> registeredThread =
    MakeUnique<RegisteredThread>(info, NS_GetCurrentThreadNoCreate(),
                                 aStackTop);

  TLSRegisteredThread::SetRegisteredThread(aLock, registeredThread.get());

  if (ActivePS::Exists(aLock) &&
      ActivePS::ShouldProfileThread(aLock, info)) {
    nsCOMPtr<nsIEventTarget> eventTarget = registeredThread->GetEventTarget();
    ProfiledThreadData* profiledThreadData =
      ActivePS::AddLiveProfiledThread(aLock, registeredThread.get(),
        MakeUnique<ProfiledThreadData>(info, eventTarget,
                                       ActivePS::FeatureResponsiveness(aLock)));

    if (ActivePS::FeatureJS(aLock)) {
      // This StartJSSampling() call is on-thread, so we can poll manually to
      // start JS sampling immediately.
      registeredThread->StartJSSampling(
        ActivePS::FeatureTrackOptimizations(aLock));
      registeredThread->PollJSSampling();
      if (registeredThread->GetJSContext()) {
        profiledThreadData->NotifyReceivedJSContext(ActivePS::Buffer(aLock).mRangeEnd);
      }
    }
  }

  CorePS::AppendRegisteredThread(aLock, std::move(registeredThread));
}

static void
NotifyObservers(const char* aTopic, nsISupports* aSubject = nullptr)
{
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

static void
NotifyProfilerStarted(const int aEntries, double aInterval, uint32_t aFeatures,
                      const char** aFilters, uint32_t aFilterCount)
{
  nsTArray<nsCString> filtersArray;
  for (size_t i = 0; i < aFilterCount; ++i) {
    filtersArray.AppendElement(aFilters[i]);
  }

  nsCOMPtr<nsIProfilerStartParams> params =
    new nsProfilerStartParams(aEntries, aInterval, aFeatures, filtersArray);

  ProfilerParent::ProfilerStarted(params);
  NotifyObservers("profiler-started", params);
}

static void
locked_profiler_start(PSLockRef aLock, uint32_t aEntries, double aInterval,
                      uint32_t aFeatures,
                      const char** aFilters, uint32_t aFilterCount);

// This basically duplicates AutoProfilerLabel's constructor.
ProfilingStack*
MozGlueLabelEnter(const char* aLabel, const char* aDynamicString, void* aSp,
                  uint32_t aLine)
{
  ProfilingStack* profilingStack = AutoProfilerLabel::sProfilingStack.get();
  if (profilingStack) {
    profilingStack->pushLabelFrame(aLabel, aDynamicString, aSp, aLine,
                                js::ProfilingStackFrame::Category::OTHER);
  }
  return profilingStack;
}

// This basically duplicates AutoProfilerLabel's destructor.
void
MozGlueLabelExit(ProfilingStack* sProfilingStack)
{
  if (sProfilingStack) {
    sProfilingStack->pop();
  }
}

static nsTArray<const char*>
SplitAtCommas(const char* aString, UniquePtr<char[]>& aStorage)
{
  size_t len = strlen(aString);
  aStorage = MakeUnique<char[]>(len + 1);
  PodCopy(aStorage.get(), aString, len + 1);

  // Iterate over all characters in aStorage and split at commas, by
  // overwriting commas with the null char.
  nsTArray<const char*> array;
  size_t currentElementStart = 0;
  for (size_t i = 0; i <= len; i++) {
    if (aStorage[i] == ',') {
      aStorage[i] = '\0';
    }
    if (aStorage[i] == '\0') {
      array.AppendElement(&aStorage[currentElementStart]);
      currentElementStart = i + 1;
    }
  }
  return array;
}

void
profiler_init(void* aStackTop)
{
  LOG("profiler_init");

  VTUNE_INIT();

  MOZ_RELEASE_ASSERT(!CorePS::Exists());

  if (getenv("MOZ_PROFILER_HELP")) {
    PrintUsageThenExit(0); // terminates execution
  }

  SharedLibraryInfo::Initialize();

  uint32_t features =
#if defined(GP_OS_android)
                      ProfilerFeature::Java |
#endif
                      ProfilerFeature::JS |
                      ProfilerFeature::Leaf |
#if defined(HAVE_NATIVE_UNWIND)
                      ProfilerFeature::StackWalk |
#endif
                      ProfilerFeature::Threads |
                      ProfilerFeature::Responsiveness |
                      0;

  UniquePtr<char[]> filterStorage;

  nsTArray<const char*> filters;
  filters.AppendElement("GeckoMain");
  filters.AppendElement("Compositor");
  filters.AppendElement("DOM Worker");

  int entries = PROFILER_DEFAULT_ENTRIES;
  double interval = PROFILER_DEFAULT_INTERVAL;

  {
    PSAutoLock lock(gPSMutex);

    // We've passed the possible failure point. Instantiate CorePS, which
    // indicates that the profiler has initialized successfully.
    CorePS::Create(lock);

    locked_register_thread(lock, kMainThreadName, aStackTop);

    // Platform-specific initialization.
    PlatformInit(lock);

#ifdef MOZ_TASK_TRACER
    tasktracer::InitTaskTracer();
#endif

#if defined(GP_OS_android)
    if (jni::IsFennec()) {
      GeckoJavaSampler::Init();
    }
#endif

    // Setup support for pushing/popping labels in mozglue.
    RegisterProfilerLabelEnterExit(MozGlueLabelEnter, MozGlueLabelExit);

    // (Linux-only) We could create CorePS::mLul and read unwind info into it
    // at this point. That would match the lifetime implied by destruction of
    // it in profiler_shutdown() just below. However, that gives a big delay on
    // startup, even if no profiling is actually to be done. So, instead, it is
    // created on demand at the first call to PlatformStart().

    const char* startupEnv = getenv("MOZ_PROFILER_STARTUP");
    if (!startupEnv || startupEnv[0] == '\0') {
      return;
    }

    LOG("- MOZ_PROFILER_STARTUP is set");

    const char* startupEntries = getenv("MOZ_PROFILER_STARTUP_ENTRIES");
    if (startupEntries && startupEntries[0] != '\0') {
      errno = 0;
      entries = strtol(startupEntries, nullptr, 10);
      if (errno == 0 && entries > 0) {
        LOG("- MOZ_PROFILER_STARTUP_ENTRIES = %d", entries);
      } else {
        LOG("- MOZ_PROFILER_STARTUP_ENTRIES not a valid integer: %s",
            startupEntries);
        PrintUsageThenExit(1);
      }
    }

    const char* startupInterval = getenv("MOZ_PROFILER_STARTUP_INTERVAL");
    if (startupInterval && startupInterval[0] != '\0') {
      errno = 0;
      interval = PR_strtod(startupInterval, nullptr);
      if (errno == 0 && interval > 0.0 && interval <= 1000.0) {
        LOG("- MOZ_PROFILER_STARTUP_INTERVAL = %f", interval);
      } else {
        LOG("- MOZ_PROFILER_STARTUP_INTERVAL not a valid float: %s",
            startupInterval);
        PrintUsageThenExit(1);
      }
    }

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
        nsTArray<const char*> featureStringArray =
          SplitAtCommas(startupFeatures, featureStringStorage);
        features = ParseFeaturesFromStringArray(featureStringArray.Elements(),
                                                featureStringArray.Length());
        LOG("- MOZ_PROFILER_STARTUP_FEATURES = %d", features);
      }
    }

    const char* startupFilters = getenv("MOZ_PROFILER_STARTUP_FILTERS");
    if (startupFilters && startupFilters[0] != '\0') {
      filters = SplitAtCommas(startupFilters, filterStorage);
      LOG("- MOZ_PROFILER_STARTUP_FILTERS = %s", startupFilters);
    }

    locked_profiler_start(lock, entries, interval, features,
                          filters.Elements(), filters.Length());
  }

  // We do this with gPSMutex unlocked. The comment in profiler_stop() explains
  // why.
  NotifyProfilerStarted(entries, interval, features,
                        filters.Elements(), filters.Length());
}

static void
locked_profiler_save_profile_to_file(PSLockRef aLock, const char* aFilename,
                                     bool aIsShuttingDown);

static SamplerThread*
locked_profiler_stop(PSLockRef aLock);

void
profiler_shutdown()
{
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

      samplerThread = locked_profiler_stop(lock);
    }

    CorePS::Destroy(lock);

    // We just destroyed CorePS and the ThreadInfos it contains, so we can
    // clear this thread's TLSRegisteredThread.
    TLSRegisteredThread::SetRegisteredThread(lock, nullptr);

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

static bool
WriteProfileToJSONWriter(SpliceableChunkedJSONWriter& aWriter,
                         double aSinceTime,
                         bool aIsShuttingDown)
{
  LOG("WriteProfileToJSONWriter");

  MOZ_RELEASE_ASSERT(CorePS::Exists());

  aWriter.Start();
  {
    if (!profiler_stream_json_for_this_process(
          aWriter, aSinceTime, aIsShuttingDown)) {
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

UniquePtr<char[]>
profiler_get_profile(double aSinceTime, bool aIsShuttingDown)
{
  LOG("profiler_get_profile");

  SpliceableChunkedJSONWriter b;
  if (!WriteProfileToJSONWriter(b, aSinceTime, aIsShuttingDown)) {
    return nullptr;
  }
  return b.WriteFunc()->CopyData();
}

void
profiler_get_profile_json_into_lazily_allocated_buffer(
  const std::function<char*(size_t)>& aAllocator,
  double aSinceTime,
  bool aIsShuttingDown)
{
  LOG("profiler_get_profile_json_into_lazily_allocated_buffer");

  SpliceableChunkedJSONWriter b;
  if (!WriteProfileToJSONWriter(b, aSinceTime, aIsShuttingDown)) {
    return;
  }

  b.WriteFunc()->CopyDataIntoLazilyAllocatedBuffer(aAllocator);
}

void
profiler_get_start_params(int* aEntries, double* aInterval, uint32_t* aFeatures,
                          Vector<const char*>* aFilters)
{
  MOZ_RELEASE_ASSERT(CorePS::Exists());

  if (NS_WARN_IF(!aEntries) || NS_WARN_IF(!aInterval) ||
      NS_WARN_IF(!aFeatures) || NS_WARN_IF(!aFilters)) {
    return;
  }

  PSAutoLock lock(gPSMutex);

  if (!ActivePS::Exists(lock)) {
    *aEntries = 0;
    *aInterval = 0;
    *aFeatures = 0;
    aFilters->clear();
    return;
  }

  *aEntries = ActivePS::Entries(lock);
  *aInterval = ActivePS::Interval(lock);
  *aFeatures = ActivePS::Features(lock);

  const Vector<std::string>& filters = ActivePS::Filters(lock);
  MOZ_ALWAYS_TRUE(aFilters->resize(filters.length()));
  for (uint32_t i = 0; i < filters.length(); ++i) {
    (*aFilters)[i] = filters[i].c_str();
  }
}

AutoSetProfilerEnvVarsForChildProcess::AutoSetProfilerEnvVarsForChildProcess(
  MOZ_GUARD_OBJECT_NOTIFIER_ONLY_PARAM_IN_IMPL)
  : mSetEntries()
  , mSetInterval()
  , mSetFeaturesBitfield()
  , mSetFilters()
{
  MOZ_GUARD_OBJECT_NOTIFIER_INIT;

  MOZ_RELEASE_ASSERT(CorePS::Exists());

  PSAutoLock lock(gPSMutex);

  if (!ActivePS::Exists(lock)) {
    PR_SetEnv("MOZ_PROFILER_STARTUP=");
    return;
  }

  PR_SetEnv("MOZ_PROFILER_STARTUP=1");
  SprintfLiteral(mSetEntries, "MOZ_PROFILER_STARTUP_ENTRIES=%d",
                 ActivePS::Entries(lock));
  PR_SetEnv(mSetEntries);

  // Use AppendFloat instead of SprintfLiteral with %f because the decimal
  // separator used by %f is locale-dependent. But the string we produce needs
  // to be parseable by strtod, which only accepts the period character as a
  // decimal separator. AppendFloat always uses the period character.
  nsCString setInterval;
  setInterval.AppendLiteral("MOZ_PROFILER_STARTUP_INTERVAL=");
  setInterval.AppendFloat(ActivePS::Interval(lock));
  strncpy(mSetInterval, setInterval.get(), MOZ_ARRAY_LENGTH(mSetInterval));
  mSetInterval[MOZ_ARRAY_LENGTH(mSetInterval) - 1] = '\0';
  PR_SetEnv(mSetInterval);

  SprintfLiteral(mSetFeaturesBitfield,
                 "MOZ_PROFILER_STARTUP_FEATURES_BITFIELD=%d",
                 ActivePS::Features(lock));
  PR_SetEnv(mSetFeaturesBitfield);

  std::string filtersString;
  const Vector<std::string>& filters = ActivePS::Filters(lock);
  for (uint32_t i = 0; i < filters.length(); ++i) {
    filtersString += filters[i];
    if (i != filters.length() - 1) {
      filtersString += ",";
    }
  }
  SprintfLiteral(mSetFilters, "MOZ_PROFILER_STARTUP_FILTERS=%s",
                 filtersString.c_str());
  PR_SetEnv(mSetFilters);
}

AutoSetProfilerEnvVarsForChildProcess::~AutoSetProfilerEnvVarsForChildProcess()
{
  // Our current process doesn't look at these variables after startup, so we
  // can just unset all the variables. This allows us to use literal strings,
  // which will be valid for the whole life time of the program and can be
  // passed to PR_SetEnv without problems.
  PR_SetEnv("MOZ_PROFILER_STARTUP=");
  PR_SetEnv("MOZ_PROFILER_STARTUP_ENTRIES=");
  PR_SetEnv("MOZ_PROFILER_STARTUP_INTERVAL=");
  PR_SetEnv("MOZ_PROFILER_STARTUP_FEATURES_BITFIELD=");
  PR_SetEnv("MOZ_PROFILER_STARTUP_FILTERS=");
}

static void
locked_profiler_save_profile_to_file(PSLockRef aLock, const char* aFilename,
                                     bool aIsShuttingDown = false)
{
  LOG("locked_profiler_save_profile_to_file(%s)", aFilename);

  MOZ_RELEASE_ASSERT(CorePS::Exists() && ActivePS::Exists(aLock));

  std::ofstream stream;
  stream.open(aFilename);
  if (stream.is_open()) {
    SpliceableJSONWriter w(MakeUnique<OStreamJSONWriteFunc>(stream));
    w.Start();
    {
      locked_profiler_stream_json_for_this_process(aLock, w, /* sinceTime */ 0,
                                                   aIsShuttingDown);

      // Don't include profiles from other processes because this is a
      // synchronous function.
      w.StartArrayProperty("processes");
      w.EndArray();
    }
    w.End();

    stream.close();
  }
}

void
profiler_save_profile_to_file(const char* aFilename)
{
  LOG("profiler_save_profile_to_file(%s)", aFilename);

  MOZ_RELEASE_ASSERT(CorePS::Exists());

  PSAutoLock lock(gPSMutex);

  if (!ActivePS::Exists(lock)) {
    return;
  }

  locked_profiler_save_profile_to_file(lock, aFilename);
}

uint32_t
profiler_get_available_features()
{
  MOZ_RELEASE_ASSERT(CorePS::Exists());

  uint32_t features = 0;

  #define ADD_FEATURE(n_, str_, Name_) ProfilerFeature::Set##Name_(features);

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

  return features;
}

Maybe<ProfilerBufferInfo>
profiler_get_buffer_info()
{
  MOZ_RELEASE_ASSERT(CorePS::Exists());

  PSAutoLock lock(gPSMutex);

  if (!ActivePS::Exists(lock)) {
    return Nothing();
  }

  return Some(ProfilerBufferInfo {
    ActivePS::Buffer(lock).mRangeStart,
    ActivePS::Buffer(lock).mRangeEnd,
    ActivePS::Entries(lock)
  });
}

static void
PollJSSamplingForCurrentThread()
{
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
static void
TriggerPollJSSamplingOnMainThread()
{
  nsCOMPtr<nsIThread> mainThread;
  nsresult rv = NS_GetMainThread(getter_AddRefs(mainThread));
  if (NS_SUCCEEDED(rv) && mainThread) {
    nsCOMPtr<nsIRunnable> task =
      NS_NewRunnableFunction("TriggerPollJSSamplingOnMainThread", []() {
        PollJSSamplingForCurrentThread();
      });
    SystemGroup::Dispatch(TaskCategory::Other, task.forget());
  }
}

static void
locked_profiler_start(PSLockRef aLock, uint32_t aEntries, double aInterval,
                      uint32_t aFeatures,
                      const char** aFilters, uint32_t aFilterCount)
{
  if (LOG_TEST) {
    LOG("locked_profiler_start");
    LOG("- entries  = %d", aEntries);
    LOG("- interval = %.2f", aInterval);

    #define LOG_FEATURE(n_, str_, Name_) \
      if (ProfilerFeature::Has##Name_(aFeatures)) { \
        LOG("- feature  = %s", str_); \
      }

    PROFILER_FOR_EACH_FEATURE(LOG_FEATURE)

    #undef LOG_FEATURE

    for (uint32_t i = 0; i < aFilterCount; i++) {
      LOG("- threads  = %s", aFilters[i]);
    }
  }

  MOZ_RELEASE_ASSERT(CorePS::Exists() && !ActivePS::Exists(aLock));

#if defined(GP_PLAT_amd64_windows)
  InitializeWin64ProfilerHooks();
#endif

  // Fall back to the default values if the passed-in values are unreasonable.
  uint32_t entries = aEntries > 0 ? aEntries : PROFILER_DEFAULT_ENTRIES;
  double interval = aInterval > 0 ? aInterval : PROFILER_DEFAULT_INTERVAL;

  ActivePS::Create(aLock, entries, interval, aFeatures, aFilters, aFilterCount);

  // Set up profiling for each registered thread, if appropriate.
  int tid = Thread::GetCurrentId();
  const nsTArray<UniquePtr<RegisteredThread>>& registeredThreads =
    CorePS::RegisteredThreads(aLock);
  for (auto& registeredThread : registeredThreads) {
    RefPtr<ThreadInfo> info = registeredThread->Info();

    if (ActivePS::ShouldProfileThread(aLock, info)) {
      nsCOMPtr<nsIEventTarget> eventTarget = registeredThread->GetEventTarget();
      ProfiledThreadData* profiledThreadData =
        ActivePS::AddLiveProfiledThread(aLock, registeredThread.get(),
          MakeUnique<ProfiledThreadData>(info, eventTarget,
                                         ActivePS::FeatureResponsiveness(aLock)));
      if (ActivePS::FeatureJS(aLock)) {
        registeredThread->StartJSSampling(
          ActivePS::FeatureTrackOptimizations(aLock));
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
      registeredThread->RacyRegisteredThread().ReinitializeOnResume();
      if (registeredThread->GetJSContext()) {
        profiledThreadData->NotifyReceivedJSContext(0);
      }
    }
  }

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
    java::GeckoJavaSampler::Start(javaInterval, 1000);
  }
#endif

  // At the very end, set up RacyFeatures.
  RacyFeatures::SetActive(ActivePS::Features(aLock));
}

void
profiler_start(uint32_t aEntries, double aInterval, uint32_t aFeatures,
               const char** aFilters, uint32_t aFilterCount)
{
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

    locked_profiler_start(lock, aEntries, aInterval, aFeatures,
                          aFilters, aFilterCount);
  }

  // We do these operations with gPSMutex unlocked. The comments in
  // profiler_stop() explain why.
  if (samplerThread) {
    ProfilerParent::ProfilerStopped();
    NotifyObservers("profiler-stopped");
    delete samplerThread;
  }
  NotifyProfilerStarted(aEntries, aInterval, aFeatures,
                        aFilters, aFilterCount);
}

void
profiler_ensure_started(uint32_t aEntries, double aInterval, uint32_t aFeatures,
                        const char** aFilters, uint32_t aFilterCount)
{
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
      if (!ActivePS::Equals(lock, aEntries, aInterval, aFeatures,
                            aFilters, aFilterCount)) {
        // Stop and restart with different settings.
        samplerThread = locked_profiler_stop(lock);
        locked_profiler_start(lock, aEntries, aInterval, aFeatures,
                              aFilters, aFilterCount);
        startedProfiler = true;
      }
    } else {
      // The profiler is stopped.
      locked_profiler_start(lock, aEntries, aInterval, aFeatures,
                            aFilters, aFilterCount);
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
    NotifyProfilerStarted(aEntries, aInterval, aFeatures,
                          aFilters, aFilterCount);
  }
}

static MOZ_MUST_USE SamplerThread*
locked_profiler_stop(PSLockRef aLock)
{
  LOG("locked_profiler_stop");

  MOZ_RELEASE_ASSERT(CorePS::Exists() && ActivePS::Exists(aLock));

  // At the very start, clear RacyFeatures.
  RacyFeatures::SetInactive();

#ifdef MOZ_TASK_TRACER
  if (ActivePS::FeatureTaskTracer(aLock)) {
    tasktracer::StopLogging();
  }
#endif

  // Stop sampling live threads.
  int tid = Thread::GetCurrentId();
  const nsTArray<LiveProfiledThreadData>& liveProfiledThreads =
    ActivePS::LiveProfiledThreads(aLock);
  for (auto& thread : liveProfiledThreads) {
    RegisteredThread* registeredThread = thread.mRegisteredThread;
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

  // The Stop() call doesn't actually stop Run(); that happens in this
  // function's caller when the sampler thread is destroyed. Stop() just gives
  // the SamplerThread a chance to do some cleanup with gPSMutex locked.
  SamplerThread* samplerThread = ActivePS::Destroy(aLock);
  samplerThread->Stop(aLock);

  return samplerThread;
}

void
profiler_stop()
{
  LOG("profiler_stop");

  MOZ_RELEASE_ASSERT(CorePS::Exists());

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

bool
profiler_is_paused()
{
  MOZ_RELEASE_ASSERT(CorePS::Exists());

  PSAutoLock lock(gPSMutex);

  if (!ActivePS::Exists(lock)) {
    return false;
  }

  return ActivePS::IsPaused(lock);
}

void
profiler_pause()
{
  LOG("profiler_pause");

  MOZ_RELEASE_ASSERT(CorePS::Exists());

  {
    PSAutoLock lock(gPSMutex);

    if (!ActivePS::Exists(lock)) {
      return;
    }

    ActivePS::SetIsPaused(lock, true);
    ActivePS::Buffer(lock).AddEntry(ProfileBufferEntry::Pause(profiler_time()));
  }

  // gPSMutex must be unlocked when we notify, to avoid potential deadlocks.
  ProfilerParent::ProfilerPaused();
  NotifyObservers("profiler-paused");
}

void
profiler_resume()
{
  LOG("profiler_resume");

  MOZ_RELEASE_ASSERT(CorePS::Exists());

  {
    PSAutoLock lock(gPSMutex);

    if (!ActivePS::Exists(lock)) {
      return;
    }

    ActivePS::Buffer(lock).AddEntry(ProfileBufferEntry::Resume(profiler_time()));
    ActivePS::SetIsPaused(lock, false);
  }

  // gPSMutex must be unlocked when we notify, to avoid potential deadlocks.
  ProfilerParent::ProfilerResumed();
  NotifyObservers("profiler-resumed");
}

bool
profiler_feature_active(uint32_t aFeature)
{
  // This function runs both on and off the main thread.

  MOZ_RELEASE_ASSERT(CorePS::Exists());

  // This function is hot enough that we use RacyFeatures, not ActivePS.
  return RacyFeatures::IsActiveWithFeature(aFeature);
}

void
profiler_register_thread(const char* aName, void* aGuessStackTop)
{
  DEBUG_LOG("profiler_register_thread(%s)", aName);

  MOZ_ASSERT_IF(NS_IsMainThread(), Scheduler::IsCooperativeThread());
  MOZ_RELEASE_ASSERT(CorePS::Exists());

  PSAutoLock lock(gPSMutex);

  void* stackTop = GetStackTop(aGuessStackTop);
  locked_register_thread(lock, aName, stackTop);
}

void
profiler_unregister_thread()
{
  MOZ_ASSERT_IF(NS_IsMainThread(), Scheduler::IsCooperativeThread());

  if (!CorePS::Exists()) {
    // This function can be called after the main thread has already shut down.
    return;
  }

  PSAutoLock lock(gPSMutex);

  // We don't call RegisteredThread::StopJSSampling() here; there's no point
  // doing that for a JS thread that is in the process of disappearing.

  RegisteredThread* registeredThread = FindCurrentThreadRegisteredThread(lock);
  MOZ_RELEASE_ASSERT(registeredThread == TLSRegisteredThread::RegisteredThread(lock));
  if (registeredThread) {
    RefPtr<ThreadInfo> info = registeredThread->Info();

    DEBUG_LOG("profiler_unregister_thread: %s", info->Name());

    if (ActivePS::Exists(lock)) {
      ActivePS::UnregisterThread(lock, registeredThread);
    }

    // Clear the pointer to the RegisteredThread object that we're about to
    // destroy.
    TLSRegisteredThread::SetRegisteredThread(lock, nullptr);

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

void
profiler_thread_sleep()
{
  // This function runs both on and off the main thread.

  MOZ_RELEASE_ASSERT(CorePS::Exists());

  RacyRegisteredThread* racyRegisteredThread =
    TLSRegisteredThread::RacyRegisteredThread();
  if (!racyRegisteredThread) {
    return;
  }

  racyRegisteredThread->SetSleeping();
}

void
profiler_thread_wake()
{
  // This function runs both on and off the main thread.

  MOZ_RELEASE_ASSERT(CorePS::Exists());

  RacyRegisteredThread* racyRegisteredThread =
    TLSRegisteredThread::RacyRegisteredThread();
  if (!racyRegisteredThread) {
    return;
  }

  racyRegisteredThread->SetAwake();
}

bool
profiler_thread_is_sleeping()
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  MOZ_RELEASE_ASSERT(CorePS::Exists());

  RacyRegisteredThread* racyRegisteredThread =
    TLSRegisteredThread::RacyRegisteredThread();
  if (!racyRegisteredThread) {
    return false;
  }
  return racyRegisteredThread->IsSleeping();
}

void
profiler_js_interrupt_callback()
{
  // This function runs on JS threads being sampled.
  PollJSSamplingForCurrentThread();
}

double
profiler_time()
{
  MOZ_RELEASE_ASSERT(CorePS::Exists());

  TimeDuration delta = TimeStamp::Now() - CorePS::ProcessStartTime();
  return delta.ToMilliseconds();
}

UniqueProfilerBacktrace
profiler_get_backtrace()
{
  MOZ_RELEASE_ASSERT(CorePS::Exists());

  PSAutoLock lock(gPSMutex);

  if (!ActivePS::Exists(lock) || ActivePS::FeaturePrivacy(lock)) {
    return nullptr;
  }

  RegisteredThread* registeredThread =
    TLSRegisteredThread::RegisteredThread(lock);
  if (!registeredThread) {
    MOZ_ASSERT(registeredThread);
    return nullptr;
  }

  int tid = Thread::GetCurrentId();

  TimeStamp now = TimeStamp::Now();

  Registers regs;
#if defined(HAVE_NATIVE_UNWIND)
  regs.SyncPopulate();
#else
  regs.Clear();
#endif

  // 1000 should be plenty for a single backtrace.
  auto buffer = MakeUnique<ProfileBuffer>(1000);

  DoSyncSample(lock, *registeredThread, now, regs, *buffer.get());

  return UniqueProfilerBacktrace(
    new ProfilerBacktrace("SyncProfile", tid, std::move(buffer)));
}

void
ProfilerBacktraceDestructor::operator()(ProfilerBacktrace* aBacktrace)
{
  delete aBacktrace;
}

static void
racy_profiler_add_marker(const char* aMarkerName,
                         UniquePtr<ProfilerMarkerPayload> aPayload)
{
  MOZ_RELEASE_ASSERT(CorePS::Exists());

  // We don't assert that RacyFeatures::IsActiveWithoutPrivacy() is true here,
  // because it's possible that the result has changed since we tested it in
  // the caller.
  //
  // Because of this imprecision it's possible to miss a marker or record one
  // we shouldn't. Either way is not a big deal.

  RacyRegisteredThread* racyRegisteredThread =
    TLSRegisteredThread::RacyRegisteredThread();
  if (!racyRegisteredThread) {
    return;
  }

  TimeStamp origin = (aPayload && !aPayload->GetStartTime().IsNull())
                       ? aPayload->GetStartTime()
                       : TimeStamp::Now();
  TimeDuration delta = origin - CorePS::ProcessStartTime();
  racyRegisteredThread->AddPendingMarker(aMarkerName, std::move(aPayload),
                                         delta.ToMilliseconds());
}

void
profiler_add_marker(const char* aMarkerName,
                    UniquePtr<ProfilerMarkerPayload> aPayload)
{
  MOZ_RELEASE_ASSERT(CorePS::Exists());

  // This function is hot enough that we use RacyFeatures, not ActivePS.
  if (!RacyFeatures::IsActiveWithoutPrivacy()) {
    return;
  }

  racy_profiler_add_marker(aMarkerName, std::move(aPayload));
}

void
profiler_add_marker(const char* aMarkerName)
{
  profiler_add_marker(aMarkerName, nullptr);
}

void
profiler_add_network_marker(nsIURI* aURI,
                            int32_t aPriority,
                            uint64_t aChannelId,
                            NetworkLoadType aType,
                            mozilla::TimeStamp aStart,
                            mozilla::TimeStamp aEnd,
                            int64_t aCount,
                            const mozilla::net::TimingStruct* aTimings,
                            nsIURI* aRedirectURI)
{
  if (!profiler_is_active()) {
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
  profiler_add_marker(name,
                      MakeUnique<NetworkMarkerPayload>(static_cast<int64_t>(aChannelId),
                                                       PromiseFlatCString(spec).get(),
                                                       aType,
                                                       aStart,
                                                       aEnd,
                                                       aPriority,
                                                       aCount,
                                                       aTimings,
                                                       PromiseFlatCString(redirect_spec).get()));
}

// This logic needs to add a marker for a different thread, so we actually need
// to lock here.
void
profiler_add_marker_for_thread(int aThreadId,
                               const char* aMarkerName,
                               UniquePtr<ProfilerMarkerPayload> aPayload)
{
  MOZ_RELEASE_ASSERT(CorePS::Exists());

  PSAutoLock lock(gPSMutex);
  if (!ActivePS::Exists(lock)) {
    return;
  }

  // Create the ProfilerMarker which we're going to store.
  TimeStamp origin = (aPayload && !aPayload->GetStartTime().IsNull())
                   ? aPayload->GetStartTime()
                   : TimeStamp::Now();
  TimeDuration delta = origin - CorePS::ProcessStartTime();
  ProfilerMarker* marker =
    new ProfilerMarker(aMarkerName, aThreadId, std::move(aPayload),
                       delta.ToMilliseconds());

#ifdef DEBUG
  // Assert that our thread ID makes sense
  bool realThread = false;
  const nsTArray<UniquePtr<RegisteredThread>>& registeredThreads =
    CorePS::RegisteredThreads(lock);
  for (auto& thread : registeredThreads) {
    RefPtr<ThreadInfo> info = thread->Info();
    if (info->ThreadId() == aThreadId) {
      realThread = true;
      break;
    }
  }
  MOZ_ASSERT(realThread, "Invalid thread id");
#endif

  // Insert the marker into the buffer
  ProfileBuffer& buffer = ActivePS::Buffer(lock);
  buffer.AddStoredMarker(marker);
  buffer.AddEntry(ProfileBufferEntry::Marker(marker));
}

void
profiler_tracing(const char* aCategory, const char* aMarkerName,
                 TracingKind aKind)
{
  MOZ_RELEASE_ASSERT(CorePS::Exists());

  VTUNE_TRACING(aMarkerName, aKind);

  // This function is hot enough that we use RacyFeatures, notActivePS.
  if (!RacyFeatures::IsActiveWithoutPrivacy()) {
    return;
  }

  auto payload = MakeUnique<TracingMarkerPayload>(aCategory, aKind);
  racy_profiler_add_marker(aMarkerName, std::move(payload));
}

void
profiler_tracing(const char* aCategory, const char* aMarkerName,
                 TracingKind aKind, UniqueProfilerBacktrace aCause)
{
  MOZ_RELEASE_ASSERT(CorePS::Exists());

  VTUNE_TRACING(aMarkerName, aKind);

  // This function is hot enough that we use RacyFeatures, notActivePS.
  if (!RacyFeatures::IsActiveWithoutPrivacy()) {
    return;
  }

  auto payload =
    MakeUnique<TracingMarkerPayload>(aCategory, aKind, std::move(aCause));
  racy_profiler_add_marker(aMarkerName, std::move(payload));
}

void
profiler_set_js_context(JSContext* aCx)
{
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
      profiledThreadData->NotifyReceivedJSContext(ActivePS::Buffer(lock).mRangeEnd);
    }
  }
}

void
profiler_clear_js_context()
{
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
      profiledThreadData->NotifyAboutToLoseJSContext(cx,
                                                     CorePS::ProcessStartTime(),
                                                     ActivePS::Buffer(lock));

      // Notify the JS context that profiling for this context has stopped.
      // Do this by calling StopJSSampling and PollJSSampling before
      // nulling out the JSContext.
      registeredThread->StopJSSampling();
      registeredThread->PollJSSampling();

      registeredThread->ClearJSContext();

      // Tell the thread that we'd like to have JS sampling on this
      // thread again, once it gets a new JSContext (if ever).
      registeredThread->StartJSSampling(
        ActivePS::FeatureTrackOptimizations(lock));
      return;
    }
  }

  registeredThread->ClearJSContext();
}

int
profiler_current_thread_id()
{
  return Thread::GetCurrentId();
}

// NOTE: aCollector's methods will be called while the target thread is paused.
// Doing things in those methods like allocating -- which may try to claim
// locks -- is a surefire way to deadlock.
void
profiler_suspend_and_sample_thread(int aThreadId,
                                   uint32_t aFeatures,
                                   ProfilerStackCollector& aCollector,
                                   bool aSampleNative /* = true */)
{
  // Lock the profiler mutex
  PSAutoLock lock(gPSMutex);

  const nsTArray<UniquePtr<RegisteredThread>>& registeredThreads =
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
      sampler.SuspendAndSampleAndResumeThread(lock, registeredThread,
                                              [&](const Registers& aRegs) {
        // The target thread is now suspended. Collect a native backtrace, and
        // call the callback.
        bool isSynchronous = false;
#if defined(HAVE_FASTINIT_NATIVE_UNWIND)
        if (aSampleNative) {
          // We can only use FramePointerStackWalk or MozStackWalk from
          // suspend_and_sample_thread as other stackwalking methods may not be
          // initialized.
# if defined(USE_FRAME_POINTER_STACK_WALK)
          DoFramePointerBacktrace(lock, registeredThread, aRegs, nativeStack);
# elif defined(USE_MOZ_STACK_WALK)
          DoMozStackWalkBacktrace(lock, registeredThread, aRegs, nativeStack);
# else
#  error "Invalid configuration"
# endif

          MergeStacks(aFeatures, isSynchronous, registeredThread, aRegs,
                      nativeStack, aCollector);
        } else
#endif
        {
          MergeStacks(aFeatures, isSynchronous, registeredThread, aRegs,
                      nativeStack, aCollector);

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
