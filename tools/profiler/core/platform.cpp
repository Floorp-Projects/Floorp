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
//   stack trace and passing it to a callback function; it does not write to a
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
#include "GeckoProfilerReporter.h"
#include "ProfilerIOInterposeObserver.h"
#include "mozilla/AutoProfilerLabel.h"
#include "mozilla/StackWalk.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/ThreadLocal.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/StaticPtr.h"
#include "ThreadInfo.h"
#include "nsIHttpProtocolHandler.h"
#include "nsIObserverService.h"
#include "nsIXULAppInfo.h"
#include "nsIXULRuntime.h"
#include "nsDirectoryServiceUtils.h"
#include "nsDirectoryServiceDefs.h"
#include "nsMemoryReporterManager.h"
#include "nsXULAppAPI.h"
#include "nsProfilerStartParams.h"
#include "ProfilerParent.h"
#include "mozilla/Services.h"
#include "nsThreadUtils.h"
#include "ProfilerMarkerPayload.h"
#include "shared-libraries.h"
#include "prdtoa.h"
#include "prtime.h"

#ifdef MOZ_TASK_TRACER
#include "GeckoTaskTracer.h"
#endif

#if defined(GP_OS_android)
# include "FennecJNINatives.h"
# include "FennecJNIWrappers.h"
#endif

#if defined(MOZ_PROFILING) && \
    (defined(GP_OS_windows) || defined(GP_OS_darwin))
# define HAVE_NATIVE_UNWIND
# define USE_NS_STACKWALK
#endif

// This should also work on ARM Linux, but not tested there yet.
#if defined(GP_PLAT_arm_android)
# define HAVE_NATIVE_UNWIND
# define USE_EHABI_STACKWALK
# include "EHABIStackWalk.h"
#endif

#if defined(GP_PLAT_amd64_linux) || defined(GP_PLAT_x86_linux)
# define HAVE_NATIVE_UNWIND
# define USE_LUL_STACKWALK
# include "lul/LulMain.h"
# include "lul/platform-linux-lul.h"
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
// - each thread's RacyThreadInfo object is accessible without locking via
//   TLSInfo::RacyThreadInfo().
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
    while (mLiveThreads.size() > 0) {
      delete mLiveThreads.back();
      mLiveThreads.pop_back();
    }

    while (mDeadThreads.size() > 0) {
      delete mDeadThreads.back();
      mDeadThreads.pop_back();
    }

#if defined(USE_LUL_STACKWALK)
    delete mLul;
#endif
  }

public:
  typedef std::vector<ThreadInfo*> ThreadVector;

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

    for (uint32_t i = 0; i < sInstance->mLiveThreads.size(); i++) {
      aProfSize +=
        sInstance->mLiveThreads.at(i)->SizeOfIncludingThis(aMallocSizeOf);
    }

    for (uint32_t i = 0; i < sInstance->mDeadThreads.size(); i++) {
      aProfSize +=
        sInstance->mDeadThreads.at(i)->SizeOfIncludingThis(aMallocSizeOf);
    }

    // Measurement of the following things may be added later if DMD finds it
    // is worthwhile:
    // - CorePS::mLiveThreads itself (its elements' children are measured
    //   above)
    // - CorePS::mDeadThreads itself (ditto)
    // - CorePS::mInterposeObserver

#if defined(USE_LUL_STACKWALK)
    if (sInstance->mLul) {
      aLulSize += sInstance->mLul->SizeOfIncludingThis(aMallocSizeOf);
    }
#endif
  }

  // No PSLockRef is needed for this field because it's immutable.
  PS_GET_LOCKLESS(TimeStamp, ProcessStartTime)

  PS_GET(ThreadVector&, LiveThreads)
  PS_GET(ThreadVector&, DeadThreads)

#ifdef USE_LUL_STACKWALK
  PS_GET_AND_SET(lul::LUL*, Lul)
#endif

private:
  // The singleton instance
  static CorePS* sInstance;

  // The time that the process started.
  const TimeStamp mProcessStartTime;

  // Info on all the registered threads, both live and dead. ThreadIds in
  // mLiveThreads are unique. ThreadIds in mDeadThreads may not be, because
  // ThreadIds can be reused. IsBeingProfiled() is true for all ThreadInfos in
  // mDeadThreads because we don't hold on to ThreadInfos for non-profiled dead
  // threads.
  ThreadVector mLiveThreads;
  ThreadVector mDeadThreads;

#ifdef USE_LUL_STACKWALK
  // LUL's state. Null prior to the first activation, non-null thereafter.
  lul::LUL* mLul;
#endif
};

CorePS* CorePS::sInstance = nullptr;

class SamplerThread;

