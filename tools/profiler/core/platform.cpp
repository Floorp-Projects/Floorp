/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
#include "ProfilerIOInterposeObserver.h"
#include "mozilla/StackWalk.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/ThreadLocal.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/Sprintf.h"
#include "mozilla/StaticPtr.h"
#include "PseudoStack.h"
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
#include "mozilla/Services.h"
#include "nsThreadUtils.h"
#include "ProfilerMarkers.h"
#include "shared-libraries.h"
#include "prtime.h"

#ifdef MOZ_TASK_TRACER
#include "GeckoTaskTracer.h"
#endif

#if defined(PROFILE_JAVA)
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

#if defined(GP_OS_windows)
typedef CONTEXT tick_context_t;
#elif defined(GP_OS_darwin)
typedef void tick_context_t;   // this type isn't used meaningfully on Mac
#elif defined(GP_OS_linux) || defined(GP_OS_android)
#include <ucontext.h>
typedef ucontext_t tick_context_t;
#endif

using namespace mozilla;

mozilla::LazyLogModule gProfilerLog("prof");

#if defined(PROFILE_JAVA)
class GeckoJavaSampler : public mozilla::java::GeckoJavaSampler::Natives<GeckoJavaSampler>
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

class SamplerThread;

// Per-thread state.
MOZ_THREAD_LOCAL(PseudoStack *) tlsPseudoStack;

class PSMutex : public mozilla::StaticMutex {};

typedef mozilla::BaseAutoLock<PSMutex> PSAutoLock;

// Only functions that take a PSLockRef arg can modify this class's fields.
typedef const PSAutoLock& PSLockRef;

// This class contains most of the profiler's global state. gPS is the single
// instance. Most profile operations can't do anything useful when gPS is not
// instantiated, so we release-assert its non-nullness in all such operations.
//
// Accesses to gPS are guarded by gPSMutex. Every getter and setter takes a
// PSAutoLock reference as an argument as proof that the gPSMutex is currently
// locked. This makes it clear when gPSMutex is locked and helps avoid
// accidental unlocked accesses to global state. There are ways to circumvent
// this mechanism, but please don't do so without *very* good reason and a
// detailed explanation.
//
// Other from the lock protection, this class is essentially a thin wrapper and
// contains very little "smarts" itself.
//
class PS
{
public:
  typedef std::vector<ThreadInfo*> ThreadVector;

  PS()
    : mEntries(0)
    , mInterval(0)
    , mFeatureDisplayListDump(false)
    , mFeatureGPU(false)
    , mFeatureJava(false)
    , mFeatureJS(false)
    , mFeatureLayersDump(false)
    , mFeatureLeaf(false)
    , mFeatureMainThreadIO(false)
    , mFeatureMemory(false)
    , mFeaturePrivacy(false)
    , mFeatureRestyle(false)
    , mFeatureStackWalk(false)
    , mFeatureTaskTracer(false)
    , mFeatureThreads(false)
    , mBuffer(nullptr)
    , mIsPaused(false)
#if defined(GP_OS_linux)
    , mWasPaused(false)
#endif
    , mSamplerThread(nullptr)
#ifdef USE_LUL_STACKWALK
    , mLUL(nullptr)
#endif
    , mInterposeObserver(nullptr)
  {}

  #define GET_AND_SET(type_, name_) \
    type_ name_(PSLockRef) const { return m##name_; } \
    void Set##name_(PSLockRef, type_ a##name_) { m##name_ = a##name_; }

  GET_AND_SET(TimeStamp, ProcessStartTime)

  GET_AND_SET(int, Entries)

  GET_AND_SET(double, Interval)

  Vector<std::string>& Features(PSLockRef) { return mFeatures; }

  Vector<std::string>& Filters(PSLockRef) { return mFilters; }

  GET_AND_SET(bool, FeatureDisplayListDump)
  GET_AND_SET(bool, FeatureGPU)
  GET_AND_SET(bool, FeatureJava)
  GET_AND_SET(bool, FeatureJS)
  GET_AND_SET(bool, FeatureLayersDump)
  GET_AND_SET(bool, FeatureLeaf)
  GET_AND_SET(bool, FeatureMainThreadIO)
  GET_AND_SET(bool, FeatureMemory)
  GET_AND_SET(bool, FeaturePrivacy)
  GET_AND_SET(bool, FeatureRestyle)
  GET_AND_SET(bool, FeatureStackWalk)
  GET_AND_SET(bool, FeatureTaskTracer)
  GET_AND_SET(bool, FeatureThreads)

  GET_AND_SET(ProfileBuffer*, Buffer)

  ThreadVector& LiveThreads(PSLockRef) { return mLiveThreads; }
  ThreadVector& DeadThreads(PSLockRef) { return mDeadThreads; }

  static bool IsActive(PSLockRef) { return sActivityGeneration > 0; }
  static uint32_t ActivityGeneration(PSLockRef) { return sActivityGeneration; }
  static void SetInactive(PSLockRef) { sActivityGeneration = 0; }
  static void SetActive(PSLockRef)
  {
    sActivityGeneration = sNextActivityGeneration;
    // On overflow, reset to 1 instead of 0, because 0 means inactive.
    sNextActivityGeneration = (sNextActivityGeneration == 0xffffffff)
                            ? 1
                            : sNextActivityGeneration + 1;
  }

  GET_AND_SET(bool, IsPaused)

#if defined(GP_OS_linux)
  GET_AND_SET(bool, WasPaused)
#endif

  GET_AND_SET(class SamplerThread*, SamplerThread)

#ifdef USE_LUL_STACKWALK
  GET_AND_SET(lul::LUL*, LUL)
#endif

  GET_AND_SET(mozilla::ProfilerIOInterposeObserver*, InterposeObserver)

  #undef GET_AND_SET

private:
  // The time that the process started.
  mozilla::TimeStamp mProcessStartTime;

  // The number of entries in mBuffer. Zeroed when the profiler is inactive.
  int mEntries;

  // The interval between samples, measured in milliseconds. Zeroed when the
  // profiler is inactive.
  double mInterval;

  // The profile features that are enabled. Cleared when the profiler is
  // inactive.
  Vector<std::string> mFeatures;

  // Substrings of names of threads we want to profile. Cleared when the
  // profiler is inactive
  Vector<std::string> mFilters;

  // Configuration flags derived from mFeatures. Cleared when the profiler is
  // inactive.
  bool mFeatureDisplayListDump;
  bool mFeatureGPU;
  bool mFeatureJava;
  bool mFeatureJS;
  bool mFeatureLayersDump;
  bool mFeatureLeaf;
  bool mFeatureMainThreadIO;
  bool mFeatureMemory;
  bool mFeaturePrivacy;
  bool mFeatureRestyle;
  bool mFeatureStackWalk;
  bool mFeatureTaskTracer;
  bool mFeatureThreads;

  // The buffer into which all samples are recorded. Always used in conjunction
  // with mLiveThreads and mDeadThreads. Null when the profiler is inactive.
  ProfileBuffer* mBuffer;

  // Info on all the registered threads, both live and dead. ThreadIds in
  // mLiveThreads are unique. ThreadIds in mDeadThreads may not be, because
  // ThreadIds can be reused. IsBeingProfiled() is true for all ThreadInfos in
  // mDeadThreads because we don't hold on to ThreadInfos for non-profiled dead
  // threads.
  ThreadVector mLiveThreads;
  ThreadVector mDeadThreads;

  // Is the profiler active? The obvious way to track this is with a bool,
  // sIsActive, but then we could have the following scenario.
  //
  // - profiler_stop() locks gPSMutex, zeroes sIsActive, unlocks gPSMutex,
  //   deletes the SamplerThread (which does a join).
  //
  // - profiler_start() runs on a different thread, locks gPSMutex, sets
  //   sIsActive, unlocks gPSMutex -- all before the join completes.
  //
  // - SamplerThread::Run() locks gPSMutex, sees that sIsActive is set, and
  //   continues as if the start/stop pair didn't occur. Also profiler_stop()
  //   is stuck, unable to finish.
  //
  // Instead, we use an integer, sActivityGeneration; zero means inactive,
  // non-zero means active. Furthermore, each time the profiler is activated
  // the value increases by 1 (as tracked by sNextActivityGeneration). This
  // allows SamplerThread::Run() to distinguish the current activation from any
  // subsequent activations.
  //
  // These variables are static because they can be referred to by
  // SamplerThread::Run() even after gPS has been destroyed by
  // profiler_shutdown().
  static uint32_t sActivityGeneration;
  static uint32_t sNextActivityGeneration;

  // Is the profiler paused? False when the profiler is inactive.
  bool mIsPaused;

#if defined(GP_OS_linux)
  // Used to record whether the profiler was paused just before forking. False
  // at all times except just before/after forking.
  bool mWasPaused;
#endif

  // The current sampler thread. Null when the profiler is inactive.
  class SamplerThread* mSamplerThread;

#ifdef USE_LUL_STACKWALK
  // LUL's state. Null prior to the first activation, non-null thereafter.
  lul::LUL* mLUL;
#endif

  // The interposer that records main thread I/O. Null when the profiler is
  // inactive.
  mozilla::ProfilerIOInterposeObserver* mInterposeObserver;
};

uint32_t PS::sActivityGeneration = 0;
uint32_t PS::sNextActivityGeneration = 1;

// The core profiler state. Null at process startup, it is set to a non-null
// value in profiler_init() and stays that way until profiler_shutdown() is
// called. Therefore it can be checked to determine if the profiler has been
// initialized but not yet shut down.
static PS* gPS = nullptr;

// The mutex that guards accesses to gPS.
static PSMutex gPSMutex;

// The name of the main thread.
static const char* const kMainThreadName = "GeckoMain";

static bool
CanNotifyObservers()
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

#if defined(GP_OS_android)
  // Android ANR reporter uses the profiler off the main thread.
  return NS_IsMainThread();
#else
  return true;
#endif
}

////////////////////////////////////////////////////////////////////////
// BEGIN tick/unwinding code

// TickSample contains all the information needed by Tick(). Some of it is
// pointers to long-lived things, and some of it is sampled just before the
// call to Tick().
class TickSample {
public:
  // This constructor is for periodic samples, i.e. those performed in response
  // to a timer firing. Periodic samples are performed off-thread, i.e. the
  // SamplerThread samples the thread in question.
  TickSample(ThreadInfo* aThreadInfo, int64_t aRSSMemory, int64_t aUSSMemory)
    : mIsSynchronous(false)
    , mTimeStamp(mozilla::TimeStamp::Now())
    , mThreadId(aThreadInfo->ThreadId())
    , mPseudoStack(aThreadInfo->Stack())
    , mStackTop(aThreadInfo->StackTop())
    , mLastSample(&aThreadInfo->LastSample())
    , mPlatformData(aThreadInfo->GetPlatformData())
    , mResponsiveness(aThreadInfo->GetThreadResponsiveness())
    , mRSSMemory(aRSSMemory)    // may be zero
    , mUSSMemory(aUSSMemory)    // may be zero
#if !defined(GP_OS_darwin)
    , mContext(nullptr)
#endif
    , mPC(nullptr)
    , mSP(nullptr)
    , mFP(nullptr)
    , mLR(nullptr)
  {}

  // This constructor is for synchronous samples, i.e. those performed in
  // response to an explicit sampling request via the API. Synchronous samples
  // are performed on-thread, i.e. the thread samples itself.
  TickSample(NotNull<PseudoStack*> aPseudoStack, PlatformData* aPlatformData)
    : mIsSynchronous(true)
    , mTimeStamp(mozilla::TimeStamp::Now())
    , mThreadId(Thread::GetCurrentId())
    , mPseudoStack(aPseudoStack)
    , mStackTop(nullptr)
    , mLastSample(nullptr)
    , mPlatformData(aPlatformData)
    , mResponsiveness(nullptr)
    , mRSSMemory(0)
    , mUSSMemory(0)
#if !defined(GP_OS_darwin)
    , mContext(nullptr)
#endif
    , mPC(nullptr)
    , mSP(nullptr)
    , mFP(nullptr)
    , mLR(nullptr)
  {}

  // Fills in mContext, mPC, mSP, mFP, and mLR for a synchronous sample.
  void PopulateContext(tick_context_t* aContext);

  // False for periodic samples, true for synchronous samples.
  const bool mIsSynchronous;

  const mozilla::TimeStamp mTimeStamp;

  const int mThreadId;

  const NotNull<PseudoStack*> mPseudoStack;

  void* const mStackTop;

  ProfileBuffer::LastSample* const mLastSample;   // may be null

  PlatformData* const mPlatformData;

  ThreadResponsiveness* const mResponsiveness;    // may be null

  const int64_t mRSSMemory;                       // may be zero
  const int64_t mUSSMemory;                       // may be zero