static SamplerThread*
NewSamplerThread(PSLockRef aLock, uint32_t aGeneration, double aInterval);

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

  ActivePS(PSLockRef aLock, int aEntries, double aInterval,
           uint32_t aFeatures, const char** aFilters, uint32_t aFilterCount)
    : mGeneration(sNextGeneration++)
    , mEntries(aEntries)
    , mInterval(aInterval)
    , mFeatures(AdjustFeatures(aFeatures, aFilterCount))
    , mBuffer(new ProfileBuffer(aEntries))
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
        NS_DispatchToMainThread(NS_NewRunnableFunction([=]() {
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
        NS_DispatchToMainThread(NS_NewRunnableFunction([=]() {
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
    }

    return false;
  }

public:
  static void Create(PSLockRef aLock, int aEntries, double aInterval,
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

  static size_t SizeOf(PSLockRef, MallocSizeOf aMallocSizeOf)
  {
    size_t n = aMallocSizeOf(sInstance);

    n += sInstance->mBuffer->SizeOfIncludingThis(aMallocSizeOf);

    return n;
  }

  static bool ShouldProfileThread(PSLockRef aLock, ThreadInfo* aInfo)
  {
    MOZ_RELEASE_ASSERT(sInstance);

    return ((aInfo->IsMainThread() || FeatureThreads(aLock)) &&
            sInstance->ThreadSelected(aInfo->Name()));
  }

  PS_GET(uint32_t, Generation)

  PS_GET(int, Entries)

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

  static ProfileBuffer* Buffer(PSLockRef) { return sInstance->mBuffer.get(); }

  PS_GET_AND_SET(bool, IsPaused)

#if defined(GP_OS_linux)
  PS_GET_AND_SET(bool, WasPaused)
#endif

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
  const int mEntries;

  // The interval between samples, measured in milliseconds.
  const double mInterval;

  // The profile features that are enabled.
  const uint32_t mFeatures;

  // Substrings of names of threads we want to profile.
  Vector<std::string> mFilters;

  // The buffer into which all samples are recorded. Always used in conjunction
  // with CorePS::m{Live,Dead}Threads.
  const UniquePtr<ProfileBuffer> mBuffer;

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

// The preferred way to check profiler activeness and features is via
// ActivePS(). However, that requires locking gPSMutex. There are some hot
// operations where absolute precision isn't required, so we duplicate the
// activeness/feature state in a lock-free manner in this class.
class RacyFeatures
{
public:
  static void SetActive(uint32_t aFeatures)
  {
    sActiveAndFeatures = Active | aFeatures;
  }

  static void SetInactive() { sActiveAndFeatures = 0; }

  static bool IsActive() { return uint32_t(sActiveAndFeatures) & Active; }

  static bool IsActiveWithFeature(uint32_t aFeature)
  {
    uint32_t af = sActiveAndFeatures;  // copy it first
    return (af & Active) && (af & aFeature);
  }

  static bool IsActiveWithoutPrivacy()
  {
    uint32_t af = sActiveAndFeatures;  // copy it first
    return (af & Active) && !(af & ProfilerFeature::Privacy);
  }

private:
  static const uint32_t Active = 1u << 31;

  // Ensure Active doesn't overlap with any of the feature bits.
  #define NO_OVERLAP(n_, str_, Name_) \
    static_assert(ProfilerFeature::Name_ != Active, "bad Active value");

  PROFILER_FOR_EACH_FEATURE(NO_OVERLAP);

  #undef NO_OVERLAP

  // We combine the active bit with the feature bits so they can be read or
  // written in a single atomic operation.
  static Atomic<uint32_t> sActiveAndFeatures;
};

Atomic<uint32_t> RacyFeatures::sActiveAndFeatures(0);

// Each live thread has a ThreadInfo, and we store a reference to it in TLS.
// This class encapsulates that TLS.
class TLSInfo
{
public:
  static bool Init(PSLockRef)
  {
    bool ok1 = sThreadInfo.init();
    bool ok2 = sPseudoStack.init();
    return ok1 && ok2;
  }

  // Get the entire ThreadInfo. Accesses are guarded by gPSMutex.
  static ThreadInfo* Info(PSLockRef) { return sThreadInfo.get(); }

  // Get only the RacyThreadInfo. Accesses are not guarded by gPSMutex.
  static RacyThreadInfo* RacyInfo()
  {
    ThreadInfo* info = sThreadInfo.get();
    return info ? info->RacyInfo().get() : nullptr;
  }

  // Get only the PseudoStack. Accesses are not guarded by gPSMutex. RacyInfo()
  // can also be used to get the PseudoStack, but that is marginally slower
  // because it requires an extra pointer indirection.
  static PseudoStack* Stack() { return sPseudoStack.get(); }

  static void SetInfo(PSLockRef, ThreadInfo* aInfo)
  {
    sThreadInfo.set(aInfo);
    sPseudoStack.set(aInfo ? aInfo->RacyInfo().get() : nullptr);  // an upcast
  }

private:
  // This is a non-owning reference to the ThreadInfo; CorePS::mLiveThreads is
  // the owning reference. On thread destruction, this reference is cleared and
  // the ThreadInfo is destroyed or transferred to CorePS::mDeadThreads.
  static MOZ_THREAD_LOCAL(ThreadInfo*) sThreadInfo;
};

MOZ_THREAD_LOCAL(ThreadInfo*) TLSInfo::sThreadInfo;

// Although you can access a thread's PseudoStack via TLSInfo::sThreadInfo, we
// also have a second TLS pointer directly to the PseudoStack. Here's why.
//
// - We need to be able to push to and pop from the PseudoStack in
//   AutoProfilerLabel.
//
// - The class functions are hot and must be defined in GeckoProfiler.h so they
//   can be inlined.
//
// - We don't want to expose TLSInfo (and ThreadInfo) in GeckoProfiler.h.
//
// This second pointer isn't ideal, but does provide a way to satisfy those
// constraints. TLSInfo manages it, except for the uses in
// AutoProfilerLabel.
MOZ_THREAD_LOCAL(PseudoStack*) sPseudoStack;

// The name of the main thread.
static const char* const kMainThreadName = "GeckoMain";

////////////////////////////////////////////////////////////////////////
// BEGIN sampling/unwinding code

// The registers used for stack unwinding and a few other sampling purposes.
class Registers
{
public:
  Registers()
    : mPC(nullptr)
    , mSP(nullptr)
    , mFP(nullptr)
    , mLR(nullptr)
#if defined(GP_OS_linux) || defined(GP_OS_android)
    , mContext(nullptr)
#endif
  {}

  // Fills in mContext, mPC, mSP, mFP, and mLR for a synchronous sample.
#if defined(GP_OS_linux) || defined(GP_OS_android)
  void SyncPopulate(ucontext_t* aContext);
#else
  void SyncPopulate();
#endif

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

static void
AddDynamicCodeLocationTag(ProfileBuffer* aBuffer, const char* aStr)
{
  aBuffer->addTag(ProfileBufferEntry::CodeLocation(""));

  size_t strLen = strlen(aStr) + 1;   // +1 for the null terminator
  for (size_t j = 0; j < strLen; ) {
    // Store as many characters in the void* as the platform allows.
    char text[sizeof(void*)];
    size_t len = sizeof(void*) / sizeof(char);
    if (j+len >= strLen) {
      len = strLen - j;
    }
    memcpy(text, &aStr[j], len);
    j += sizeof(void*) / sizeof(char);

    // Cast to *((void**) to pass the text data to a void*.
    aBuffer->addTag(ProfileBufferEntry::EmbeddedString(*((void**)(&text[0]))));
  }
}

static void
AddPseudoEntry(PSLockRef aLock, ProfileBuffer* aBuffer,
               js::ProfileEntry& entry, NotNull<RacyThreadInfo*> aRacyInfo)
{
  MOZ_ASSERT(entry.kind() == js::ProfileEntry::Kind::CPP_NORMAL ||
             entry.kind() == js::ProfileEntry::Kind::JS_NORMAL);

  int lineno = -1;

  // First entry has kind CodeLocation.
  const char* label = entry.label();
  bool includeDynamicString = !ActivePS::FeaturePrivacy(aLock);
  const char* dynamicString =
    includeDynamicString ? entry.dynamicString() : nullptr;

  if (dynamicString) {
    // Create a string that is label + ' ' + annotationString (unless
    // label is an empty string, in which case the ' ' is omitted).
    // Avoid sprintf because it can take a lock on Windows, and this
    // code runs during the profiler's "critical section" as defined
    // in SamplerThread::SuspendAndSampleAndResumeThread.
    char combinedStringBuffer[512];
    const char* locationString;
    size_t labelLength = strlen(label);
    size_t spaceLength = label[0] == '\0' ? 0 : 1;
    size_t dynamicLength = strlen(dynamicString);
    size_t combinedLength = labelLength + spaceLength + dynamicLength;

    if (combinedLength < ArrayLength(combinedStringBuffer)) {
      PodCopy(combinedStringBuffer, label, labelLength);
      if (spaceLength != 0) {
        combinedStringBuffer[labelLength] = ' ';
      }
      PodCopy(&combinedStringBuffer[labelLength + spaceLength], dynamicString, dynamicLength);
      combinedStringBuffer[combinedLength] = '\0';
      locationString = combinedStringBuffer;
    } else {
      locationString = label;
    }

    // Store the string using 1 or more EmbeddedString tags.
    // That will happen to the preceding tag.
    AddDynamicCodeLocationTag(aBuffer, locationString);
    if (entry.isJs()) {
      JSScript* script = entry.script();
      if (script) {
        if (!entry.pc()) {
          // The JIT only allows the top-most entry to have a nullptr pc.
          MOZ_ASSERT(&entry ==
                     &aRacyInfo->entries[aRacyInfo->stackSize() - 1]);
        } else {
          lineno = JS_PCToLineNumber(script, entry.pc());
        }
      }
    } else {
      lineno = entry.line();
    }
  } else {
    aBuffer->addTag(ProfileBufferEntry::CodeLocation(label));

    // XXX: Bug 1010578. Don't assume a CPP entry and try to get the line for
    // js entries as well.
    if (entry.isCpp()) {
      lineno = entry.line();
    }
  }

  if (lineno != -1) {
    aBuffer->addTag(ProfileBufferEntry::LineNumber(lineno));
  }

  aBuffer->addTag(ProfileBufferEntry::Category(uint32_t(entry.category())));
}

// Setting MAX_NATIVE_FRAMES too high risks the unwinder wasting a lot of time
// looping on corrupted stacks.
//
// The PseudoStack frame size is found in PseudoStack::MaxEntries.
static const size_t MAX_NATIVE_FRAMES = 1024;
static const size_t MAX_JS_FRAMES     = 1024;

struct NativeStack
{
  void* mPCs[MAX_NATIVE_FRAMES];
  void* mSPs[MAX_NATIVE_FRAMES];
  size_t mCount;  // Number of entries filled.

  NativeStack()
    : mCount(0)
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

static void
MergeStacksIntoProfile(PSLockRef aLock, bool aIsSynchronous,
                       const ThreadInfo& aThreadInfo, const Registers& aRegs,
                       const NativeStack& aNativeStack, ProfileBuffer* aBuffer)
{
  // WARNING: this function runs within the profiler's "critical section".

  NotNull<RacyThreadInfo*> racyInfo = aThreadInfo.RacyInfo();
  js::ProfileEntry* pseudoEntries = racyInfo->entries;
  uint32_t pseudoCount = racyInfo->stackSize();
  JSContext* context = aThreadInfo.mContext;

  // Make a copy of the JS stack into a JSFrame array. This is necessary since,
  // like the native stack, the JS stack is iterated youngest-to-oldest and we
  // need to iterate oldest-to-youngest when adding entries to aInfo.

  // Synchronous sampling reports an invalid buffer generation to
  // ProfilingFrameIterator to avoid incorrectly resetting the generation of
  // sampled JIT entries inside the JS engine. See note below concerning 'J'
  // entries.
  uint32_t startBufferGen;
  startBufferGen = aIsSynchronous
                 ? UINT32_MAX
                 : aBuffer->mGeneration;
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

      JS::ProfilingFrameIterator jsIter(context, registerState,
                                        startBufferGen);
      for (; jsCount < maxFrames && !jsIter.done(); ++jsIter) {
        // See note below regarding 'J' entries.
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

  // Start the sample with a root entry.
  aBuffer->addTag(ProfileBufferEntry::Sample("(root)"));

  // While the pseudo-stack array is ordered oldest-to-youngest, the JS and
  // native arrays are ordered youngest-to-oldest. We must add frames to aInfo
  // oldest-to-youngest. Thus, iterate over the pseudo-stack forwards and JS
  // and native arrays backwards. Note: this means the terminating condition
  // jsIndex and nativeIndex is being < 0.
  uint32_t pseudoIndex = 0;
  int32_t jsIndex = jsCount - 1;
  int32_t nativeIndex = aNativeStack.mCount - 1;

  uint8_t* lastPseudoCppStackAddr = nullptr;

  // Iterate as long as there is at least one frame remaining.
  while (pseudoIndex != pseudoCount || jsIndex >= 0 || nativeIndex >= 0) {
    // There are 1 to 3 frames available. Find and add the oldest.
    uint8_t* pseudoStackAddr = nullptr;
    uint8_t* jsStackAddr = nullptr;
    uint8_t* nativeStackAddr = nullptr;

    if (pseudoIndex != pseudoCount) {
      js::ProfileEntry& pseudoEntry = pseudoEntries[pseudoIndex];

      if (pseudoEntry.isCpp()) {
        lastPseudoCppStackAddr = (uint8_t*) pseudoEntry.stackAddress();
      }

      // Skip any JS_OSR frames. Such frames are used when the JS interpreter
      // enters a jit frame on a loop edge (via on-stack-replacement, or OSR).
      // To avoid both the pseudoframe and jit frame being recorded (and
      // showing up twice), the interpreter marks the interpreter pseudostack
      // frame as JS_OSR to ensure that it doesn't get counted.
      if (pseudoEntry.kind() == js::ProfileEntry::Kind::JS_OSR) {
          pseudoIndex++;
          continue;
      }

      MOZ_ASSERT(lastPseudoCppStackAddr);
      pseudoStackAddr = lastPseudoCppStackAddr;
    }

    if (jsIndex >= 0) {
      jsStackAddr = (uint8_t*) jsFrames[jsIndex].stackAddress;
    }

    if (nativeIndex >= 0) {
      nativeStackAddr = (uint8_t*) aNativeStack.mSPs[nativeIndex];
    }

    // If there's a native stack entry which has the same SP as a pseudo stack
    // entry, pretend we didn't see the native stack entry.  Ditto for a native
    // stack entry which has the same SP as a JS stack entry.  In effect this
    // means pseudo or JS entries trump conflicting native entries.
    if (nativeStackAddr && (pseudoStackAddr == nativeStackAddr ||
                            jsStackAddr == nativeStackAddr)) {
      nativeStackAddr = nullptr;
      nativeIndex--;
      MOZ_ASSERT(pseudoStackAddr || jsStackAddr);
    }

    // Sanity checks.
    MOZ_ASSERT_IF(pseudoStackAddr, pseudoStackAddr != jsStackAddr &&
                                   pseudoStackAddr != nativeStackAddr);
    MOZ_ASSERT_IF(jsStackAddr, jsStackAddr != pseudoStackAddr &&
                               jsStackAddr != nativeStackAddr);
    MOZ_ASSERT_IF(nativeStackAddr, nativeStackAddr != pseudoStackAddr &&
                                   nativeStackAddr != jsStackAddr);

    // Check to see if pseudoStack frame is top-most.
    if (pseudoStackAddr > jsStackAddr && pseudoStackAddr > nativeStackAddr) {
      MOZ_ASSERT(pseudoIndex < pseudoCount);
      js::ProfileEntry& pseudoEntry = pseudoEntries[pseudoIndex];

      // Pseudo-frames with the CPP_MARKER_FOR_JS kind are just annotations and
      // should not be recorded in the profile.
      if (pseudoEntry.kind() != js::ProfileEntry::Kind::CPP_MARKER_FOR_JS) {
        AddPseudoEntry(aLock, aBuffer, pseudoEntry, racyInfo);
      }
      pseudoIndex++;
      continue;
    }

    // Check to see if JS jit stack frame is top-most
    if (jsStackAddr > nativeStackAddr) {
      MOZ_ASSERT(jsIndex >= 0);
      const JS::ProfilingFrameIterator::Frame& jsFrame = jsFrames[jsIndex];

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
        AddDynamicCodeLocationTag(aBuffer, jsFrame.label);
      } else {
        MOZ_ASSERT(jsFrame.kind == JS::ProfilingFrameIterator::Frame_Ion ||
                   jsFrame.kind == JS::ProfilingFrameIterator::Frame_Baseline);
        aBuffer->addTag(
          ProfileBufferEntry::JitReturnAddr(jsFrames[jsIndex].returnAddress));
      }

      jsIndex--;
      continue;
    }

    // If we reach here, there must be a native stack entry and it must be the
    // greatest entry.
    if (nativeStackAddr) {
      MOZ_ASSERT(nativeIndex >= 0);
      void* addr = (void*)aNativeStack.mPCs[nativeIndex];
      aBuffer->addTag(ProfileBufferEntry::NativeLeafAddr(addr));
    }
    if (nativeIndex >= 0) {
      nativeIndex--;
    }
  }

  // Update the JS context with the current profile sample buffer generation.
  //
  // Do not do this for synchronous samples, which use their own
  // ProfileBuffers instead of the global one in CorePS.
  if (!aIsSynchronous && context) {
    MOZ_ASSERT(aBuffer->mGeneration >= startBufferGen);
    uint32_t lapCount = aBuffer->mGeneration - startBufferGen;
    JS::UpdateJSContextProfilerSampleBufferGen(context, aBuffer->mGeneration,
                                               lapCount);
  }
}

#if defined(GP_OS_windows)
static uintptr_t GetThreadHandle(PlatformData* aData);
#endif

#ifdef USE_NS_STACKWALK
static void
StackWalkCallback(uint32_t aFrameNumber, void* aPC, void* aSP, void* aClosure)
{
  NativeStack* nativeStack = static_cast<NativeStack*>(aClosure);
  MOZ_ASSERT(nativeStack->mCount < MAX_NATIVE_FRAMES);
  nativeStack->mSPs[nativeStack->mCount] = aSP;
  nativeStack->mPCs[nativeStack->mCount] = aPC;
  nativeStack->mCount++;
}

static void
DoNativeBacktrace(PSLockRef aLock, const ThreadInfo& aThreadInfo,
                  const Registers& aRegs, NativeStack& aNativeStack)
{
  // WARNING: this function runs within the profiler's "critical section".

  // Start with the current function. We use 0 as the frame number here because
  // the FramePointerStackWalk() and MozStackWalk() calls below will use 1..N.
  // This is a bit weird but it doesn't matter because StackWalkCallback()
  // doesn't use the frame number argument.
  StackWalkCallback(/* frameNum */ 0, aRegs.mPC, aRegs.mSP, &aNativeStack);

  uint32_t maxFrames = uint32_t(MAX_NATIVE_FRAMES - aNativeStack.mCount);

#if defined(GP_OS_darwin) || (defined(GP_PLAT_x86_windows))
  void* stackEnd = aThreadInfo.StackTop();
  if (aRegs.mFP >= aRegs.mSP && aRegs.mFP <= stackEnd) {
    FramePointerStackWalk(StackWalkCallback, /* skipFrames */ 0, maxFrames,
                          &aNativeStack, reinterpret_cast<void**>(aRegs.mFP),
                          stackEnd);
  }
#else
  // Win64 always omits frame pointers so for it we use the slower
  // MozStackWalk().
  uintptr_t thread = GetThreadHandle(aThreadInfo.GetPlatformData());
  MOZ_ASSERT(thread);
  MozStackWalk(StackWalkCallback, /* skipFrames */ 0, maxFrames, &aNativeStack,
               thread, /* platformData */ nullptr);
#endif
}
#endif

#ifdef USE_EHABI_STACKWALK
static void
DoNativeBacktrace(PSLockRef aLock, const ThreadInfo& aThreadInfo,
                  const Registers& aRegs, NativeStack& aNativeStack)
{
  // WARNING: this function runs within the profiler's "critical section".

  const mcontext_t* mcontext = &aRegs.mContext->uc_mcontext;
  mcontext_t savedContext;
  NotNull<RacyThreadInfo*> racyInfo = aThreadInfo.RacyInfo();

  // The pseudostack contains an "EnterJIT" frame whenever we enter
  // JIT code with profiling enabled; the stack pointer value points
  // the saved registers.  We use this to unwind resume unwinding
  // after encounting JIT code.
  for (uint32_t i = racyInfo->stackSize(); i > 0; --i) {
    // The pseudostack grows towards higher indices, so we iterate
    // backwards (from callee to caller).
    js::ProfileEntry& entry = racyInfo->entries[i - 1];
    if (!entry.isJs() && strcmp(entry.label(), "EnterJIT") == 0) {
      // Found JIT entry frame.  Unwind up to that point (i.e., force
      // the stack walk to stop before the block of saved registers;
      // note that it yields nondecreasing stack pointers), then restore
      // the saved state.
      uint32_t* vSP = reinterpret_cast<uint32_t*>(entry.stackAddress());

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
    EHABIStackWalk(*mcontext, aThreadInfo.StackTop(),
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
DoNativeBacktrace(PSLockRef aLock, const ThreadInfo& aThreadInfo,
                  const Registers& aRegs, NativeStack& aNativeStack)
{
  // WARNING: this function runs within the profiler's "critical section".

  const mcontext_t* mc = &aRegs.mContext->uc_mcontext;

  lul::UnwindRegs startRegs;
  memset(&startRegs, 0, sizeof(startRegs));

#if defined(GP_PLAT_amd64_linux)
  startRegs.xip = lul::TaggedUWord(mc->gregs[REG_RIP]);
  startRegs.xsp = lul::TaggedUWord(mc->gregs[REG_RSP]);
  startRegs.xbp = lul::TaggedUWord(mc->gregs[REG_RBP]);
#elif defined(GP_PLAT_arm_android)
  startRegs.r15 = lul::TaggedUWord(mc->arm_pc);
  startRegs.r14 = lul::TaggedUWord(mc->arm_lr);
  startRegs.r13 = lul::TaggedUWord(mc->arm_sp);
  startRegs.r12 = lul::TaggedUWord(mc->arm_ip);
  startRegs.r11 = lul::TaggedUWord(mc->arm_fp);
  startRegs.r7  = lul::TaggedUWord(mc->arm_r7);
#elif defined(GP_PLAT_x86_linux) || defined(GP_PLAT_x86_android)
  startRegs.xip = lul::TaggedUWord(mc->gregs[REG_EIP]);
  startRegs.xsp = lul::TaggedUWord(mc->gregs[REG_ESP]);
  startRegs.xbp = lul::TaggedUWord(mc->gregs[REG_EBP]);
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
#elif defined(GP_PLAT_arm_android)
    uintptr_t rEDZONE_SIZE = 0;
    uintptr_t start = startRegs.r13.Value() - rEDZONE_SIZE;
#elif defined(GP_PLAT_x86_linux) || defined(GP_PLAT_x86_android)
    uintptr_t rEDZONE_SIZE = 0;
    uintptr_t start = startRegs.xsp.Value() - rEDZONE_SIZE;
#else
#   error "Unknown plat"
#endif
    uintptr_t end = reinterpret_cast<uintptr_t>(aThreadInfo.StackTop());
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

  size_t scannedFramesAllowed = 0;

  size_t scannedFramesAcquired = 0, framePointerFramesAcquired = 0;
  lul::LUL* lul = CorePS::Lul(aLock);
  lul->Unwind(reinterpret_cast<uintptr_t*>(aNativeStack.mPCs),
              reinterpret_cast<uintptr_t*>(aNativeStack.mSPs),
              &aNativeStack.mCount, &framePointerFramesAcquired,
              &scannedFramesAcquired,
              MAX_NATIVE_FRAMES, scannedFramesAllowed,
              &startRegs, &stackImg);

  // Update stats in the LUL stats object.  Unfortunately this requires
  // three global memory operations.
  lul->mStats.mContext += 1;
  lul->mStats.mCFI     += aNativeStack.mCount - 1 - framePointerFramesAcquired -
                          scannedFramesAcquired;
  lul->mStats.mFP      += framePointerFramesAcquired;
  lul->mStats.mScanned += scannedFramesAcquired;
}

#endif

// Writes some components shared by periodic and synchronous profiles to
// ActivePS's ProfileBuffer. (This should only be called from DoSyncSample()
// and DoPeriodicSample().)
static inline void
DoSharedSample(PSLockRef aLock, bool aIsSynchronous,
               ThreadInfo& aThreadInfo, const TimeStamp& aNow,
               const Registers& aRegs, ProfileBuffer::LastSample* aLS,
               ProfileBuffer* aBuffer)
{
  // WARNING: this function runs within the profiler's "critical section".

  MOZ_RELEASE_ASSERT(ActivePS::Exists(aLock));

  aBuffer->addTagThreadId(aThreadInfo.ThreadId(), aLS);

  TimeDuration delta = aNow - CorePS::ProcessStartTime();
  aBuffer->addTag(ProfileBufferEntry::Time(delta.ToMilliseconds()));

  NativeStack nativeStack;
#if defined(HAVE_NATIVE_UNWIND)
  if (ActivePS::FeatureStackWalk(aLock)) {
    DoNativeBacktrace(aLock, aThreadInfo, aRegs, nativeStack);

    MergeStacksIntoProfile(aLock, aIsSynchronous, aThreadInfo, aRegs,
                           nativeStack, aBuffer);
  } else
#endif
  {
    MergeStacksIntoProfile(aLock, aIsSynchronous, aThreadInfo, aRegs,
                           nativeStack, aBuffer);

    if (ActivePS::FeatureLeaf(aLock)) {
      aBuffer->addTag(ProfileBufferEntry::NativeLeafAddr((void*)aRegs.mPC));
    }
  }
}

// Writes the components of a synchronous sample to the given ProfileBuffer.
static void
DoSyncSample(PSLockRef aLock, ThreadInfo& aThreadInfo, const TimeStamp& aNow,
             const Registers& aRegs, ProfileBuffer* aBuffer)
{
  // WARNING: this function runs within the profiler's "critical section".

  DoSharedSample(aLock, /* isSynchronous = */ true, aThreadInfo, aNow, aRegs,
                 /* lastSample = */ nullptr, aBuffer);
}

// Writes the components of a periodic sample to ActivePS's ProfileBuffer.
static void
DoPeriodicSample(PSLockRef aLock, ThreadInfo& aThreadInfo,
                 const TimeStamp& aNow, const Registers& aRegs,
                 int64_t aRSSMemory, int64_t aUSSMemory)
{
  // WARNING: this function runs within the profiler's "critical section".

  ProfileBuffer* buffer = ActivePS::Buffer(aLock);

  DoSharedSample(aLock, /* isSynchronous = */ false, aThreadInfo, aNow, aRegs,
                 &aThreadInfo.LastSample(), buffer);

  ProfilerMarkerLinkedList* pendingMarkersList =
    aThreadInfo.RacyInfo()->GetPendingMarkers();
  while (pendingMarkersList && pendingMarkersList->peek()) {
    ProfilerMarker* marker = pendingMarkersList->popHead();
    buffer->addStoredMarker(marker);
    buffer->addTag(ProfileBufferEntry::Marker(marker));
  }

  ThreadResponsiveness* resp = aThreadInfo.GetThreadResponsiveness();
  if (resp && resp->HasData()) {
    TimeDuration delta = resp->GetUnresponsiveDuration(aNow);
    buffer->addTag(ProfileBufferEntry::Responsiveness(delta.ToMilliseconds()));
  }

  if (aRSSMemory != 0) {
    double rssMemory = static_cast<double>(aRSSMemory);
    buffer->addTag(ProfileBufferEntry::ResidentMemory(rssMemory));
  }

  if (aUSSMemory != 0) {
    double ussMemory = static_cast<double>(aUSSMemory);
    buffer->addTag(ProfileBufferEntry::UnsharedMemory(ussMemory));
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
  aWriter.StringProperty("breakpadId", aLib.GetBreakpadId().c_str());
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
    const CorePS::ThreadVector& liveThreads = CorePS::LiveThreads(aLock);
    for (size_t i = 0; i < liveThreads.size(); i++) {
      ThreadInfo* info = liveThreads.at(i);
      StreamNameAndThreadId(aWriter, info->Name(), info->ThreadId());
    }

    const CorePS::ThreadVector& deadThreads = CorePS::DeadThreads(aLock);
    for (size_t i = 0; i < deadThreads.size(); i++) {
      ThreadInfo* info = deadThreads.at(i);
      StreamNameAndThreadId(aWriter, info->Name(), info->ThreadId());
    }
  }
  aWriter.EndArray();

  aWriter.DoubleProperty(
    "start", static_cast<double>(tasktracer::GetStartTime()));
#endif
}

static void
StreamMetaJSCustomObject(PSLockRef aLock, SpliceableJSONWriter& aWriter)
{
  MOZ_RELEASE_ASSERT(CorePS::Exists() && ActivePS::Exists(aLock));

  aWriter.IntProperty("version", 6);

  // The "startTime" field holds the number of milliseconds since midnight
  // January 1, 1970 GMT. This grotty code computes (Now - (Now -
  // ProcessStartTime)) to convert CorePS::ProcessStartTime() into that form.
  TimeDuration delta = TimeStamp::Now() - CorePS::ProcessStartTime();
  aWriter.DoubleProperty(
    "startTime", static_cast<double>(PR_Now()/1000.0 - delta.ToMilliseconds()));

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
  }
}

#if defined(GP_OS_android)
static void
BuildJavaThreadJSObject(SpliceableJSONWriter& aWriter)
{
  aWriter.StringProperty("name", "Java Main Thread");

  aWriter.StartArrayProperty("samples");
  {
    for (int sampleId = 0; true; sampleId++) {
      bool firstRun = true;
      for (int frameId = 0; true; frameId++) {
        jni::String::LocalRef frameName =
            java::GeckoJavaSampler::GetFrameName(0, sampleId, frameId);

        // When we run out of frames, we stop looping.
        if (!frameName) {
          // If we found at least one frame, we have objects to close.
          if (!firstRun) {
            aWriter.EndArray();
            aWriter.EndObject();
          }
          break;
        }
        // The first time around, open the sample object and frames array.
        if (firstRun) {
          firstRun = false;

          double sampleTime =
              java::GeckoJavaSampler::GetSampleTime(0, sampleId);

          aWriter.StartObjectElement();
            aWriter.DoubleProperty("time", sampleTime);

            aWriter.StartArrayProperty("frames");
        }

        // Add a frame to the sample.
        aWriter.StartObjectElement();
        {
          aWriter.StringProperty("location",
                                 frameName->ToCString().BeginReading());
        }
        aWriter.EndObject();
      }

      // If we found no frames for this sample, we are done.
      if (firstRun) {
        break;
      }
    }
  }
  aWriter.EndArray();
}
#endif

static void
locked_profiler_stream_json_for_this_process(PSLockRef aLock,
                                             SpliceableJSONWriter& aWriter,
                                             double aSinceTime)
{
  LOG("locked_profiler_stream_json_for_this_process");

  MOZ_RELEASE_ASSERT(CorePS::Exists() && ActivePS::Exists(aLock));

  // Put shared library info
  aWriter.StartArrayProperty("libs");
  AppendSharedLibraries(aWriter);
  aWriter.EndArray();

  // Put meta data
  aWriter.StartObjectProperty("meta");
  {
    StreamMetaJSCustomObject(aLock, aWriter);
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
    const CorePS::ThreadVector& liveThreads = CorePS::LiveThreads(aLock);
    for (size_t i = 0; i < liveThreads.size(); i++) {
      ThreadInfo* info = liveThreads.at(i);
      if (!info->IsBeingProfiled()) {
        continue;
      }
      info->StreamJSON(ActivePS::Buffer(aLock), aWriter,
                       CorePS::ProcessStartTime(), aSinceTime);
    }

    const CorePS::ThreadVector& deadThreads = CorePS::DeadThreads(aLock);
    for (size_t i = 0; i < deadThreads.size(); i++) {
      ThreadInfo* info = deadThreads.at(i);
      MOZ_ASSERT(info->IsBeingProfiled());
      info->StreamJSON(ActivePS::Buffer(aLock), aWriter,
                       CorePS::ProcessStartTime(), aSinceTime);
    }

#if defined(GP_OS_android)
    if (ActivePS::FeatureJava(aLock)) {
      java::GeckoJavaSampler::Pause();

      aWriter.Start();
      {
        BuildJavaThreadJSObject(aWriter);
      }
      aWriter.End();

      java::GeckoJavaSampler::Unpause();
    }
#endif
  }
  aWriter.EndArray();
}

bool
profiler_stream_json_for_this_process(SpliceableJSONWriter& aWriter, double aSinceTime)
{
  LOG("profiler_stream_json_for_this_process");

  MOZ_RELEASE_ASSERT(CorePS::Exists());

  PSAutoLock lock(gPSMutex);

  if (!ActivePS::Exists(lock)) {
    return false;
  }

  locked_profiler_stream_json_for_this_process(lock, aWriter, aSinceTime);
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
                                       const ThreadInfo& aThreadInfo,
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

      ActivePS::Buffer(lock)->deleteExpiredStoredMarkers();

      if (!ActivePS::IsPaused(lock)) {
        const CorePS::ThreadVector& liveThreads = CorePS::LiveThreads(lock);
        for (uint32_t i = 0; i < liveThreads.size(); i++) {
          ThreadInfo* info = liveThreads.at(i);

          if (!info->IsBeingProfiled()) {
            // We are not interested in profiling this thread.
            continue;
          }

          // If the thread is asleep and has been sampled before in the same
          // sleep episode, find and copy the previous sample, as that's
          // cheaper than taking a new sample.
          if (info->RacyInfo()->CanDuplicateLastSampleDueToSleep()) {
            bool dup_ok =
              ActivePS::Buffer(lock)->DuplicateLastSample(
                info->ThreadId(), CorePS::ProcessStartTime(),
                info->LastSample());
            if (dup_ok) {
              continue;
            }
          }

          // We only track responsiveness for the main thread.
          if (info->IsMainThread()) {
            info->GetThreadResponsiveness()->Update();
          }

          // We only get the memory measurements once for all live threads.
          int64_t rssMemory = 0;
          int64_t ussMemory = 0;
          if (i == 0 && ActivePS::FeatureMemory(lock)) {
            rssMemory = nsMemoryReporterManager::ResidentFast();
#if defined(GP_OS_linux) || defined(GP_OS_android)
            ussMemory = nsMemoryReporterManager::ResidentUnique();
#endif
          }

          TimeStamp now = TimeStamp::Now();
          SuspendAndSampleAndResumeThread(lock, *info,
                                          [&](const Registers& aRegs) {
            DoPeriodicSample(lock, *info, now, aRegs, rssMemory, ussMemory);
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

// Find the ThreadInfo for the current thread. This should only be called in
// places where TLSInfo can't be used. On success, *aIndexOut is set to the
// index if it is non-null.
static ThreadInfo*
FindLiveThreadInfo(PSLockRef aLock, int* aIndexOut = nullptr)
{
  ThreadInfo* ret = nullptr;
  Thread::tid_t id = Thread::GetCurrentId();
  const CorePS::ThreadVector& liveThreads = CorePS::LiveThreads(aLock);
  for (uint32_t i = 0; i < liveThreads.size(); i++) {
    ThreadInfo* info = liveThreads.at(i);
    if (info->ThreadId() == id) {
      if (aIndexOut) {
        *aIndexOut = i;
      }
      ret = info;
      break;
    }
  }

  return ret;
}

static void
locked_register_thread(PSLockRef aLock, const char* aName, void* aStackTop)
{
  MOZ_RELEASE_ASSERT(CorePS::Exists());

  MOZ_RELEASE_ASSERT(!FindLiveThreadInfo(aLock));

  if (!TLSInfo::Init(aLock)) {
    return;
  }

  ThreadInfo* info = new ThreadInfo(aName, Thread::GetCurrentId(),
                                    NS_IsMainThread(), aStackTop);
  TLSInfo::SetInfo(aLock, info);

  if (ActivePS::Exists(aLock) && ActivePS::ShouldProfileThread(aLock, info)) {
    info->StartProfiling();
    if (ActivePS::FeatureJS(aLock)) {
      // This StartJSSampling() call is on-thread, so we can poll manually to
      // start JS sampling immediately.
      info->StartJSSampling();
      info->PollJSSampling();
    }
  }

  CorePS::LiveThreads(aLock).push_back(info);
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
    NS_DispatchToMainThread(NS_NewRunnableFunction([=] {
      NotifyObservers(aTopic, subject);
    }));
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
locked_profiler_start(PSLockRef aLock, const int aEntries, double aInterval,
                      uint32_t aFeatures,
                      const char** aFilters, uint32_t aFilterCount);

// This basically duplicates AutoProfilerLabel's constructor.
PseudoStack*
MozGlueLabelEnter(const char* aLabel, const char* aDynamicString, void* aSp,
                  uint32_t aLine)
{
  PseudoStack* pseudoStack = sPseudoStack.get();
  if (pseudoStack) {
    pseudoStack->pushCppFrame(aLabel, aDynamicString, aSp, aLine,
                              js::ProfileEntry::Kind::CPP_NORMAL,
                              js::ProfileEntry::Category::OTHER);
  }
  return pseudoStack;
}

// This basically duplicates AutoProfilerLabel's destructor.
void
MozGlueLabelExit(PseudoStack* aPseudoStack)
{
  if (aPseudoStack) {
    aPseudoStack->pop();
  }
}

void
profiler_init(void* aStackTop)
{
  LOG("profiler_init");

  MOZ_RELEASE_ASSERT(!CorePS::Exists());

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
                      0;

  const char* filters[] = { "GeckoMain", "Compositor" };

  if (getenv("MOZ_PROFILER_HELP")) {
    PrintUsageThenExit(0); // terminates execution
  }

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

    if (!getenv("MOZ_PROFILER_STARTUP")) {
      return;
    }

    LOG("- MOZ_PROFILER_STARTUP is set");

    int entries = PROFILER_DEFAULT_ENTRIES;
    const char* startupEntries = getenv("MOZ_PROFILER_STARTUP_ENTRIES");
    if (startupEntries) {
      errno = 0;
      entries = strtol(startupEntries, nullptr, 10);
      if (errno == 0 && entries > 0) {
        LOG("- MOZ_PROFILER_STARTUP_ENTRIES = %d", entries);
      } else {
        PrintUsageThenExit(1);
      }
    }

    double interval = PROFILER_DEFAULT_INTERVAL;
    const char* startupInterval = getenv("MOZ_PROFILER_STARTUP_INTERVAL");
    if (startupInterval) {
      errno = 0;
      interval = PR_strtod(startupInterval, nullptr);
      if (errno == 0 && interval > 0.0 && interval <= 1000.0) {
        LOG("- MOZ_PROFILER_STARTUP_INTERVAL = %f", interval);
      } else {
        PrintUsageThenExit(1);
      }
    }

    locked_profiler_start(lock, entries, interval, features,
                          filters, MOZ_ARRAY_LENGTH(filters));
  }

  // We do this with gPSMutex unlocked. The comment in profiler_stop() explains
  // why.
  NotifyProfilerStarted(PROFILER_DEFAULT_ENTRIES, PROFILER_DEFAULT_INTERVAL,
                        features, filters, MOZ_ARRAY_LENGTH(filters));
}

static void
locked_profiler_save_profile_to_file(PSLockRef aLock, const char* aFilename);

static SamplerThread*
locked_profiler_stop(PSLockRef aLock);

void
profiler_shutdown()
{
  LOG("profiler_shutdown");

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
        locked_profiler_save_profile_to_file(lock, filename);
      }

      samplerThread = locked_profiler_stop(lock);
    }

    CorePS::Destroy(lock);

    // We just destroyed CorePS and the ThreadInfos it contains, so we can
    // clear this thread's TLSInfo.
    TLSInfo::SetInfo(lock, nullptr);

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

UniquePtr<char[]>
profiler_get_profile(double aSinceTime)
{
  LOG("profiler_get_profile");

  MOZ_RELEASE_ASSERT(CorePS::Exists());

  SpliceableChunkedJSONWriter b;
  b.Start(SpliceableJSONWriter::SingleLineStyle);
  {
    if (!profiler_stream_json_for_this_process(b, aSinceTime)) {
      return nullptr;
    }

    // Don't include profiles from other processes because this is a
    // synchronous function.
    b.StartArrayProperty("processes");
    b.EndArray();
  }
  b.End();

  return b.WriteFunc()->CopyData();
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

static void
locked_profiler_save_profile_to_file(PSLockRef aLock, const char* aFilename)
{
  LOG("locked_profiler_save_profile_to_file(%s)", aFilename);

  MOZ_RELEASE_ASSERT(CorePS::Exists() && ActivePS::Exists(aLock));

  std::ofstream stream;
  stream.open(aFilename);
  if (stream.is_open()) {
    SpliceableJSONWriter w(MakeUnique<OStreamJSONWriteFunc>(stream));
    w.Start(SpliceableJSONWriter::SingleLineStyle);
    {
      locked_profiler_stream_json_for_this_process(aLock, w, /* sinceTime */ 0);

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

void
profiler_get_buffer_info_helper(uint32_t* aCurrentPosition,
                                uint32_t* aEntries,
                                uint32_t* aGeneration)
{
  // This function is called by profiler_get_buffer_info(), which has already
  // zeroed the outparams.

  MOZ_RELEASE_ASSERT(CorePS::Exists());

  PSAutoLock lock(gPSMutex);

  if (!ActivePS::Exists(lock)) {
    return;
  }

  *aCurrentPosition = ActivePS::Buffer(lock)->mWritePos;
  *aEntries = ActivePS::Entries(lock);
  *aGeneration = ActivePS::Buffer(lock)->mGeneration;
}

static void
locked_profiler_start(PSLockRef aLock, int aEntries, double aInterval,
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

  // Fall back to the default values if the passed-in values are unreasonable.
  int entries = aEntries > 0 ? aEntries : PROFILER_DEFAULT_ENTRIES;
  double interval = aInterval > 0 ? aInterval : PROFILER_DEFAULT_INTERVAL;

  ActivePS::Create(aLock, entries, interval, aFeatures, aFilters, aFilterCount);

  // Set up profiling for each registered thread, if appropriate.
  Thread::tid_t tid = Thread::GetCurrentId();
  const CorePS::ThreadVector& liveThreads = CorePS::LiveThreads(aLock);
  for (uint32_t i = 0; i < liveThreads.size(); i++) {
    ThreadInfo* info = liveThreads.at(i);

    if (ActivePS::ShouldProfileThread(aLock, info)) {
      info->StartProfiling();
      if (ActivePS::FeatureJS(aLock)) {
        info->StartJSSampling();
        if (info->ThreadId() == tid) {
          // We can manually poll the current thread so it starts sampling
          // immediately.
          info->PollJSSampling();
        }
      }
    }
  }

  // Dead ThreadInfos are deleted in profiler_stop(), and dead ThreadInfos
  // aren't saved when the profiler is inactive. Therefore the dead threads
  // vector should be empty here.
  MOZ_RELEASE_ASSERT(CorePS::DeadThreads(aLock).empty());

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
profiler_start(int aEntries, double aInterval, uint32_t aFeatures,
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
  Thread::tid_t tid = Thread::GetCurrentId();
  CorePS::ThreadVector& liveThreads = CorePS::LiveThreads(aLock);
  for (uint32_t i = 0; i < liveThreads.size(); i++) {
    ThreadInfo* info = liveThreads.at(i);
    if (info->IsBeingProfiled()) {
      if (ActivePS::FeatureJS(aLock)) {
        info->StopJSSampling();
        if (info->ThreadId() == tid) {
          // We can manually poll the current thread so it stops profiling
          // immediately.
          info->PollJSSampling();
        }
      }
      info->StopProfiling();
    }
  }

  // This is where we destroy the ThreadInfos for all dead threads.
  CorePS::ThreadVector& deadThreads = CorePS::DeadThreads(aLock);
  while (deadThreads.size() > 0) {
    delete deadThreads.back();
    deadThreads.pop_back();
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

bool
profiler_is_active()
{
  // This function runs both on and off the main thread.

  MOZ_RELEASE_ASSERT(CorePS::Exists());

  // This function is hot enough that we use RacyFeatures, notActivePS.
  return RacyFeatures::IsActive();
}

void
profiler_register_thread(const char* aName, void* aGuessStackTop)
{
  DEBUG_LOG("profiler_register_thread(%s)", aName);

  MOZ_RELEASE_ASSERT(!NS_IsMainThread());
  MOZ_RELEASE_ASSERT(CorePS::Exists());

  PSAutoLock lock(gPSMutex);

  void* stackTop = GetStackTop(aGuessStackTop);
  locked_register_thread(lock, aName, stackTop);
}

void
profiler_unregister_thread()
{
  MOZ_RELEASE_ASSERT(!NS_IsMainThread());
  MOZ_RELEASE_ASSERT(CorePS::Exists());

  PSAutoLock lock(gPSMutex);

  // We don't call ThreadInfo::StopJSSampling() here; there's no point doing
  // that for a JS thread that is in the process of disappearing.

  int i;
  ThreadInfo* info = FindLiveThreadInfo(lock, &i);
  MOZ_RELEASE_ASSERT(info == TLSInfo::Info(lock));
  if (info) {
    DEBUG_LOG("profiler_unregister_thread: %s", info->Name());
    if (ActivePS::Exists(lock) && info->IsBeingProfiled()) {
      CorePS::DeadThreads(lock).push_back(info);
    } else {
      delete info;
    }
    CorePS::ThreadVector& liveThreads = CorePS::LiveThreads(lock);
    liveThreads.erase(liveThreads.begin() + i);

    // Whether or not we just destroyed the ThreadInfo or transferred it to the
    // dead thread vector, we no longer need to access it via TLS.
    TLSInfo::SetInfo(lock, nullptr);

  } else {
    // There are two ways FindLiveThreadInfo() might have failed.
    //
    // - TLSInfo::Init() failed in locked_register_thread().
    //
    // - We've already called profiler_unregister_thread() for this thread.
    //   (Whether or not it should, this does happen in practice.)
    //
    // Either way, TLSInfo should be empty.
    MOZ_RELEASE_ASSERT(!TLSInfo::Info(lock));
  }
}

void
profiler_thread_sleep()
{
  // This function runs both on and off the main thread.

  MOZ_RELEASE_ASSERT(CorePS::Exists());

  RacyThreadInfo* racyInfo = TLSInfo::RacyInfo();
  if (!racyInfo) {
    return;
  }

  racyInfo->SetSleeping();
}

void
profiler_thread_wake()
{
  // This function runs both on and off the main thread.

  MOZ_RELEASE_ASSERT(CorePS::Exists());

  RacyThreadInfo* racyInfo = TLSInfo::RacyInfo();
  if (!racyInfo) {
    return;
  }

  racyInfo->SetAwake();
}

bool
profiler_thread_is_sleeping()
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  MOZ_RELEASE_ASSERT(CorePS::Exists());

  RacyThreadInfo* racyInfo = TLSInfo::RacyInfo();
  if (!racyInfo) {
    return false;
  }
  return racyInfo->IsSleeping();
}

void
profiler_js_interrupt_callback()
{
  // This function runs on JS threads being sampled.

  MOZ_RELEASE_ASSERT(CorePS::Exists());

  PSAutoLock lock(gPSMutex);

  ThreadInfo* info = TLSInfo::Info(lock);
  if (!info) {
    return;
  }

  info->PollJSSampling();
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

  ThreadInfo* info = TLSInfo::Info(lock);
  if (!info) {
    MOZ_ASSERT(info);
    return nullptr;
  }

  Thread::tid_t tid = Thread::GetCurrentId();

  TimeStamp now = TimeStamp::Now();

  Registers regs;

#if defined(HAVE_NATIVE_UNWIND)
#if defined(GP_OS_linux) || defined(GP_OS_android)
  ucontext_t context;
  regs.SyncPopulate(&context);
#else
  regs.SyncPopulate();
#endif
#endif

  ProfileBuffer* buffer = new ProfileBuffer(PROFILER_GET_BACKTRACE_ENTRIES);

  DoSyncSample(lock, *info, now, regs, buffer);

  return UniqueProfilerBacktrace(
    new ProfilerBacktrace("SyncProfile", tid, buffer));
}

void
ProfilerBacktraceDestructor::operator()(ProfilerBacktrace* aBacktrace)
{
  delete aBacktrace;
}

// Fill the output buffer with the following pattern:
// "Label 1" "\0" "Label 2" "\0" ... "Label N" "\0" "\0"
// TODO: use the unwinder instead of pseudo stack.
void
profiler_get_backtrace_noalloc(char *output, size_t outputSize)
{
  MOZ_RELEASE_ASSERT(CorePS::Exists());

  MOZ_ASSERT(outputSize >= 2);
  char *bound = output + outputSize - 2;
  output[0] = output[1] = '\0';

  PSAutoLock lock(gPSMutex);

  if (!ActivePS::Exists(lock)) {
    return;
  }

  PseudoStack* pseudoStack = TLSInfo::Stack();
  if (!pseudoStack) {
    return;
  }

  bool includeDynamicString = !ActivePS::FeaturePrivacy(lock);

  js::ProfileEntry* pseudoEntries = pseudoStack->entries;
  uint32_t pseudoCount = pseudoStack->stackSize();

  for (uint32_t i = 0; i < pseudoCount; i++) {
    const char* label = pseudoEntries[i].label();
    const char* dynamicString =
      includeDynamicString ? pseudoEntries[i].dynamicString() : nullptr;
    size_t labelLength = strlen(label);
    if (dynamicString) {
      // Put the label, maybe a space, and the dynamic string into output.
      size_t spaceLength = label[0] == '\0' ? 0 : 1;
      size_t dynamicStringLength = strlen(dynamicString);
      if (output + labelLength + spaceLength + dynamicStringLength >= bound) {
        break;
      }
      strcpy(output, label);
      output += labelLength;
      if (spaceLength != 0) {
        *output++ = ' ';
      }
      strcpy(output, dynamicString);
      output += dynamicStringLength;
    } else {
      // Only put the label into output.
      if (output + labelLength >= bound) {
        break;
      }
      strcpy(output, label);
      output += labelLength;
    }
    *output++ = '\0';
    *output = '\0';
  }
}

static void
racy_profiler_add_marker(const char* aMarkerName,
                         ProfilerMarkerPayload* aPayload)
{
  MOZ_RELEASE_ASSERT(CorePS::Exists());

  // aPayload must be freed if we return early.
  UniquePtr<ProfilerMarkerPayload> payload(aPayload);

  // We don't assert that RacyFeatures::IsActiveWithoutPrivacy() is true here,
  // because it's possible that the result has changed since we tested it in
  // the caller.
  //
  // Because of this imprecision it's possible to miss a marker or record one
  // we shouldn't. Either way is not a big deal.

  RacyThreadInfo* racyInfo = TLSInfo::RacyInfo();
  if (!racyInfo) {
    return;
  }

  TimeStamp origin = (payload && !payload->GetStartTime().IsNull())
                   ? payload->GetStartTime()
                   : TimeStamp::Now();
  TimeDuration delta = origin - CorePS::ProcessStartTime();
  racyInfo->AddPendingMarker(aMarkerName, payload.release(),
                             delta.ToMilliseconds());
}

void
profiler_add_marker(const char* aMarkerName, ProfilerMarkerPayload* aPayload)
{
  MOZ_RELEASE_ASSERT(CorePS::Exists());

  // aPayload must be freed if we return early.
  UniquePtr<ProfilerMarkerPayload> payload(aPayload);

  // This function is hot enough that we use RacyFeatures, notActivePS.
  if (!RacyFeatures::IsActiveWithoutPrivacy()) {
    return;
  }

  racy_profiler_add_marker(aMarkerName, payload.release());
}

void
profiler_tracing(const char* aCategory, const char* aMarkerName,
                 TracingKind aKind)
{
  MOZ_RELEASE_ASSERT(CorePS::Exists());

  // This function is hot enough that we use RacyFeatures, notActivePS.
  if (!RacyFeatures::IsActiveWithoutPrivacy()) {
    return;
  }

  auto payload = new ProfilerMarkerTracing(aCategory, aKind);
  racy_profiler_add_marker(aMarkerName, payload);
}

void
profiler_tracing(const char* aCategory, const char* aMarkerName,
                 UniqueProfilerBacktrace aCause, TracingKind aKind)
{
  MOZ_RELEASE_ASSERT(CorePS::Exists());

  // This function is hot enough that we use RacyFeatures, notActivePS.
  if (!RacyFeatures::IsActiveWithoutPrivacy()) {
    return;
  }

  auto payload = new ProfilerMarkerTracing(aCategory, aKind, Move(aCause));
  racy_profiler_add_marker(aMarkerName, payload);
}

void
profiler_log(const char* aStr)
{
  profiler_tracing("log", aStr);
}

PseudoStack*
profiler_get_pseudo_stack()
{
  return TLSInfo::Stack();
}

void
profiler_set_js_context(JSContext* aCx)
{
  MOZ_ASSERT(aCx);

  PSAutoLock lock(gPSMutex);

  ThreadInfo* info = TLSInfo::Info(lock);
  if (!info) {
    return;
  }

  info->SetJSContext(aCx);
}

void
profiler_clear_js_context()
{
  MOZ_RELEASE_ASSERT(CorePS::Exists());

  PSAutoLock lock(gPSMutex);

  ThreadInfo* info = TLSInfo::Info(lock);
  if (!info || !info->mContext) {
    return;
  }

  // On JS shut down, flush the current buffer as stringifying JIT samples
  // requires a live JSContext.

  if (ActivePS::Exists(lock)) {
    // Flush this thread's ThreadInfo, if it is being profiled.
    if (info->IsBeingProfiled()) {
      info->FlushSamplesAndMarkers(ActivePS::Buffer(lock),
                                   CorePS::ProcessStartTime());
    }
  }

  // We don't call info->StopJSSampling() here; there's no point doing that for
  // a JS thread that is in the process of disappearing.

  info->mContext = nullptr;
}

int
profiler_current_thread_id()
{
  return Thread::GetCurrentId();
}

// NOTE: The callback function passed in will be called while the target thread
// is paused. Doing stuff in this function like allocating which may try to
// claim locks is a surefire way to deadlock.
void
profiler_suspend_and_sample_thread(
  int aThreadId,
  const std::function<void(void**, size_t)>& aCallback,
  bool aSampleNative /* = true */)
{
  // Allocate the space for the native stack
  NativeStack nativeStack;

  // Lock the profiler mutex
  PSAutoLock lock(gPSMutex);

  const CorePS::ThreadVector& liveThreads = CorePS::LiveThreads(lock);
  for (uint32_t i = 0; i < liveThreads.size(); i++) {
    ThreadInfo* info = liveThreads.at(i);

    if (info->ThreadId() == aThreadId) {
      // Suspend, sample, and then resume the target thread.
      Sampler sampler(lock);
      sampler.SuspendAndSampleAndResumeThread(lock, *info,
                                              [&](const Registers& aRegs) {
        // The target thread is now suspended. Collect a native backtrace, and
        // call the callback.
#if defined(HAVE_NATIVE_UNWIND)
        if (aSampleNative) {
          DoNativeBacktrace(lock, *info, aRegs, nativeStack);
        }
#endif
        aCallback(nativeStack.mPCs, nativeStack.mCount);
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