  // The remaining fields are filled in, after construction, by
  // SamplerThread::SuspendAndSampleAndResume() for periodic samples, and
  // PopulateContext() for synchronous samples. They are filled in separately
  // from the other fields in this class because the code that fills them in is
  // platform-specific.
#if !defined(GP_OS_darwin)
  void* mContext; // The context from the signal handler.
#endif
  Address mPC;    // Instruction pointer.
  Address mSP;    // Stack pointer.
  Address mFP;    // Frame pointer.
  Address mLR;    // ARM link register.
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

static const int SAMPLER_MAX_STRING_LENGTH = 128;

static void
AddPseudoEntry(PSLockRef aLock, ProfileBuffer* aBuffer,
               volatile js::ProfileEntry& entry, PseudoStack* stack)
{
  // Pseudo-frames with the BEGIN_PSEUDO_JS flag are just annotations and
  // should not be recorded in the profile.
  if (entry.hasFlag(js::ProfileEntry::BEGIN_PSEUDO_JS)) {
    return;
  }

  int lineno = -1;

  // First entry has kind CodeLocation. Check for magic pointer bit 1 to
  // indicate copy.
  const char* sampleLabel = entry.label();
  bool includeDynamicString = !gPS->FeaturePrivacy(aLock);
  const char* dynamicString =
    includeDynamicString ? entry.getDynamicString() : nullptr;
  char combinedStringBuffer[SAMPLER_MAX_STRING_LENGTH];

  if (entry.isCopyLabel() || dynamicString) {
    if (dynamicString) {
      int bytesWritten =
        SprintfLiteral(combinedStringBuffer, "%s %s", sampleLabel, dynamicString);
      if (bytesWritten > 0) {
        sampleLabel = combinedStringBuffer;
      }
    }
    // Store the string using 1 or more EmbeddedString tags.
    // That will happen to the preceding tag.
    AddDynamicCodeLocationTag(aBuffer, sampleLabel);
    if (entry.isJs()) {
      JSScript* script = entry.script();
      if (script) {
        if (!entry.pc()) {
          // The JIT only allows the top-most entry to have a nullptr pc.
          MOZ_ASSERT(&entry == &stack->mStack[stack->stackSize() - 1]);
        } else {
          lineno = JS_PCToLineNumber(script, entry.pc());
        }
      }
    } else {
      lineno = entry.line();
    }
  } else {
    aBuffer->addTag(ProfileBufferEntry::CodeLocation(sampleLabel));

    // XXX: Bug 1010578. Don't assume a CPP entry and try to get the line for
    // js entries as well.
    if (entry.isCpp()) {
      lineno = entry.line();
    }
  }

  if (lineno != -1) {
    aBuffer->addTag(ProfileBufferEntry::LineNumber(lineno));
  }

  uint32_t category = entry.category();
  MOZ_ASSERT(!(category & js::ProfileEntry::IS_CPP_ENTRY));
  MOZ_ASSERT(!(category & js::ProfileEntry::FRAME_LABEL_COPY));

  if (category) {
    aBuffer->addTag(ProfileBufferEntry::Category((int)category));
  }
}

struct NativeStack
{
  void** pc_array;
  void** sp_array;
  size_t size;
  size_t count;
};

mozilla::Atomic<bool> WALKING_JS_STACK(false);

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
MergeStacksIntoProfile(PSLockRef aLock, ProfileBuffer* aBuffer,
                       const TickSample& aSample, NativeStack& aNativeStack)
{
  NotNull<PseudoStack*> pseudoStack = aSample.mPseudoStack;
  volatile js::ProfileEntry* pseudoFrames = pseudoStack->mStack;
  uint32_t pseudoCount = pseudoStack->stackSize();

  // Make a copy of the JS stack into a JSFrame array. This is necessary since,
  // like the native stack, the JS stack is iterated youngest-to-oldest and we
  // need to iterate oldest-to-youngest when adding entries to aInfo.

  // Synchronous sampling reports an invalid buffer generation to
  // ProfilingFrameIterator to avoid incorrectly resetting the generation of
  // sampled JIT entries inside the JS engine. See note below concerning 'J'
  // entries.
  uint32_t startBufferGen;
  startBufferGen = aSample.mIsSynchronous
                 ? UINT32_MAX
                 : aBuffer->mGeneration;
  uint32_t jsCount = 0;
  JS::ProfilingFrameIterator::Frame jsFrames[1000];

  // Only walk jit stack if profiling frame iterator is turned on.
  if (pseudoStack->mContext &&
      JS::IsProfilingEnabledForContext(pseudoStack->mContext)) {
    AutoWalkJSStack autoWalkJSStack;
    const uint32_t maxFrames = mozilla::ArrayLength(jsFrames);

    if (autoWalkJSStack.walkAllowed) {
      JS::ProfilingFrameIterator::RegisterState registerState;
      registerState.pc = aSample.mPC;
      registerState.sp = aSample.mSP;
      registerState.lr = aSample.mLR;
      registerState.fp = aSample.mFP;

      JS::ProfilingFrameIterator jsIter(pseudoStack->mContext,
                                        registerState,
                                        startBufferGen);
      for (; jsCount < maxFrames && !jsIter.done(); ++jsIter) {
        // See note below regarding 'J' entries.
        if (aSample.mIsSynchronous || jsIter.isWasm()) {
          uint32_t extracted =
            jsIter.extractStack(jsFrames, jsCount, maxFrames);
          jsCount += extracted;
          if (jsCount == maxFrames) {
            break;
          }
        } else {
          mozilla::Maybe<JS::ProfilingFrameIterator::Frame> frame =
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
  int32_t nativeIndex = aNativeStack.count - 1;

  uint8_t* lastPseudoCppStackAddr = nullptr;

  // Iterate as long as there is at least one frame remaining.
  while (pseudoIndex != pseudoCount || jsIndex >= 0 || nativeIndex >= 0) {
    // There are 1 to 3 frames available. Find and add the oldest.
    uint8_t* pseudoStackAddr = nullptr;
    uint8_t* jsStackAddr = nullptr;
    uint8_t* nativeStackAddr = nullptr;

    if (pseudoIndex != pseudoCount) {
      volatile js::ProfileEntry& pseudoFrame = pseudoFrames[pseudoIndex];

      if (pseudoFrame.isCpp()) {
        lastPseudoCppStackAddr = (uint8_t*) pseudoFrame.stackAddress();
      }

      // Skip any pseudo-stack JS frames which are marked isOSR. Pseudostack
      // frames are marked isOSR when the JS interpreter enters a jit frame on
      // a loop edge (via on-stack-replacement, or OSR). To avoid both the
      // pseudoframe and jit frame being recorded (and showing up twice), the
      // interpreter marks the interpreter pseudostack entry with the OSR flag
      // to ensure that it doesn't get counted.
      if (pseudoFrame.isJs() && pseudoFrame.isOSR()) {
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
      nativeStackAddr = (uint8_t*) aNativeStack.sp_array[nativeIndex];
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
      volatile js::ProfileEntry& pseudoFrame = pseudoFrames[pseudoIndex];
      AddPseudoEntry(aLock, aBuffer, pseudoFrame, pseudoStack);
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
      if (aSample.mIsSynchronous ||
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
      void* addr = (void*)aNativeStack.pc_array[nativeIndex];
      aBuffer->addTag(ProfileBufferEntry::NativeLeafAddr(addr));
    }
    if (nativeIndex >= 0) {
      nativeIndex--;
    }
  }

  // Update the JS context with the current profile sample buffer generation.
  //
  // Do not do this for synchronous samples, which use their own
  // ProfileBuffers instead of the global one in PS.
  if (!aSample.mIsSynchronous && pseudoStack->mContext) {
    MOZ_ASSERT(aBuffer->mGeneration >= startBufferGen);
    uint32_t lapCount = aBuffer->mGeneration - startBufferGen;
    JS::UpdateJSContextProfilerSampleBufferGen(pseudoStack->mContext,
                                               aBuffer->mGeneration,
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
  MOZ_ASSERT(nativeStack->count < nativeStack->size);
  nativeStack->sp_array[nativeStack->count] = aSP;
  nativeStack->pc_array[nativeStack->count] = aPC;
  nativeStack->count++;
}

static void
DoNativeBacktrace(PSLockRef aLock, ProfileBuffer* aBuffer,
                  const TickSample& aSample)
{
  void* pc_array[1000];
  void* sp_array[1000];
  NativeStack nativeStack = {
    pc_array,
    sp_array,
    mozilla::ArrayLength(pc_array),
    0
  };

  // Start with the current function. We use 0 as the frame number here because
  // the FramePointerStackWalk() and MozStackWalk() calls below will use 1..N.
  // This is a bit weird but it doesn't matter because StackWalkCallback()
  // doesn't use the frame number argument.
  StackWalkCallback(/* frameNum */ 0, aSample.mPC, aSample.mSP, &nativeStack);

  uint32_t maxFrames = uint32_t(nativeStack.size - nativeStack.count);

#if defined(GP_OS_darwin) || (defined(GP_PLAT_x86_windows))
  void* stackEnd = aSample.mStackTop;
  if (aSample.mFP >= aSample.mSP && aSample.mFP <= stackEnd) {
    FramePointerStackWalk(StackWalkCallback, /* skipFrames */ 0, maxFrames,
                          &nativeStack, reinterpret_cast<void**>(aSample.mFP),
                          stackEnd);
  }
#else
  // Win64 always omits frame pointers so for it we use the slower
  // MozStackWalk().
  uintptr_t thread = GetThreadHandle(aSample.mPlatformData);
  MOZ_ASSERT(thread);
  MozStackWalk(StackWalkCallback, /* skipFrames */ 0, maxFrames, &nativeStack,
               thread, /* platformData */ nullptr);
#endif

  MergeStacksIntoProfile(aLock, aBuffer, aSample, nativeStack);
}
#endif

#ifdef USE_EHABI_STACKWALK
static void
DoNativeBacktrace(PSLockRef aLock, ProfileBuffer* aBuffer,
                  const TickSample& aSample)
{
  void* pc_array[1000];
  void* sp_array[1000];
  NativeStack nativeStack = {
    pc_array,
    sp_array,
    mozilla::ArrayLength(pc_array),
    0
  };

  const mcontext_t* mcontext =
    &reinterpret_cast<ucontext_t*>(aSample.mContext)->uc_mcontext;
  mcontext_t savedContext;
  NotNull<PseudoStack*> pseudoStack = aSample.mPseudoStack;

  // The pseudostack contains an "EnterJIT" frame whenever we enter
  // JIT code with profiling enabled; the stack pointer value points
  // the saved registers.  We use this to unwind resume unwinding
  // after encounting JIT code.
  for (uint32_t i = pseudoStack->stackSize(); i > 0; --i) {
    // The pseudostack grows towards higher indices, so we iterate
    // backwards (from callee to caller).
    volatile js::ProfileEntry& entry = pseudoStack->mStack[i - 1];
    if (!entry.isJs() && strcmp(entry.label(), "EnterJIT") == 0) {
      // Found JIT entry frame.  Unwind up to that point (i.e., force
      // the stack walk to stop before the block of saved registers;
      // note that it yields nondecreasing stack pointers), then restore
      // the saved state.
      uint32_t* vSP = reinterpret_cast<uint32_t*>(entry.stackAddress());

      nativeStack.count += EHABIStackWalk(*mcontext,
                                          /* stackBase = */ vSP,
                                          sp_array + nativeStack.count,
                                          pc_array + nativeStack.count,
                                          nativeStack.size - nativeStack.count);

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
  nativeStack.count += EHABIStackWalk(*mcontext,
                                      aSample.mStackTop,
                                      sp_array + nativeStack.count,
                                      pc_array + nativeStack.count,
                                      nativeStack.size - nativeStack.count);

  MergeStacksIntoProfile(aLock, aBuffer, aSample, nativeStack);
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
DoNativeBacktrace(PSLockRef aLock, ProfileBuffer* aBuffer,
                  const TickSample& aSample)
{
  const mcontext_t* mc =
    &reinterpret_cast<ucontext_t*>(aSample.mContext)->uc_mcontext;

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
    uintptr_t end = reinterpret_cast<uintptr_t>(aSample.mStackTop);
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

  // The maximum number of frames that LUL will produce.  Setting it
  // too high gives a risk of it wasting a lot of time looping on
  // corrupted stacks.
  const int MAX_NATIVE_FRAMES = 256;

  size_t scannedFramesAllowed = 0;

  uintptr_t framePCs[MAX_NATIVE_FRAMES];
  uintptr_t frameSPs[MAX_NATIVE_FRAMES];
  size_t framesAvail = mozilla::ArrayLength(framePCs);
  size_t framesUsed  = 0;
  size_t scannedFramesAcquired = 0, framePointerFramesAcquired = 0;
  lul::LUL* lul = gPS->LUL(aLock);
  lul->Unwind(&framePCs[0], &frameSPs[0],
              &framesUsed, &framePointerFramesAcquired, &scannedFramesAcquired,
              framesAvail, scannedFramesAllowed,
              &startRegs, &stackImg);

  NativeStack nativeStack = {
    reinterpret_cast<void**>(framePCs),
    reinterpret_cast<void**>(frameSPs),
    mozilla::ArrayLength(framePCs),
    framesUsed
  };

  MergeStacksIntoProfile(aLock, aBuffer, aSample, nativeStack);

  // Update stats in the LUL stats object.  Unfortunately this requires
  // three global memory operations.
  lul->mStats.mContext += 1;
  lul->mStats.mCFI     += framesUsed - 1 - framePointerFramesAcquired -
                                           scannedFramesAcquired;
  lul->mStats.mFP      += framePointerFramesAcquired;
  lul->mStats.mScanned += scannedFramesAcquired;
}

#endif

static void
DoSampleStackTrace(PSLockRef aLock, ProfileBuffer* aBuffer,
                   const TickSample& aSample)
{
  NativeStack nativeStack = { nullptr, nullptr, 0, 0 };
  MergeStacksIntoProfile(aLock, aBuffer, aSample, nativeStack);

  if (gPS->FeatureLeaf(aLock)) {
    aBuffer->addTag(ProfileBufferEntry::NativeLeafAddr((void*)aSample.mPC));
  }
}

// This function is called for each sampling period with the current program
// counter. It is called within a signal and so must be re-entrant.
static void
Tick(PSLockRef aLock, ProfileBuffer* aBuffer, const TickSample& aSample)
{
  aBuffer->addTagThreadId(aSample.mThreadId, aSample.mLastSample);

  mozilla::TimeDuration delta =
    aSample.mTimeStamp - gPS->ProcessStartTime(aLock);
  aBuffer->addTag(ProfileBufferEntry::Time(delta.ToMilliseconds()));

  NotNull<PseudoStack*> pseudoStack = aSample.mPseudoStack;

#if defined(HAVE_NATIVE_UNWIND)
  if (gPS->FeatureStackWalk(aLock)) {
    DoNativeBacktrace(aLock, aBuffer, aSample);
  } else
#endif
  {
    DoSampleStackTrace(aLock, aBuffer, aSample);
  }

  // Don't process the PseudoStack's markers if we're synchronously sampling
  // the current thread.
  if (!aSample.mIsSynchronous) {
    ProfilerMarkerLinkedList* pendingMarkersList =
      pseudoStack->getPendingMarkers();
    while (pendingMarkersList && pendingMarkersList->peek()) {
      ProfilerMarker* marker = pendingMarkersList->popHead();
      aBuffer->addStoredMarker(marker);
      aBuffer->addTag(ProfileBufferEntry::Marker(marker));
    }
  }

  if (aSample.mResponsiveness && aSample.mResponsiveness->HasData()) {
    mozilla::TimeDuration delta =
      aSample.mResponsiveness->GetUnresponsiveDuration(aSample.mTimeStamp);
    aBuffer->addTag(ProfileBufferEntry::Responsiveness(delta.ToMilliseconds()));
  }

  // rssMemory is equal to 0 when we are not recording.
  if (aSample.mRSSMemory != 0) {
    double rssMemory = static_cast<double>(aSample.mRSSMemory);
    aBuffer->addTag(ProfileBufferEntry::ResidentMemory(rssMemory));
  }

  // ussMemory is equal to 0 when we are not recording.
  if (aSample.mUSSMemory != 0) {
    double ussMemory = static_cast<double>(aSample.mUSSMemory);
    aBuffer->addTag(ProfileBufferEntry::UnsharedMemory(ussMemory));
  }
}

// END tick/unwinding code
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
  aWriter.StartArrayProperty("data");
  {
    UniquePtr<nsTArray<nsCString>> data =
      mozilla::tasktracer::GetLoggedData(gPS->ProcessStartTime(aLock));
    for (uint32_t i = 0; i < data->Length(); ++i) {
      aWriter.StringElement((data->ElementAt(i)).get());
    }
  }
  aWriter.EndArray();

  aWriter.StartArrayProperty("threads");
  {
    const PS::ThreadVector& liveThreads = gPS->LiveThreads(aLock);
    for (size_t i = 0; i < liveThreads.size(); i++) {
      ThreadInfo* info = liveThreads.at(i);
      StreamNameAndThreadId(aWriter, info->Name(), info->ThreadId());
    }

    const PS::ThreadVector& deadThreads = gPS->DeadThreads(aLock);
    for (size_t i = 0; i < deadThreads.size(); i++) {
      ThreadInfo* info = deadThreads.at(i);
      StreamNameAndThreadId(aWriter, info->Name(), info->ThreadId());
    }
  }
  aWriter.EndArray();

  aWriter.DoubleProperty(
    "start", static_cast<double>(mozilla::tasktracer::GetStartTime()));
#endif
}

static void
StreamMetaJSCustomObject(PSLockRef aLock, SpliceableJSONWriter& aWriter)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  aWriter.IntProperty("version", 6);
  aWriter.DoubleProperty("interval", gPS->Interval(aLock));
  aWriter.IntProperty("stackwalk", gPS->FeatureStackWalk(aLock));

#ifdef DEBUG
  aWriter.IntProperty("debug", 1);
#else
  aWriter.IntProperty("debug", 0);
#endif

  aWriter.IntProperty("gcpoison", JS::IsGCPoisoning() ? 1 : 0);

  bool asyncStacks = Preferences::GetBool("javascript.options.asyncstack");
  aWriter.IntProperty("asyncstack", asyncStacks);

  // The "startTime" field holds the number of milliseconds since midnight
  // January 1, 1970 GMT. This grotty code computes (Now - (Now -
  // ProcessStartTime)) to convert gPS->ProcessStartTime() into that form.
  mozilla::TimeDuration delta =
    mozilla::TimeStamp::Now() - gPS->ProcessStartTime(aLock);
  aWriter.DoubleProperty(
    "startTime", static_cast<double>(PR_Now()/1000.0 - delta.ToMilliseconds()));

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

#if defined(PROFILE_JAVA)
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

  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  MOZ_RELEASE_ASSERT(gPS && gPS->IsActive(aLock));

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
  if (gPS->FeatureTaskTracer(aLock)) {
    aWriter.StartObjectProperty("tasktracer");
    StreamTaskTracer(aLock, aWriter);
    aWriter.EndObject();
  }

  // Lists the samples for each thread profile
  aWriter.StartArrayProperty("threads");
  {
    gPS->SetIsPaused(aLock, true);

    const PS::ThreadVector& liveThreads = gPS->LiveThreads(aLock);
    for (size_t i = 0; i < liveThreads.size(); i++) {
      ThreadInfo* info = liveThreads.at(i);
      if (!info->IsBeingProfiled()) {
        continue;
      }
      info->StreamJSON(gPS->Buffer(aLock), aWriter,
                       gPS->ProcessStartTime(aLock), aSinceTime);
    }

    const PS::ThreadVector& deadThreads = gPS->DeadThreads(aLock);
    for (size_t i = 0; i < deadThreads.size(); i++) {
      ThreadInfo* info = deadThreads.at(i);
      MOZ_ASSERT(info->IsBeingProfiled());
      info->StreamJSON(gPS->Buffer(aLock), aWriter,
                       gPS->ProcessStartTime(aLock), aSinceTime);
    }

#if defined(PROFILE_JAVA)
    if (gPS->FeatureJava(aLock)) {
      java::GeckoJavaSampler::Pause();

      aWriter.Start();
      {
        BuildJavaThreadJSObject(aWriter);
      }
      aWriter.End();

      java::GeckoJavaSampler::Unpause();
    }
#endif

    gPS->SetIsPaused(aLock, false);
  }
  aWriter.EndArray();
}

bool
profiler_stream_json_for_this_process(SpliceableJSONWriter& aWriter, double aSinceTime)
{
  LOG("profiler_stream_json_for_this_process");

  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  MOZ_RELEASE_ASSERT(gPS);

  PSAutoLock lock(gPSMutex);

  if (!gPS->IsActive(lock)) {
    return false;
  }

  locked_profiler_stream_json_for_this_process(lock, aWriter, aSinceTime);
  return true;
}

// END saving/streaming code
////////////////////////////////////////////////////////////////////////

ProfilerMarker::ProfilerMarker(const char* aMarkerName,
                               ProfilerMarkerPayload* aPayload,
                               double aTime)
  : mMarkerName(strdup(aMarkerName))
  , mPayload(aPayload)
  , mTime(aTime)
{
}

ProfilerMarker::~ProfilerMarker() {
  free(mMarkerName);
  delete mPayload;
}

void
ProfilerMarker::SetGeneration(uint32_t aGenID) {
  mGenID = aGenID;
}

double
ProfilerMarker::GetTime() const {
  return mTime;
}

void ProfilerMarker::StreamJSON(SpliceableJSONWriter& aWriter,
                                const TimeStamp& aProcessStartTime,
                                UniqueStacks& aUniqueStacks) const
{
  // Schema:
  //   [name, time, data]

  aWriter.StartArrayElement();
  {
    aUniqueStacks.mUniqueStrings.WriteElement(aWriter, GetMarkerName());
    aWriter.DoubleElement(mTime);
    // TODO: Store the callsite for this marker if available:
    // if have location data
    //   b.NameValue(marker, "location", ...);
    if (mPayload) {
      aWriter.StartObjectElement();
      {
        mPayload->StreamPayload(aWriter, aProcessStartTime, aUniqueStacks);
      }
      aWriter.EndObject();
    }
  }
  aWriter.EndArray();
}

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
// BEGIN SamplerThread

// This suspends the calling thread for the given number of microseconds.
// Best effort timing.
static void SleepMicro(int aMicroseconds);

#if defined(GP_OS_linux) || defined(GP_OS_android)
struct SigHandlerCoordinator;
#endif

// The sampler thread controls sampling and runs whenever the profiler is
// active. It periodically runs through all registered threads, finds those
// that should be sampled, then pauses and samples them.

class SamplerThread
{
public:
  // Creates a sampler thread, but doesn't start it.
  SamplerThread(PSLockRef aLock, uint32_t aActivityGeneration,
                double aIntervalMilliseconds);
  ~SamplerThread();

  // This runs on the sampler thread.  It suspends and resumes the samplee
  // threads.
  void SuspendAndSampleAndResumeThread(PSLockRef aLock, TickSample& aSample);

  // This runs on (is!) the sampler thread.
  void Run();

  // This runs on the main thread.
  void Stop(PSLockRef aLock);

private:
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

#if defined(GP_OS_linux) || defined(GP_OS_android)
  // Used to restore the SIGPROF handler when ours is removed.
  struct sigaction mOldSigprofHandler;

  // This process' ID.  Needed as an argument for tgkill in
  // SuspendAndSampleAndResumeThread.
  int mMyPid;

public:
  // The sampler thread's ID.  Used to assert that it is not sampling itself,
  // which would lead to deadlock.
  int mSamplerTid;

  // This is the one-and-only variable used to communicate between the sampler
  // thread and the samplee thread's signal handler. It's static because the
  // samplee thread's signal handler is static.
  static struct SigHandlerCoordinator* sSigHandlerCoordinator;
#endif

private:
  SamplerThread(const SamplerThread&) = delete;
  void operator=(const SamplerThread&) = delete;
};


// This function is the sampler thread.  This implementation is used for all
// targets.
void
SamplerThread::Run()
{
  // This will be positive if we are running behind schedule (sampling less
  // frequently than desired) and negative if we are ahead of schedule.
  TimeDuration lastSleepOvershoot = 0;
  TimeStamp sampleStart = TimeStamp::Now();

  while (true) {
    // This scope is for |lock|. It ends before we sleep below.
    {
      PSAutoLock lock(gPSMutex);

      // At this point profiler_stop() might have been called, and
      // profiler_start() might have been called on another thread.
      // Alternatively, profiler_shutdown() might have been called and gPS
      // may be null. In all these cases, PS::sActivityGeneration will no
      // longer equal mActivityGeneration, so we must exit immediately, but
      // without touching gPS. (This is why PS::sActivityGeneration must be
      // static.)
      if (PS::ActivityGeneration(lock) != mActivityGeneration) {
        return;
      }

      gPS->Buffer(lock)->deleteExpiredStoredMarkers();

      if (!gPS->IsPaused(lock)) {
        const PS::ThreadVector& liveThreads = gPS->LiveThreads(lock);
        for (uint32_t i = 0; i < liveThreads.size(); i++) {
          ThreadInfo* info = liveThreads[i];

          if (!info->IsBeingProfiled()) {
            // We are not interested in profiling this thread.
            continue;
          }

          // If the thread is asleep and has been sampled before in the same
          // sleep episode, find and copy the previous sample, as that's
          // cheaper than taking a new sample.
          if (info->Stack()->CanDuplicateLastSampleDueToSleep()) {
            bool dup_ok =
              gPS->Buffer(lock)->DuplicateLastSample(
                info->ThreadId(), gPS->ProcessStartTime(lock),
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
          if (i == 0 && gPS->FeatureMemory(lock)) {
            rssMemory = nsMemoryReporterManager::ResidentFast();
#if defined(GP_OS_linux) || defined(GP_OS_android)
            ussMemory = nsMemoryReporterManager::ResidentUnique();
#endif
          }

          TickSample sample(info, rssMemory, ussMemory);

          SuspendAndSampleAndResumeThread(lock, sample);
        }

#if defined(USE_LUL_STACKWALK)
        // The LUL unwind object accumulates frame statistics. Periodically we
        // should poke it to give it a chance to print those statistics.  This
        // involves doing I/O (fprintf, __android_log_print, etc.) and so
        // can't safely be done from the critical section inside
        // SuspendAndSampleAndResumeThread, which is why it is done here.
        gPS->LUL(lock)->MaybeShowStats();
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
    SleepMicro(static_cast<int>(sleepTime));
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
#if defined(USE_LUL_STACKWALK)
  size_t lulSize = 0;
#endif

  {
    PSAutoLock lock(gPSMutex);

    if (gPS) {
      profSize = GeckoProfilerMallocSizeOf(gPS);

      const PS::ThreadVector& liveThreads = gPS->LiveThreads(lock);
      for (uint32_t i = 0; i < liveThreads.size(); i++) {
        ThreadInfo* info = liveThreads.at(i);
        profSize += info->SizeOfIncludingThis(GeckoProfilerMallocSizeOf);
      }

      const PS::ThreadVector& deadThreads = gPS->DeadThreads(lock);
      for (uint32_t i = 0; i < deadThreads.size(); i++) {
        ThreadInfo* info = deadThreads.at(i);
        profSize += info->SizeOfIncludingThis(GeckoProfilerMallocSizeOf);
      }

      if (gPS->IsActive(lock)) {
        profSize +=
          gPS->Buffer(lock)->SizeOfIncludingThis(GeckoProfilerMallocSizeOf);
      }

      // Measurement of the following things may be added later if DMD finds it
      // is worthwhile:
      // - gPS->mFeatures
      // - gPS->mFilters
      // - gPS->mLiveThreads itself (its elements' children are measured above)
      // - gPS->mDeadThreads itself (ditto)
      // - gPS->mInterposeObserver

#if defined(USE_LUL_STACKWALK)
      lul::LUL* lul = gPS->LUL(lock);
      lulSize = lul ? lul->SizeOfIncludingThis(GeckoProfilerMallocSizeOf) : 0;
#endif
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
ThreadSelected(PSLockRef aLock, const char* aThreadName)
{
  // This function runs both on and off the main thread.

  MOZ_RELEASE_ASSERT(gPS);

  const Vector<std::string>& filters = gPS->Filters(aLock);

  if (filters.empty()) {
    return true;
  }

  std::string name = aThreadName;
  std::transform(name.begin(), name.end(), name.begin(), ::tolower);

  for (uint32_t i = 0; i < filters.length(); ++i) {
    std::string filter = filters[i];
    std::transform(filter.begin(), filter.end(), filter.begin(), ::tolower);

    // Crude, non UTF-8 compatible, case insensitive substring search
    if (name.find(filter) != std::string::npos) {
      return true;
    }
  }

  return false;
}

static bool
ShouldProfileThread(PSLockRef aLock, ThreadInfo* aInfo)
{
  // This function runs both on and off the main thread.

  MOZ_RELEASE_ASSERT(gPS);

  return ((aInfo->IsMainThread() || gPS->FeatureThreads(aLock)) &&
          ThreadSelected(aLock, aInfo->Name()));
}

// Find the ThreadInfo for the current thread. On success, *aIndexOut is set to
// the index if it is non-null.
static ThreadInfo*
FindLiveThreadInfo(PSLockRef aLock, int* aIndexOut = nullptr)
{
  // This function runs both on and off the main thread.

  Thread::tid_t id = Thread::GetCurrentId();
  const PS::ThreadVector& liveThreads = gPS->LiveThreads(aLock);
  for (uint32_t i = 0; i < liveThreads.size(); i++) {
    ThreadInfo* info = liveThreads.at(i);
    if (info->ThreadId() == id) {
      if (aIndexOut) {
        *aIndexOut = i;
      }
      return info;
    }
  }
  return nullptr;
}

static void
locked_register_thread(PSLockRef aLock, const char* aName, void* stackTop)
{
  // This function runs both on and off the main thread.

  MOZ_RELEASE_ASSERT(gPS);

  MOZ_RELEASE_ASSERT(!FindLiveThreadInfo(aLock));

  if (!tlsPseudoStack.init()) {
    return;
  }

  ThreadInfo* info = new ThreadInfo(aName, Thread::GetCurrentId(),
                                    NS_IsMainThread(), stackTop);
  NotNull<PseudoStack*> pseudoStack = info->Stack();

  tlsPseudoStack.set(pseudoStack.get());

  if (gPS->IsActive(aLock) && ShouldProfileThread(aLock, info)) {
    info->StartProfiling();
    if (gPS->FeatureJS(aLock)) {
      // This startJSSampling() call is on-thread, so we can poll manually to
      // start JS sampling immediately.
      pseudoStack->startJSSampling();
      pseudoStack->pollJSSampling();
    }
  }

  gPS->LiveThreads(aLock).push_back(info);
}

static void
NotifyProfilerStarted(const int aEntries, double aInterval,
                      const char** aFeatures, uint32_t aFeatureCount,
                      const char** aFilters, uint32_t aFilterCount)
{
  if (!CanNotifyObservers()) {
    return;
  }

  nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
  if (!os) {
    return;
  }

  nsTArray<nsCString> featuresArray;
  for (size_t i = 0; i < aFeatureCount; ++i) {
    featuresArray.AppendElement(aFeatures[i]);
  }

  nsTArray<nsCString> filtersArray;
  for (size_t i = 0; i < aFilterCount; ++i) {
    filtersArray.AppendElement(aFilters[i]);
  }

  nsCOMPtr<nsIProfilerStartParams> params =
    new nsProfilerStartParams(aEntries, aInterval, featuresArray, filtersArray);

  os->NotifyObservers(params, "profiler-started", nullptr);
}

static void
NotifyObservers(const char* aTopic)
{
  if (!CanNotifyObservers()) {
    return;
  }

  nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
  if (!os) {
    return;
  }

  os->NotifyObservers(nullptr, aTopic, nullptr);
}

static void
locked_profiler_start(PSLockRef aLock, const int aEntries, double aInterval,
                      const char** aFeatures, uint32_t aFeatureCount,
                      const char** aFilters, uint32_t aFilterCount);

void
profiler_init(void* aStackTop)
{
  LOG("profiler_init");

  MOZ_RELEASE_ASSERT(!gPS);

  const char* features[] = { "js"
#if defined(PROFILE_JAVA)
                           , "java"
#endif
                           , "leaf"
#if defined(HAVE_NATIVE_UNWIND)
                           , "stackwalk"
#endif
                           , "threads"
                           };

  const char* threadFilters[] = { "GeckoMain", "Compositor" };

  if (getenv("MOZ_PROFILER_HELP")) {
    PrintUsageThenExit(0); // terminates execution
  }

  {
    PSAutoLock lock(gPSMutex);

    // We've passed the possible failure point. Instantiate gPS, which
    // indicates that the profiler has initialized successfully.
    gPS = new PS();

    gPS->SetProcessStartTime(lock, mozilla::TimeStamp::ProcessCreation());

    locked_register_thread(lock, kMainThreadName, aStackTop);

    // Platform-specific initialization.
    PlatformInit(lock);

#ifdef MOZ_TASK_TRACER
    mozilla::tasktracer::InitTaskTracer();
#endif

#if defined(PROFILE_JAVA)
    if (mozilla::jni::IsFennec()) {
      GeckoJavaSampler::Init();
    }
#endif

    // (Linux-only) We could create gPS->mLUL and read unwind info into it at
    // this point. That would match the lifetime implied by destruction of it
    // in profiler_shutdown() just below. However, that gives a big delay on
    // startup, even if no profiling is actually to be done. So, instead, it is
    // created on demand at the first call to PlatformStart().

    if (!getenv("MOZ_PROFILER_STARTUP")) {
      return;
    }

    LOG("- MOZ_PROFILER_STARTUP is set");

    int entries = PROFILE_DEFAULT_ENTRIES;
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

    int interval = PROFILE_DEFAULT_INTERVAL;
    const char* startupInterval = getenv("MOZ_PROFILER_STARTUP_INTERVAL");
    if (startupInterval) {
      errno = 0;
      interval = strtol(startupInterval, nullptr, 10);
      if (errno == 0 && 1 <= interval && interval <= 1000) {
        LOG("- MOZ_PROFILER_STARTUP_INTERVAL = %d", interval);
      } else {
        PrintUsageThenExit(1);
      }
    }

    locked_profiler_start(lock, entries, interval,
                          features, MOZ_ARRAY_LENGTH(features),
                          threadFilters, MOZ_ARRAY_LENGTH(threadFilters));
  }

  // We do this with gPSMutex unlocked. The comment in profiler_stop() explains
  // why.
  NotifyProfilerStarted(PROFILE_DEFAULT_ENTRIES, PROFILE_DEFAULT_INTERVAL,
                        features, MOZ_ARRAY_LENGTH(features),
                        threadFilters, MOZ_ARRAY_LENGTH(threadFilters));
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
  MOZ_RELEASE_ASSERT(gPS);

  // If the profiler is active we must get a handle to the SamplerThread before
  // gPS is destroyed, in order to delete it.
  SamplerThread* samplerThread = nullptr;
  {
    PSAutoLock lock(gPSMutex);

    // Save the profile on shutdown if requested.
    if (gPS->IsActive(lock)) {
      const char* filename = getenv("MOZ_PROFILER_SHUTDOWN");
      if (filename) {
        locked_profiler_save_profile_to_file(lock, filename);
      }

      samplerThread = locked_profiler_stop(lock);
    }

    PS::ThreadVector& liveThreads = gPS->LiveThreads(lock);
    while (liveThreads.size() > 0) {
      delete liveThreads.back();
      liveThreads.pop_back();
    }

    PS::ThreadVector& deadThreads = gPS->DeadThreads(lock);
    while (deadThreads.size() > 0) {
      delete deadThreads.back();
      deadThreads.pop_back();
    }

#if defined(USE_LUL_STACKWALK)
    // Delete the LUL object if it actually got created.
    lul::LUL* lul = gPS->LUL(lock);
    if (lul) {
      delete lul;
      gPS->SetLUL(lock, nullptr);
    }
#endif

    delete gPS;
    gPS = nullptr;

    // We just destroyed gPS and the ThreadInfos (and PseudoStacks) it
    // contains, so we can clear this thread's tlsPseudoStack.
    tlsPseudoStack.set(nullptr);

#ifdef MOZ_TASK_TRACER
    mozilla::tasktracer::ShutdownTaskTracer();
#endif
  }

  // We do these operations with gPSMutex unlocked. The comments in
  // profiler_stop() explain why.
  if (samplerThread) {
    NotifyObservers("profiler-stopped");
    delete samplerThread;
  }
}

UniquePtr<char[]>
profiler_get_profile(double aSinceTime)
{
  LOG("profiler_get_profile");

  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  MOZ_RELEASE_ASSERT(gPS);

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
profiler_get_start_params(int* aEntries, double* aInterval,
                          mozilla::Vector<const char*>* aFeatures,
                          mozilla::Vector<const char*>* aFilters)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  MOZ_RELEASE_ASSERT(gPS);

  if (NS_WARN_IF(!aEntries) || NS_WARN_IF(!aInterval) ||
      NS_WARN_IF(!aFeatures) || NS_WARN_IF(!aFilters)) {
    return;
  }

  PSAutoLock lock(gPSMutex);

  *aEntries = gPS->Entries(lock);
  *aInterval = gPS->Interval(lock);

  const Vector<std::string>& features = gPS->Features(lock);
  MOZ_ALWAYS_TRUE(aFeatures->resize(features.length()));
  for (size_t i = 0; i < features.length(); ++i) {
    (*aFeatures)[i] = features[i].c_str();
  }

  const Vector<std::string>& filters = gPS->Filters(lock);
  MOZ_ALWAYS_TRUE(aFilters->resize(filters.length()));
  for (uint32_t i = 0; i < filters.length(); ++i) {
    (*aFilters)[i] = filters[i].c_str();
  }
}

static void
locked_profiler_save_profile_to_file(PSLockRef aLock, const char* aFilename)
{
  LOG("locked_profiler_save_profile_to_file(%s)", aFilename);

  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  MOZ_RELEASE_ASSERT(gPS && gPS->IsActive(aLock));

  std::ofstream stream;
  stream.open(aFilename);
  if (stream.is_open()) {
    SpliceableJSONWriter w(mozilla::MakeUnique<OStreamJSONWriteFunc>(stream));
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

  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  MOZ_RELEASE_ASSERT(gPS);

  PSAutoLock lock(gPSMutex);

  if (!gPS->IsActive(lock)) {
    return;
  }

  locked_profiler_save_profile_to_file(lock, aFilename);
}

const char**
profiler_get_features()
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  MOZ_RELEASE_ASSERT(gPS);

  static const char* features[] = {
#if defined(HAVE_NATIVE_UNWIND)
    // Walk the C++ stack.
    "stackwalk",
#endif
    // Include the C++ leaf node if not stackwalking. DevTools
    // profiler doesn't want the native addresses.
    "leaf",
    // Profile Java code (Android only).
    "java",
    // Tell the JS engine to emit pseudostack entries in the prologue/epilogue.
    "js",
    // GPU Profiling (may not be supported by the GL)
    "gpu",
    // Profile the registered secondary threads.
    "threads",
    // Do not include user-identifiable information
    "privacy",
    // Dump the layer tree with the textures.
    "layersdump",
    // Dump the display list with the textures.
    "displaylistdump",
    // Add main thread I/O to the profile
    "mainthreadio",
    // Add RSS collection
    "memory",
    // Restyle profiling.
    "restyle",
#ifdef MOZ_TASK_TRACER
    // Start profiling with feature TaskTracer.
    "tasktracer",
#endif
    nullptr
  };

  return features;
}

void
profiler_get_buffer_info_helper(uint32_t* aCurrentPosition,
                                uint32_t* aEntries,
                                uint32_t* aGeneration)
{
  // This function is called by profiler_get_buffer_info(), which has already
  // zeroed the outparams.

  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  MOZ_RELEASE_ASSERT(gPS);

  PSAutoLock lock(gPSMutex);

  if (!gPS->IsActive(lock)) {
    return;
  }

  *aCurrentPosition = gPS->Buffer(lock)->mWritePos;
  *aEntries = gPS->Entries(lock);
  *aGeneration = gPS->Buffer(lock)->mGeneration;
}

static bool
hasFeature(const char** aFeatures, uint32_t aFeatureCount, const char* aFeature)
{
  for (size_t i = 0; i < aFeatureCount; i++) {
    if (strcmp(aFeatures[i], aFeature) == 0) {
      return true;
    }
  }
  return false;
}

static void
locked_profiler_start(PSLockRef aLock, int aEntries, double aInterval,
                      const char** aFeatures, uint32_t aFeatureCount,
                      const char** aFilters, uint32_t aFilterCount)
{
  if (LOG_TEST) {
    LOG("locked_profiler_start");
    LOG("- entries  = %d", aEntries);
    LOG("- interval = %.2f", aInterval);
    for (uint32_t i = 0; i < aFeatureCount; i++) {
      LOG("- feature  = %s", aFeatures[i]);
    }
    for (uint32_t i = 0; i < aFilterCount; i++) {
      LOG("- threads  = %s", aFilters[i]);
    }
  }

  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  MOZ_RELEASE_ASSERT(gPS && !gPS->IsActive(aLock));

  // Fall back to the default value if the passed-in value is unreasonable.
  int entries = aEntries > 0 ? aEntries : PROFILE_DEFAULT_ENTRIES;
  gPS->SetEntries(aLock, entries);

  // Ditto.
  double interval = aInterval > 0 ? aInterval : PROFILE_DEFAULT_INTERVAL;
  gPS->SetInterval(aLock, interval);

  // Deep copy aFeatures. Must precede the ShouldProfileThread() call below.
  Vector<std::string>& features = gPS->Features(aLock);
  MOZ_ALWAYS_TRUE(features.resize(aFeatureCount));
  for (uint32_t i = 0; i < aFeatureCount; ++i) {
    features[i] = aFeatures[i];
  }

  // Deep copy aFilters. Must precede the ShouldProfileThread() call below.
  Vector<std::string>& filters = gPS->Filters(aLock);
  MOZ_ALWAYS_TRUE(filters.resize(aFilterCount));
  for (uint32_t i = 0; i < aFilterCount; ++i) {
    filters[i] = aFilters[i];
  }

#define HAS_FEATURE(feature) hasFeature(aFeatures, aFeatureCount, feature)

  gPS->SetFeatureDisplayListDump(aLock, HAS_FEATURE("displaylistdump"));
  gPS->SetFeatureGPU(aLock, HAS_FEATURE("gpu"));
#if defined(PROFILE_JAVA)
  gPS->SetFeatureJava(aLock, mozilla::jni::IsFennec() && HAS_FEATURE("java"));
#endif
  bool featureJS = HAS_FEATURE("js");
  gPS->SetFeatureJS(aLock, featureJS);
  gPS->SetFeatureLayersDump(aLock, HAS_FEATURE("layersdump"));
  gPS->SetFeatureLeaf(aLock, HAS_FEATURE("leaf"));
  bool featureMainThreadIO = HAS_FEATURE("mainthreadio");
  gPS->SetFeatureMainThreadIO(aLock, featureMainThreadIO);
  gPS->SetFeatureMemory(aLock, HAS_FEATURE("memory"));
  gPS->SetFeaturePrivacy(aLock, HAS_FEATURE("privacy"));
  gPS->SetFeatureRestyle(aLock, HAS_FEATURE("restyle"));
  gPS->SetFeatureStackWalk(aLock, HAS_FEATURE("stackwalk"));
#ifdef MOZ_TASK_TRACER
  bool featureTaskTracer = HAS_FEATURE("tasktracer");
  gPS->SetFeatureTaskTracer(aLock, featureTaskTracer);
#endif
  // Profile non-main threads if we have a filter, because users sometimes ask
  // to filter by a list of threads but forget to explicitly request.
  // Must precede the ShouldProfileThread() call below.
  gPS->SetFeatureThreads(aLock, HAS_FEATURE("threads") || aFilterCount > 0);

#undef HAS_FEATURE

  gPS->SetBuffer(aLock, new ProfileBuffer(entries));

  // Set up profiling for each registered thread, if appropriate.
  const PS::ThreadVector& liveThreads = gPS->LiveThreads(aLock);
  for (uint32_t i = 0; i < liveThreads.size(); i++) {
    ThreadInfo* info = liveThreads.at(i);

    if (ShouldProfileThread(aLock, info)) {
      info->StartProfiling();
      if (featureJS) {
        info->Stack()->startJSSampling();
      }
    }
  }

  // Dead ThreadInfos are deleted in profiler_stop(), and dead ThreadInfos
  // aren't saved when the profiler is inactive. Therefore mDeadThreads should
  // be empty here.
  MOZ_RELEASE_ASSERT(gPS->DeadThreads(aLock).empty());

  if (featureJS) {
    // We just called startJSSampling() on all relevant threads. We can also
    // manually poll the current thread so it starts sampling immediately.
    if (PseudoStack* pseudoStack = tlsPseudoStack.get()) {
      pseudoStack->pollJSSampling();
    }
  }

#ifdef MOZ_TASK_TRACER
  if (featureTaskTracer) {
    mozilla::tasktracer::StartLogging();
  }
#endif

#if defined(PROFILE_JAVA)
  if (gPS->FeatureJava(aLock)) {
    int javaInterval = interval;
    // Java sampling doesn't accuratly keep up with 1ms sampling
    if (javaInterval < 10) {
      javaInterval = 10;
    }
    mozilla::java::GeckoJavaSampler::Start(javaInterval, 1000);
  }
#endif

  // Must precede the PS::ActivityGeneration() call below.
  PS::SetActive(aLock);

  gPS->SetIsPaused(aLock, false);

  // This creates the sampler thread. It doesn't start sampling immediately
  // because the main loop within Run() is blocked until this function's caller
  // unlocks gPSMutex.
  gPS->SetSamplerThread(aLock, new SamplerThread(aLock,
                                                 PS::ActivityGeneration(aLock),
                                                 interval));

  if (featureMainThreadIO) {
    auto interposeObserver = new mozilla::ProfilerIOInterposeObserver();
    gPS->SetInterposeObserver(aLock, interposeObserver);
    mozilla::IOInterposer::Register(mozilla::IOInterposeObserver::OpAll,
                                    interposeObserver);
  }
}

void
profiler_start(int aEntries, double aInterval,
               const char** aFeatures, uint32_t aFeatureCount,
               const char** aFilters, uint32_t aFilterCount)
{
  LOG("profiler_start");

  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  SamplerThread* samplerThread = nullptr;
  {
    PSAutoLock lock(gPSMutex);

    // Initialize if necessary.
    if (!gPS) {
      profiler_init(nullptr);
    }

    // Reset the current state if the profiler is running.
    if (gPS->IsActive(lock)) {
      samplerThread = locked_profiler_stop(lock);
    }

    locked_profiler_start(lock, aEntries, aInterval, aFeatures, aFeatureCount,
                          aFilters, aFilterCount);
  }

  // We do these operations with gPSMutex unlocked. The comments in
  // profiler_stop() explain why.
  if (samplerThread) {
    NotifyObservers("profiler-stopped");
    delete samplerThread;
  }
  NotifyProfilerStarted(aEntries, aInterval, aFeatures, aFeatureCount,
                        aFilters, aFilterCount);
}

static MOZ_MUST_USE SamplerThread*
locked_profiler_stop(PSLockRef aLock)
{
  LOG("locked_profiler_stop");

  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  MOZ_RELEASE_ASSERT(gPS && gPS->IsActive(aLock));

  // We clear things in roughly reverse order to their setting in
  // locked_profiler_start().

  if (gPS->FeatureMainThreadIO(aLock)) {
    mozilla::IOInterposer::Unregister(mozilla::IOInterposeObserver::OpAll,
                                      gPS->InterposeObserver(aLock));
    delete gPS->InterposeObserver(aLock);
    gPS->SetInterposeObserver(aLock, nullptr);
  }

  // The Stop() call doesn't actually stop Run(); that happens in this
  // function's caller when the sampler thread is destroyed. Stop() just gives
  // the SamplerThread a chance to do some cleanup with gPSMutex locked.
  SamplerThread* samplerThread = gPS->SamplerThread(aLock);
  samplerThread->Stop(aLock);
  gPS->SetSamplerThread(aLock, nullptr);

  gPS->SetIsPaused(aLock, false);

  gPS->SetInactive(aLock);

#ifdef MOZ_TASK_TRACER
  if (gPS->FeatureTaskTracer(aLock)) {
    mozilla::tasktracer::StopLogging();
  }
#endif

  // Stop sampling live threads.
  PS::ThreadVector& liveThreads = gPS->LiveThreads(aLock);
  for (uint32_t i = 0; i < liveThreads.size(); i++) {
    ThreadInfo* info = liveThreads.at(i);
    if (info->IsBeingProfiled()) {
      if (gPS->FeatureJS(aLock)) {
        info->Stack()->stopJSSampling();
      }
      info->StopProfiling();
    }
  }

  // This is where we destroy the ThreadInfos for all dead threads.
  PS::ThreadVector& deadThreads = gPS->DeadThreads(aLock);
  while (deadThreads.size() > 0) {
    delete deadThreads.back();
    deadThreads.pop_back();
  }

  if (gPS->FeatureJS(aLock)) {
    // We just called stopJSSampling() (through ThreadInfo::StopProfiling) on
    // all relevant threads. We can also manually poll the current thread so
    // it stops profiling immediately.
    if (PseudoStack* stack = tlsPseudoStack.get()) {
      stack->pollJSSampling();
    }
  }

  delete gPS->Buffer(aLock);
  gPS->SetBuffer(aLock, nullptr);

  gPS->SetFeatureDisplayListDump(aLock, false);
  gPS->SetFeatureGPU(aLock, false);
  gPS->SetFeatureJava(aLock, false);
  gPS->SetFeatureJS(aLock, false);
  gPS->SetFeatureLayersDump(aLock, false);
  gPS->SetFeatureLeaf(aLock, false);
  gPS->SetFeatureMemory(aLock, false);
  gPS->SetFeaturePrivacy(aLock, false);
  gPS->SetFeatureRestyle(aLock, false);
  gPS->SetFeatureStackWalk(aLock, false);
  gPS->SetFeatureTaskTracer(aLock, false);
  gPS->SetFeatureThreads(aLock, false);

  gPS->Filters(aLock).clear();

  gPS->Features(aLock).clear();

  gPS->SetInterval(aLock, 0.0);

  gPS->SetEntries(aLock, 0);

  return samplerThread;
}

void
profiler_stop()
{
  LOG("profiler_stop");

  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  MOZ_RELEASE_ASSERT(gPS);

  SamplerThread* samplerThread;
  {
    PSAutoLock lock(gPSMutex);

    if (!gPS->IsActive(lock)) {
      return;
    }

    samplerThread = locked_profiler_stop(lock);
  }

  // We notify observers with gPSMutex unlocked. Otherwise we might get a
  // deadlock, if code run by the observer calls a profiler function that locks
  // gPSMutex. (This has been seen in practise in bug 1346356.)
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
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  MOZ_RELEASE_ASSERT(gPS);

  PSAutoLock lock(gPSMutex);

  if (!gPS->IsActive(lock)) {
    return false;
  }

  return gPS->IsPaused(lock);
}

void
profiler_pause()
{
  LOG("profiler_pause");

  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  MOZ_RELEASE_ASSERT(gPS);

  {
    PSAutoLock lock(gPSMutex);

    if (!gPS->IsActive(lock)) {
      return;
    }

    gPS->SetIsPaused(lock, true);
  }

  // gPSMutex must be unlocked when we notify, to avoid potential deadlocks.
  NotifyObservers("profiler-paused");
}

void
profiler_resume()
{
  LOG("profiler_resume");

  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  MOZ_RELEASE_ASSERT(gPS);

  PSAutoLock lock(gPSMutex);

  {
    if (!gPS->IsActive(lock)) {
      return;
    }

    gPS->SetIsPaused(lock, false);
  }

  // gPSMutex must be unlocked when we notify, to avoid potential deadlocks.
  NotifyObservers("profiler-resumed");
}

bool
profiler_feature_active(const char* aName)
{
  // This function runs both on and off the main thread.

  MOZ_RELEASE_ASSERT(gPS);

  PSAutoLock lock(gPSMutex);

  if (!gPS->IsActive(lock)) {
    return false;
  }

  if (strcmp(aName, "displaylistdump") == 0) {
    return gPS->FeatureDisplayListDump(lock);
  }

  if (strcmp(aName, "gpu") == 0) {
    return gPS->FeatureGPU(lock);
  }

  if (strcmp(aName, "layersdump") == 0) {
    return gPS->FeatureLayersDump(lock);
  }

  if (strcmp(aName, "restyle") == 0) {
    return gPS->FeatureRestyle(lock);
  }

  return false;
}

bool
profiler_is_active()
{
  // This function runs both on and off the main thread.

  MOZ_RELEASE_ASSERT(gPS);

  PSAutoLock lock(gPSMutex);

  return gPS->IsActive(lock);
}

void
profiler_register_thread(const char* aName, void* aGuessStackTop)
{
  DEBUG_LOG("profiler_register_thread(%s)", aName);

  MOZ_RELEASE_ASSERT(!NS_IsMainThread());
  MOZ_RELEASE_ASSERT(gPS);

  PSAutoLock lock(gPSMutex);

  void* stackTop = GetStackTop(aGuessStackTop);
  locked_register_thread(lock, aName, stackTop);
}

void
profiler_unregister_thread()
{
  MOZ_RELEASE_ASSERT(!NS_IsMainThread());
  MOZ_RELEASE_ASSERT(gPS);

  PSAutoLock lock(gPSMutex);

  // We don't call PseudoStack::stopJSSampling() here; there's no point doing
  // that for a JS thread that is in the process of disappearing.

  int i;
  ThreadInfo* info = FindLiveThreadInfo(lock, &i);
  if (info) {
    DEBUG_LOG("profiler_unregister_thread: %s", info->Name());
    if (gPS->IsActive(lock) && info->IsBeingProfiled()) {
      gPS->DeadThreads(lock).push_back(info);
    } else {
      delete info;
    }
    PS::ThreadVector& liveThreads = gPS->LiveThreads(lock);
    liveThreads.erase(liveThreads.begin() + i);

    // Whether or not we just destroyed the PseudoStack (via its owning
    // ThreadInfo), we no longer need to access it via TLS.
    tlsPseudoStack.set(nullptr);

  } else {
    // There are two ways FindLiveThreadInfo() can fail.
    //
    // - tlsPseudoStack.init() failed in locked_register_thread().
    //
    // - We've already called profiler_unregister_thread() for this thread.
    //   (Whether or not it should, this does happen in practice.)
    //
    // Either way, tlsPseudoStack should be empty.
    MOZ_RELEASE_ASSERT(!tlsPseudoStack.get());
  }
}

void
profiler_thread_sleep()
{
  // This function runs both on and off the main thread.

  MOZ_RELEASE_ASSERT(gPS);

  PseudoStack *stack = tlsPseudoStack.get();
  if (!stack) {
    return;
  }
  stack->setSleeping();
}

void
profiler_thread_wake()
{
  // This function runs both on and off the main thread.

  MOZ_RELEASE_ASSERT(gPS);

  PseudoStack *stack = tlsPseudoStack.get();
  if (!stack) {
    return;
  }
  stack->setAwake();
}

bool
profiler_thread_is_sleeping()
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  MOZ_RELEASE_ASSERT(gPS);

  PseudoStack *stack = tlsPseudoStack.get();
  if (!stack) {
    return false;
  }
  return stack->isSleeping();
}

void
profiler_js_interrupt_callback()
{
  // This function runs both on and off the main thread, on JS threads being
  // sampled.

  MOZ_RELEASE_ASSERT(gPS);

  PseudoStack *stack = tlsPseudoStack.get();
  if (!stack) {
    return;
  }

  stack->pollJSSampling();
}

double
profiler_time()
{
  // This function runs both on and off the main thread.

  MOZ_RELEASE_ASSERT(gPS);

  PSAutoLock lock(gPSMutex);

  mozilla::TimeDuration delta =
    mozilla::TimeStamp::Now() - gPS->ProcessStartTime(lock);
  return delta.ToMilliseconds();
}

UniqueProfilerBacktrace
profiler_get_backtrace()
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  MOZ_RELEASE_ASSERT(gPS);

  PSAutoLock lock(gPSMutex);

  if (!gPS->IsActive(lock) || gPS->FeaturePrivacy(lock)) {
    return nullptr;
  }

  PseudoStack* stack = tlsPseudoStack.get();
  if (!stack) {
    MOZ_ASSERT(stack);
    return nullptr;
  }

  Thread::tid_t tid = Thread::GetCurrentId();

  ProfileBuffer* buffer = new ProfileBuffer(GET_BACKTRACE_DEFAULT_ENTRIES);

  UniquePlatformData platformData = AllocPlatformData(tid);

  TickSample sample(WrapNotNull(stack), platformData.get());

#if defined(HAVE_NATIVE_UNWIND)
#if defined(GP_OS_windows) || defined(GP_OS_linux) || defined(GP_OS_android)
  tick_context_t context;
  sample.PopulateContext(&context);
#elif defined(GP_OS_darwin)
  sample.PopulateContext(nullptr);
#else
# error "unknown platform"
#endif
#endif

  Tick(lock, buffer, sample);

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
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  MOZ_RELEASE_ASSERT(gPS);

  MOZ_ASSERT(outputSize >= 2);
  char *bound = output + outputSize - 2;
  output[0] = output[1] = '\0';

  PseudoStack *pseudoStack = tlsPseudoStack.get();
  if (!pseudoStack) {
    return;
  }

  bool includeDynamicString = true;
  {
    PSAutoLock lock(gPSMutex);
    includeDynamicString = !gPS->FeaturePrivacy(lock);
  }

  volatile js::ProfileEntry *pseudoFrames = pseudoStack->mStack;
  uint32_t pseudoCount = pseudoStack->stackSize();

  for (uint32_t i = 0; i < pseudoCount; i++) {
    const char* label = pseudoFrames[i].label();
    const char* dynamicString =
      includeDynamicString ? pseudoFrames[i].getDynamicString() : nullptr;
    size_t labelLength = strlen(label);
    if (dynamicString) {
      // Put the label, a space, and the dynamic string into output.
      size_t dynamicStringLength = strlen(dynamicString);
      if (output + labelLength + 1 + dynamicStringLength >= bound) {
        break;
      }
      strcpy(output, label);
      output += labelLength;
      *output++ = ' ';
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
locked_profiler_add_marker(PSLockRef aLock, const char* aMarker,
                           ProfilerMarkerPayload* aPayload)
{
  // This function runs both on and off the main thread.

  MOZ_RELEASE_ASSERT(gPS);
  MOZ_RELEASE_ASSERT(gPS->IsActive(aLock) && !gPS->FeaturePrivacy(aLock));

  // aPayload must be freed if we return early.
  mozilla::UniquePtr<ProfilerMarkerPayload> payload(aPayload);

  PseudoStack *stack = tlsPseudoStack.get();
  if (!stack) {
    return;
  }

  mozilla::TimeStamp origin = (payload && !payload->GetStartTime().IsNull())
                            ? payload->GetStartTime()
                            : mozilla::TimeStamp::Now();
  mozilla::TimeDuration delta = origin - gPS->ProcessStartTime(aLock);
  stack->addMarker(aMarker, payload.release(), delta.ToMilliseconds());
}

void
profiler_add_marker(const char* aMarker, ProfilerMarkerPayload* aPayload)
{
  // This function runs both on and off the main thread.

  MOZ_RELEASE_ASSERT(gPS);

  PSAutoLock lock(gPSMutex);

  // aPayload must be freed if we return early.
  mozilla::UniquePtr<ProfilerMarkerPayload> payload(aPayload);

  if (!gPS->IsActive(lock) || gPS->FeaturePrivacy(lock)) {
    return;
  }

  locked_profiler_add_marker(lock, aMarker, payload.release());
}

void
profiler_tracing(const char* aCategory, const char* aInfo,
                 TracingKind aKind)
{
  // This function runs both on and off the main thread.

  MOZ_RELEASE_ASSERT(gPS);

  PSAutoLock lock(gPSMutex);

  if (!gPS->IsActive(lock) || gPS->FeaturePrivacy(lock)) {
    return;
  }

  auto marker = new ProfilerMarkerTracing(aCategory, aKind);
  locked_profiler_add_marker(lock, aInfo, marker);
}

void
profiler_tracing(const char* aCategory, const char* aInfo,
                 UniqueProfilerBacktrace aCause, TracingKind aKind)
{
  // This function runs both on and off the main thread.

  MOZ_RELEASE_ASSERT(gPS);

  PSAutoLock lock(gPSMutex);

  if (!gPS->IsActive(lock) || gPS->FeaturePrivacy(lock)) {
    return;
  }

  auto marker =
    new ProfilerMarkerTracing(aCategory, aKind, mozilla::Move(aCause));
  locked_profiler_add_marker(lock, aInfo, marker);
}

void
profiler_log(const char* aStr)
{
  // This function runs both on and off the main thread.

  profiler_tracing("log", aStr);
}

void
profiler_set_js_context(JSContext* aCx)
{
  // This function runs both on and off the main thread.

  MOZ_ASSERT(aCx);

  PseudoStack* stack = tlsPseudoStack.get();
  if (!stack) {
    return;
  }

  stack->setJSContext(aCx);
}

void
profiler_clear_js_context()
{
  // This function runs both on and off the main thread.

  MOZ_RELEASE_ASSERT(gPS);

  PseudoStack* stack = tlsPseudoStack.get();
  if (!stack) {
    return;
  }

  if (!stack->mContext) {
    return;
  }

  // On JS shut down, flush the current buffer as stringifying JIT samples
  // requires a live JSContext.

  PSAutoLock lock(gPSMutex);

  if (gPS->IsActive(lock)) {
    gPS->SetIsPaused(lock, true);

    // Flush this thread's ThreadInfo, if it is being profiled.
    ThreadInfo* info = FindLiveThreadInfo(lock);
    MOZ_RELEASE_ASSERT(info);
    if (info->IsBeingProfiled()) {
      info->FlushSamplesAndMarkers(gPS->Buffer(lock),
                                   gPS->ProcessStartTime(lock));
    }

    gPS->SetIsPaused(lock, false);
  }

  // We don't call stack->stopJSSampling() here; there's no point doing
  // that for a JS thread that is in the process of disappearing.

  stack->mContext = nullptr;
}

// END externally visible functions
////////////////////////////////////////////////////////////////////////
