/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
#include "nsIProfileSaveEvent.h"
#include "nsIXULAppInfo.h"
#include "nsIXULRuntime.h"
#include "nsDirectoryServiceUtils.h"
#include "nsDirectoryServiceDefs.h"
#include "nsXULAppAPI.h"
#include "nsProfilerStartParams.h"
#include "mozilla/Services.h"
#include "nsThreadUtils.h"
#include "ProfileGatherer.h"
#include "ProfilerMarkers.h"
#include "shared-libraries.h"

#ifdef MOZ_TASK_TRACER
#include "GeckoTaskTracer.h"
#endif

#if defined(PROFILE_JAVA)
# include "FennecJNINatives.h"
# include "FennecJNIWrappers.h"
#endif

#if defined(MOZ_PROFILING) && \
    (defined(GP_OS_windows) || defined(GP_OS_darwin))
# define USE_NS_STACKWALK
#endif

// This should also work on ARM Linux, but not tested there yet.
#if defined(GP_arm_android)
# define USE_EHABI_STACKWALK
# include "EHABIStackWalk.h"
#endif

#if defined(GP_PLAT_amd64_linux) || defined(GP_PLAT_x86_linux)
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
typedef CONTEXT tickcontext_t;
#elif defined(GP_OS_linux) || defined(GP_OS_android)
#include <ucontext.h>
typedef ucontext_t tickcontext_t;
#endif

using namespace mozilla;

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

MOZ_THREAD_LOCAL(PseudoStack *) tlsPseudoStack;

static Sampler* gSampler;

static std::vector<ThreadInfo*>* gRegisteredThreads = nullptr;
static mozilla::StaticMutex gRegisteredThreadsMutex;

// All accesses to gGatherer are on the main thread, so no locking is needed.
static StaticRefPtr<mozilla::ProfileGatherer> gGatherer;

// Always used in conjunction with gRegisteredThreads.
static ProfileBuffer* gBuffer;

// gThreadNameFilters is accessed from multiple threads. All accesses to it
// must be guarded by gThreadNameFiltersMutex.
static Vector<std::string> gThreadNameFilters;
static mozilla::StaticMutex gThreadNameFiltersMutex;

// All accesses to gFeatures are on the main thread, so no locking is needed.
static Vector<std::string> gFeatures;

// All accesses to gEntries are on the main thread, so no locking is needed.
static int gEntries = 0;

// All accesses to gInterval are on the main thread, so no locking is needed.
static double gInterval = 0;

// XXX: These two variables are used extensively both on and off the main
// thread. It's possible that code that checks them then unsafely assumes their
// values don't subsequently change.
static Atomic<bool> gIsActive(false);
static Atomic<bool> gIsPaused(false);

// We need to track whether we've been initialized otherwise
// we end up using tlsStack without initializing it.
// Because tlsStack is totally opaque to us we can't reuse
// it as the flag itself.
bool stack_key_initialized;

// XXX: This is set by profiler_init() and profiler_start() on the main thread.
// It is read off the main thread, e.g. by Tick(). It might require more
// inter-thread synchronization than it currently has.
static mozilla::TimeStamp gStartTime;

// XXX: These are accessed by multiple threads and might require more
// inter-thread synchronization than they currently have.
static int gFrameNumber = 0;
static int gLastFrameNumber = 0;

static int gInitCount = 0; // Each init must have a matched shutdown.

static bool gIsProfiling = false; // is raced on

// All accesses to these are on the main thread, so no locking is needed.
static bool gProfileJava = false;
static bool gProfileJS = false;
static bool gTaskTracer = false;

// XXX: These are all accessed by multiple threads and might require more
// inter-thread synchronization than they currently have.
static Atomic<bool> gAddLeafAddresses(false);
static Atomic<bool> gDisplayListDump(false);
static Atomic<bool> gLayersDump(false);
static Atomic<bool> gProfileGPU(false);
static Atomic<bool> gProfileMemory(false);
static Atomic<bool> gProfileRestyle(false);
static Atomic<bool> gProfileThreads(false);
static Atomic<bool> gUseStackWalk(false);

/* we don't need to worry about overflow because we only treat the
 * case of them being the same as special. i.e. we only run into
 * a problem if 2^32 events happen between samples that we need
 * to know are associated with different events */

// Values harvested from env vars, that control the profiler.
static int gEnvVarEntries;    /* how many entries do we store? */
static int gEnvVarInterval;   /* in milliseconds */

static mozilla::StaticAutoPtr<mozilla::ProfilerIOInterposeObserver>
                                                            gInterposeObserver;

// The name that identifies the gecko thread for calls to
// profiler_register_thread.
static const char* gGeckoThreadName = "GeckoMain";

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

// TickSample captures the information collected for each sample.
class TickSample {
public:
  TickSample()
    : pc(NULL)
    , sp(NULL)
    , fp(NULL)
    , lr(NULL)
    , context(NULL)
    , isSamplingCurrentThread(false)
    , threadInfo(nullptr)
    , rssMemory(0)
    , ussMemory(0)
  {}

  void PopulateContext(void* aContext);

  Address pc;  // Instruction pointer.
  Address sp;  // Stack pointer.
  Address fp;  // Frame pointer.
  Address lr;  // ARM link register
  void* context;   // The context from the signal handler, if available. On
                   // Win32 this may contain the windows thread context.
  bool isSamplingCurrentThread;
  ThreadInfo* threadInfo;
  mozilla::TimeStamp timestamp;
  int64_t rssMemory;
  int64_t ussMemory;
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
AddPseudoEntry(ProfileBuffer* aBuffer, volatile js::ProfileEntry& entry,
               PseudoStack* stack, void* lastpc)
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

  if (entry.isCopyLabel()) {
    // Store the string using 1 or more EmbeddedString tags.
    // That will happen to the preceding tag.
    AddDynamicCodeLocationTag(aBuffer, sampleLabel);
    if (entry.isJs()) {
      JSScript* script = entry.script();
      if (script) {
        if (!entry.pc()) {
          // The JIT only allows the top-most entry to have a nullptr pc.
          MOZ_ASSERT(&entry == &stack->mStack[stack->stackSize() - 1]);

          // If stack-walking was disabled, then that's just unfortunate.
          if (lastpc) {
            jsbytecode* jspc = js::ProfilingGetPC(stack->mContext, script,
                                                  lastpc);
            if (jspc) {
              lineno = JS_PCToLineNumber(script, jspc);
            }
          }
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
MergeStacksIntoProfile(ProfileBuffer* aBuffer, TickSample* aSample,
                       NativeStack& aNativeStack)
{
  PseudoStack* pseudoStack = aSample->threadInfo->Stack();
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
  startBufferGen = aSample->isSamplingCurrentThread
                 ? UINT32_MAX
                 : aBuffer->mGeneration;
  uint32_t jsCount = 0;
  JS::ProfilingFrameIterator::Frame jsFrames[1000];

  // Only walk jit stack if profiling frame iterator is turned on.
  if (pseudoStack->mContext &&
      JS::IsProfilingEnabledForContext(pseudoStack->mContext)) {
    AutoWalkJSStack autoWalkJSStack;
    const uint32_t maxFrames = mozilla::ArrayLength(jsFrames);

    if (aSample && autoWalkJSStack.walkAllowed) {
      JS::ProfilingFrameIterator::RegisterState registerState;
      registerState.pc = aSample->pc;
      registerState.sp = aSample->sp;
      registerState.lr = aSample->lr;

      JS::ProfilingFrameIterator jsIter(pseudoStack->mContext,
                                        registerState,
                                        startBufferGen);
      for (; jsCount < maxFrames && !jsIter.done(); ++jsIter) {
        // See note below regarding 'J' entries.
        if (aSample->isSamplingCurrentThread || jsIter.isWasm()) {
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
      AddPseudoEntry(aBuffer, pseudoFrame, pseudoStack, nullptr);
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
      if (aSample->isSamplingCurrentThread ||
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
  // Do not do this for synchronous sampling, which create their own
  // ProfileBuffers.
  if (!aSample->isSamplingCurrentThread && pseudoStack->mContext) {
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
DoNativeBacktrace(ProfileBuffer* aBuffer, TickSample* aSample)
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
  StackWalkCallback(/* frameNum */ 0, aSample->pc, aSample->sp, &nativeStack);

  uint32_t maxFrames = uint32_t(nativeStack.size - nativeStack.count);

#if defined(GP_OS_darwin) || (defined(GP_PLAT_x86_windows))
  void* stackEnd = aSample->threadInfo->StackTop();
  if (aSample->fp >= aSample->sp && aSample->fp <= stackEnd) {
    FramePointerStackWalk(StackWalkCallback, /* skipFrames */ 0, maxFrames,
                          &nativeStack, reinterpret_cast<void**>(aSample->fp),
                          stackEnd);
  }
#else
  // Win64 always omits frame pointers so for it we use the slower
  // MozStackWalk().
  uintptr_t thread = GetThreadHandle(aSample->threadInfo->GetPlatformData());
  MOZ_ASSERT(thread);
  MozStackWalk(StackWalkCallback, /* skipFrames */ 0, maxFrames, &nativeStack,
               thread, /* platformData */ nullptr);
#endif

  MergeStacksIntoProfile(aBuffer, aSample, nativeStack);
}
#endif

#ifdef USE_EHABI_STACKWALK
static void
DoNativeBacktrace(Profile* aBuffer, TickSample* aSample)
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
    &reinterpret_cast<ucontext_t*>(aSample->context)->uc_mcontext;
  mcontext_t savedContext;
  PseudoStack* pseudoStack = aInfo.Stack();

  nativeStack.count = 0;

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
                                      aInfo.StackTop(),
                                      sp_array + nativeStack.count,
                                      pc_array + nativeStack.count,
                                      nativeStack.size - nativeStack.count);

  MergeStacksIntoProfile(aInfo, aSample, nativeStack);
}
#endif

#ifdef USE_LUL_STACKWALK
static void
DoNativeBacktrace(ProfileBuffer* aBuffer, TickSample* aSample)
{
  const mcontext_t* mc =
    &reinterpret_cast<ucontext_t*>(aSample->context)->uc_mcontext;

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
    uintptr_t end = reinterpret_cast<uintptr_t>(aSample->threadInfo->StackTop());
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
      memcpy(&stackImg.mContents[0], (void*)start, nToCopy);
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
  size_t scannedFramesAcquired = 0;
  gLUL->Unwind(&framePCs[0], &frameSPs[0],
               &framesUsed, &scannedFramesAcquired,
               framesAvail, scannedFramesAllowed,
               &startRegs, &stackImg );

  NativeStack nativeStack = {
    reinterpret_cast<void**>(framePCs),
    reinterpret_cast<void**>(frameSPs),
    mozilla::ArrayLength(framePCs),
    0
  };

  nativeStack.count = framesUsed;

  MergeStacksIntoProfile(aBuffer, aSample, nativeStack);

  // Update stats in the LUL stats object.  Unfortunately this requires
  // three global memory operations.
  gLUL->mStats.mContext += 1;
  gLUL->mStats.mCFI     += framesUsed - 1 - scannedFramesAcquired;
  gLUL->mStats.mScanned += scannedFramesAcquired;
}
#endif

static void
DoSampleStackTrace(ProfileBuffer* aBuffer, TickSample* aSample,
                   bool aAddLeafAddresses)
{
  NativeStack nativeStack = { nullptr, nullptr, 0, 0 };
  MergeStacksIntoProfile(aBuffer, aSample, nativeStack);

  if (aSample && aAddLeafAddresses) {
    aBuffer->addTag(ProfileBufferEntry::NativeLeafAddr((void*)aSample->pc));
  }
}

// This function is called for each sampling period with the current program
// counter. It is called within a signal and so must be re-entrant.
static void
Tick(ProfileBuffer* aBuffer, TickSample* aSample)
{
  ThreadInfo& threadInfo = *aSample->threadInfo;

  aBuffer->addTag(ProfileBufferEntry::ThreadId(threadInfo.ThreadId()));

  mozilla::TimeDuration delta = aSample->timestamp - gStartTime;
  aBuffer->addTag(ProfileBufferEntry::Time(delta.ToMilliseconds()));

  PseudoStack* stack = threadInfo.Stack();

#if defined(USE_NS_STACKWALK) || defined(USE_EHABI_STACKWALK) || \
    defined(USE_LUL_STACKWALK)
  if (gUseStackWalk) {
    DoNativeBacktrace(aBuffer, aSample);
  } else {
    DoSampleStackTrace(aBuffer, aSample, gAddLeafAddresses);
  }
#else
  DoSampleStackTrace(aBuffer, aSample, gAddLeafAddresses);
#endif

  // Don't process the PeudoStack's markers if we're synchronously sampling the
  // current thread.
  if (!aSample->isSamplingCurrentThread) {
    ProfilerMarkerLinkedList* pendingMarkersList = stack->getPendingMarkers();
    while (pendingMarkersList && pendingMarkersList->peek()) {
      ProfilerMarker* marker = pendingMarkersList->popHead();
      aBuffer->addStoredMarker(marker);
      aBuffer->addTag(ProfileBufferEntry::Marker(marker));
    }
  }

  if (threadInfo.GetThreadResponsiveness()->HasData()) {
    mozilla::TimeDuration delta =
      threadInfo.GetThreadResponsiveness()->GetUnresponsiveDuration(
        aSample->timestamp);
    aBuffer->addTag(ProfileBufferEntry::Responsiveness(delta.ToMilliseconds()));
  }

  // rssMemory is equal to 0 when we are not recording.
  if (aSample->rssMemory != 0) {
    aBuffer->addTag(ProfileBufferEntry::ResidentMemory(
      static_cast<double>(aSample->rssMemory)));
  }

  // ussMemory is equal to 0 when we are not recording.
  if (aSample->ussMemory != 0) {
    aBuffer->addTag(ProfileBufferEntry::UnsharedMemory(
      static_cast<double>(aSample->ussMemory)));
  }

  if (gLastFrameNumber != gFrameNumber) {
    aBuffer->addTag(ProfileBufferEntry::FrameNumber(gFrameNumber));
    gLastFrameNumber = gFrameNumber;
  }
}

// END tick/unwinding code
////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////
// BEGIN saving/streaming code

class ProfileSaveEvent final : public nsIProfileSaveEvent
{
public:
  typedef void (*AddSubProfileFunc)(const char* aProfile, void* aClosure);
  NS_DECL_ISUPPORTS

  ProfileSaveEvent(AddSubProfileFunc aFunc, void* aClosure)
    : mFunc(aFunc)
    , mClosure(aClosure)
  {}

  NS_IMETHOD AddSubProfile(const char* aProfile) override {
    mFunc(aProfile, mClosure);
    return NS_OK;
  }

private:
  ~ProfileSaveEvent() {}

  AddSubProfileFunc mFunc;
  void* mClosure;
};

NS_IMPL_ISUPPORTS(ProfileSaveEvent, nsIProfileSaveEvent)

static void
AddSharedLibraryInfoToStream(std::ostream& aStream, const SharedLibrary& aLib)
{
  aStream << "{";
  aStream << "\"start\":" << aLib.GetStart();
  aStream << ",\"end\":" << aLib.GetEnd();
  aStream << ",\"offset\":" << aLib.GetOffset();
  aStream << ",\"name\":\"" << aLib.GetNativeDebugName() << "\"";
  const std::string& breakpadId = aLib.GetBreakpadId();
  aStream << ",\"breakpadId\":\"" << breakpadId << "\"";
  aStream << "}";
}

static std::string
GetSharedLibraryInfoStringInternal()
{
  SharedLibraryInfo info = SharedLibraryInfo::GetInfoForSelf();
  if (info.GetSize() == 0) {
    return "[]";
  }

  std::ostringstream os;
  os << "[";
  AddSharedLibraryInfoToStream(os, info.GetEntry(0));

  for (size_t i = 1; i < info.GetSize(); i++) {
    os << ",";
    AddSharedLibraryInfoToStream(os, info.GetEntry(i));
  }

  os << "]";
  return os.str();
}

static void
StreamTaskTracer(SpliceableJSONWriter& aWriter)
{
#ifdef MOZ_TASK_TRACER
  aWriter.StartArrayProperty("data");
  {
    UniquePtr<nsTArray<nsCString>> data =
      mozilla::tasktracer::GetLoggedData(gStartTime);
    for (uint32_t i = 0; i < data->Length(); ++i) {
      aWriter.StringElement((data->ElementAt(i)).get());
    }
  }
  aWriter.EndArray();

  aWriter.StartArrayProperty("threads");
  {
    StaticMutexAutoLock lock(gRegisteredThreadsMutex);

    for (size_t i = 0; i < gRegisteredThreads->size(); i++) {
      // Thread meta data
      ThreadInfo* info = gRegisteredThreads->at(i);
      aWriter.StartObjectElement();
      {
        if (XRE_GetProcessType() == GeckoProcessType_Plugin) {
          // TODO Add the proper plugin name
          aWriter.StringProperty("name", "Plugin");
        } else {
          aWriter.StringProperty("name", info->Name());
        }
        aWriter.IntProperty("tid", static_cast<int>(info->ThreadId()));
      }
      aWriter.EndObject();
    }
  }
  aWriter.EndArray();

  aWriter.DoubleProperty(
    "start", static_cast<double>(mozilla::tasktracer::GetStartTime()));
#endif
}

static void
StreamMetaJSCustomObject(SpliceableJSONWriter& aWriter)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  aWriter.IntProperty("version", 3);
  aWriter.DoubleProperty("interval", gInterval);
  aWriter.IntProperty("stackwalk", gUseStackWalk);

#ifdef DEBUG
  aWriter.IntProperty("debug", 1);
#else
  aWriter.IntProperty("debug", 0);
#endif

  aWriter.IntProperty("gcpoison", JS::IsGCPoisoning() ? 1 : 0);

  bool asyncStacks = Preferences::GetBool("javascript.options.asyncstack");
  aWriter.IntProperty("asyncstack", asyncStacks);

  mozilla::TimeDuration delta = mozilla::TimeStamp::Now() - gStartTime;
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

struct SubprocessClosure
{
  explicit SubprocessClosure(SpliceableJSONWriter* aWriter)
    : mWriter(aWriter)
  {}

  SpliceableJSONWriter* mWriter;
};

static void
SubProcessCallback(const char* aProfile, void* aClosure)
{
  // Called by the observer to get their profile data included as a sub profile.
  SubprocessClosure* closure = (SubprocessClosure*)aClosure;

  // Add the string profile into the profile.
  closure->mWriter->StringElement(aProfile);
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
StreamJSON(SpliceableJSONWriter& aWriter, double aSinceTime)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  aWriter.Start(SpliceableJSONWriter::SingleLineStyle);
  {
    // Put shared library info
    aWriter.StringProperty("libs",
                           GetSharedLibraryInfoStringInternal().c_str());

    // Put meta data
    aWriter.StartObjectProperty("meta");
    {
      StreamMetaJSCustomObject(aWriter);
    }
    aWriter.EndObject();

    // Data of TaskTracer doesn't belong in the circular buffer.
    if (gTaskTracer) {
      aWriter.StartObjectProperty("tasktracer");
      StreamTaskTracer(aWriter);
      aWriter.EndObject();
    }

    // Lists the samples for each thread profile
    aWriter.StartArrayProperty("threads");
    {
      gIsPaused = true;

      {
        StaticMutexAutoLock lock(gRegisteredThreadsMutex);

        for (size_t i = 0; i < gRegisteredThreads->size(); i++) {
          // Thread not being profiled, skip it
          ThreadInfo* info = gRegisteredThreads->at(i);
          if (!info->HasProfile()) {
            continue;
          }

          // Note that we intentionally include thread profiles which
          // have been marked for pending delete.

          MutexAutoLock lock(info->GetMutex());

          info->StreamJSON(gBuffer, aWriter, gStartTime, aSinceTime);
        }
      }

      if (CanNotifyObservers()) {
        // Send a event asking any subprocesses (plugins) to
        // give us their information
        SubprocessClosure closure(&aWriter);
        nsCOMPtr<nsIObserverService> os =
          mozilla::services::GetObserverService();
        if (os) {
          RefPtr<ProfileSaveEvent> pse =
            new ProfileSaveEvent(SubProcessCallback, &closure);
          os->NotifyObservers(pse, "profiler-subprocess", nullptr);
        }
      }

#if defined(PROFILE_JAVA)
      if (gProfileJava) {
        java::GeckoJavaSampler::Pause();

        aWriter.Start();
        {
          BuildJavaThreadJSObject(aWriter);
        }
        aWriter.End();

        java::GeckoJavaSampler::Unpause();
      }
#endif

      gIsPaused = false;
    }
    aWriter.EndArray();
  }
  aWriter.End();
}

UniquePtr<char[]>
ToJSON(double aSinceTime)
{
  SpliceableChunkedJSONWriter b;
  StreamJSON(b, aSinceTime);
  return b.WriteFunc()->CopyData();
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
                                const TimeStamp& aStartTime,
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
          mPayload->StreamPayload(aWriter, aStartTime, aUniqueStacks);
      }
      aWriter.EndObject();
    }
  }
  aWriter.EndArray();
}

// Verbosity control for the profiler.  The aim is to check env var
// MOZ_PROFILER_VERBOSE only once.

enum class Verbosity : int8_t { UNCHECKED, NOTVERBOSE, VERBOSE };

// Raced on, potentially
static Verbosity gVerbosity = Verbosity::UNCHECKED;

bool
profiler_verbose()
{
  if (gVerbosity == Verbosity::UNCHECKED) {
    gVerbosity = getenv("MOZ_PROFILER_VERBOSE")
               ? Verbosity::VERBOSE
               : Verbosity::NOTVERBOSE;
  }

  return gVerbosity == Verbosity::VERBOSE;
}

static bool
set_profiler_interval(const char* aInterval)
{
  if (aInterval) {
    errno = 0;
    long int n = strtol(aInterval, nullptr, 10);
    if (errno == 0 && 1 <= n && n <= 1000) {
      gEnvVarInterval = n;
      return true;
    }
    return false;
  }

  return true;
}

static bool
set_profiler_entries(const char* aEntries)
{
  if (aEntries) {
    errno = 0;
    long int n = strtol(aEntries, nullptr, 10);
    if (errno == 0 && n > 0) {
      gEnvVarEntries = n;
      return true;
    }
    return false;
  }

  return true;
}

static bool
is_native_unwinding_avail()
{
# if defined(HAVE_NATIVE_UNWIND)
  return true;
#else
  return false;
#endif
}

static void
profiler_usage(int aExitCode)
{
  // Force-enable verbosity so that LOG prints something.
  gVerbosity = Verbosity::VERBOSE;

  LOG ("");
  LOG ("Environment variable usage:");
  LOG ("");
  LOG ("  MOZ_PROFILER_HELP");
  LOG ("  If set to any value, prints this message.");
  LOG ("");
  LOG ("  MOZ_PROFILER_ENTRIES=<1..>      (count)");
  LOG ("  If unset, platform default is used.");
  LOG ("");
  LOG ("  MOZ_PROFILER_INTERVAL=<1..1000> (milliseconds)");
  LOG ("  If unset, platform default is used.");
  LOG ("");
  LOG ("  MOZ_PROFILER_VERBOSE");
  LOG ("  If set to any value, increases verbosity (recommended).");
  LOG ("");
  LOG ("  MOZ_PROFILER_LUL_TEST");
  LOG ("  If set to any value, runs LUL unit tests at startup of");
  LOG ("  the unwinder thread, and prints a short summary of ");
  LOG ("  results.");
  LOG ("");
  LOGF("  This platform %s native unwinding.",
       is_native_unwinding_avail() ? "supports" : "does not support");
  LOG ("");

  exit(aExitCode);
}

// Read env vars at startup, so as to set:
//   gEnvVarEntries, gEnvVarInterval
static void
ReadProfilerEnvVars()
{
  const char* help     = getenv("MOZ_PROFILER_HELP");
  const char* entries  = getenv("MOZ_PROFILER_ENTRIES");
  const char* interval = getenv("MOZ_PROFILER_INTERVAL");

  if (help) {
    profiler_usage(0); // terminates execution
  }

  if (!set_profiler_entries(entries) ||
      !set_profiler_interval(interval)) {
    profiler_usage(1); // terminates execution
  }

  LOG ("");
  LOGF("entries  = %d (zero means \"platform default\")",
       gEnvVarEntries);
  LOGF("interval = %d ms (zero means \"platform default\")",
       gEnvVarInterval);
  LOG ("");
}

static bool
is_main_thread_name(const char* aName)
{
  return aName && (strcmp(aName, gGeckoThreadName) == 0);
}

#ifdef HAVE_VA_COPY
#define VARARGS_ASSIGN(foo, bar)     VA_COPY(foo,bar)
#elif defined(HAVE_VA_LIST_AS_ARRAY)
#define VARARGS_ASSIGN(foo, bar)     foo[0] = bar[0]
#else
#define VARARGS_ASSIGN(foo, bar)     (foo) = (bar)
#endif

void
profiler_log(const char* aStr)
{
  // This function runs both on and off the main thread.

  profiler_tracing("log", aStr, TRACING_EVENT);
}

void
profiler_log(const char* aFmt, va_list aArgs)
{
  // This function runs both on and off the main thread.

  if (profiler_is_active()) {
    // nsAutoCString AppendPrintf would be nicer but
    // this is mozilla external code
    char buf[2048];
    va_list argsCpy;
    VARARGS_ASSIGN(argsCpy, aArgs);
    int required = VsprintfLiteral(buf, aFmt, argsCpy);
    va_end(argsCpy);

    if (required < 0) {
      return; // silently drop for now
    } else if (required < 2048) {
      profiler_tracing("log", buf, TRACING_EVENT);
    } else {
      char* heapBuf = new char[required+1];
      va_list argsCpy;
      VARARGS_ASSIGN(argsCpy, aArgs);
      vsnprintf(heapBuf, required+1, aFmt, argsCpy);
      va_end(argsCpy);
      // EVENT_BACKTRACE could be used to get a source
      // for all log events. This could be a runtime
      // flag later.
      profiler_tracing("log", heapBuf, TRACING_EVENT);
      delete[] heapBuf;
    }
  }
}

////////////////////////////////////////////////////////////////////////
// BEGIN externally visible functions

MOZ_DEFINE_MALLOC_SIZE_OF(GeckoProfilerMallocSizeOf);

NS_IMETHODIMP
GeckoProfilerReporter::CollectReports(nsIHandleReportCallback* aHandleReport,
                                      nsISupports* aData, bool aAnonymize)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  size_t n = 0;
  {
    StaticMutexAutoLock lock(gRegisteredThreadsMutex);

    for (uint32_t i = 0; i < gRegisteredThreads->size(); i++) {
      ThreadInfo* info = gRegisteredThreads->at(i);

      n += info->SizeOfIncludingThis(GeckoProfilerMallocSizeOf);
    }
  }
  MOZ_COLLECT_REPORT(
    "explicit/profiler/thread-info", KIND_HEAP, UNITS_BYTES, n,
    "Memory used by the Gecko Profiler's ThreadInfo objects.");

  if (gBuffer) {
    n = gBuffer->SizeOfIncludingThis(GeckoProfilerMallocSizeOf);
    MOZ_COLLECT_REPORT(
      "explicit/profiler/profile-buffer", KIND_HEAP, UNITS_BYTES, n,
      "Memory used by the Gecko Profiler's ProfileBuffer object.");
  }

#if defined(USE_LUL_STACKWALK)
  n = gLUL ? gLUL->SizeOfIncludingThis(GeckoProfilerMallocSizeOf) : 0;
  MOZ_COLLECT_REPORT(
    "explicit/profiler/lul", KIND_HEAP, UNITS_BYTES, n,
    "Memory used by LUL, a stack unwinder used by the Gecko Profiler.");
#endif

  // Measurement of the following things may be added later if DMD finds it
  // is worthwhile:
  // - gThreadNameFilters
  // - gFeatures

  return NS_OK;
}

NS_IMPL_ISUPPORTS(GeckoProfilerReporter, nsIMemoryReporter)

static bool
ThreadSelected(const char* aThreadName)
{
  StaticMutexAutoLock lock(gThreadNameFiltersMutex);

  if (gThreadNameFilters.empty()) {
    return true;
  }

  std::string name = aThreadName;
  std::transform(name.begin(), name.end(), name.begin(), ::tolower);

  for (uint32_t i = 0; i < gThreadNameFilters.length(); ++i) {
    std::string filter = gThreadNameFilters[i];
    std::transform(filter.begin(), filter.end(), filter.begin(), ::tolower);

    // Crude, non UTF-8 compatible, case insensitive substring search
    if (name.find(filter) != std::string::npos) {
      return true;
    }
  }

  return false;
}

static void
MaybeSetProfile(ThreadInfo* aInfo)
{
  if ((aInfo->IsMainThread() || gProfileThreads) &&
      ThreadSelected(aInfo->Name())) {
    aInfo->SetHasProfile();
  }
}

static void
RegisterCurrentThread(const char* aName, PseudoStack* aPseudoStack,
                      bool aIsMainThread, void* stackTop)
{
  StaticMutexAutoLock lock(gRegisteredThreadsMutex);

  if (!gRegisteredThreads) {
    return;
  }

  Thread::tid_t id = Thread::GetCurrentId();

  for (uint32_t i = 0; i < gRegisteredThreads->size(); i++) {
    ThreadInfo* info = gRegisteredThreads->at(i);
    if (info->ThreadId() == id && !info->IsPendingDelete()) {
      // Thread already registered. This means the first unregister will be
      // too early.
      MOZ_ASSERT(false);
      return;
    }
  }

  ThreadInfo* info =
    new ThreadInfo(aName, id, aIsMainThread, aPseudoStack, stackTop);

  MaybeSetProfile(info);

  gRegisteredThreads->push_back(info);
}

// Platform-specific init/start/stop actions.
static void PlatformInit();
static void PlatformStart(double aInterval);
static void PlatformStop();

void
profiler_init(void* stackTop)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  gInitCount++;

  if (stack_key_initialized)
    return;

#ifdef MOZ_TASK_TRACER
  mozilla::tasktracer::InitTaskTracer();
#endif

  LOG("BEGIN profiler_init");
  if (!tlsPseudoStack.init()) {
    LOG("Failed to init.");
    return;
  }
  bool ignore;
  gStartTime = mozilla::TimeStamp::ProcessCreation(ignore);

  stack_key_initialized = true;

  {
    StaticMutexAutoLock lock(gRegisteredThreadsMutex);

    gRegisteredThreads = new std::vector<ThreadInfo*>();
  }

  // (Linux-only) We could create the gLUL object and read unwind info into it
  // at this point. That would match the lifetime implied by destruction of it
  // in profiler_shutdown() just below. However, that gives a big delay on
  // startup, even if no profiling is actually to be done. So, instead, gLUL is
  // created on demand at the first call to PlatformStart().

  PseudoStack* stack = new PseudoStack();
  tlsPseudoStack.set(stack);

  bool isMainThread = true;
  RegisterCurrentThread(gGeckoThreadName, stack, isMainThread, stackTop);

  // Read settings from environment variables.
  ReadProfilerEnvVars();

  // Platform-specific initialization.
  PlatformInit();

  set_stderr_callback(profiler_log);

#if defined(PROFILE_JAVA)
  if (mozilla::jni::IsFennec()) {
    GeckoJavaSampler::Init();
  }
#endif

  // We can't open pref so we use an environment variable
  // to know if we should trigger the profiler on startup
  // NOTE: Default
  const char *val = getenv("MOZ_PROFILER_STARTUP");
  if (!val || !*val) {
    return;
  }

  const char* features[] = { "js", "leaf", "threads"
#if defined(HAVE_NATIVE_UNWIND)
                           , "stackwalk"
#endif
#if defined(PROFILE_JAVA)
                           , "java"
#endif
                           };

  const char* threadFilters[] = { "GeckoMain", "Compositor" };

  profiler_start(PROFILE_DEFAULT_ENTRIES, PROFILE_DEFAULT_INTERVAL,
                 features, MOZ_ARRAY_LENGTH(features),
                 threadFilters, MOZ_ARRAY_LENGTH(threadFilters));
  LOG("END   profiler_init");
}

void
profiler_shutdown()
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  gInitCount--;

  if (gInitCount > 0)
    return;

  // Save the profile on shutdown if requested.
  if (gSampler) {
    const char* filename = getenv("MOZ_PROFILER_SHUTDOWN");
    if (filename) {
      profiler_save_profile_to_file(filename);
    }
  }

  profiler_stop();

  set_stderr_callback(nullptr);

  {
    StaticMutexAutoLock lock(gRegisteredThreadsMutex);

    while (gRegisteredThreads->size() > 0) {
      delete gRegisteredThreads->back();
      gRegisteredThreads->pop_back();
    }

    // UnregisterThread can be called after shutdown in XPCShell. Thus we need
    // to point to null to ignore such a call after shutdown.
    delete gRegisteredThreads;
    gRegisteredThreads = nullptr;
  }

#if defined(USE_LUL_STACKWALK)
  // Delete the gLUL object, if it actually got created.
  if (gLUL) {
    delete gLUL;
    gLUL = nullptr;
  }
#endif

  // We just destroyed all the ThreadInfos in gRegisteredThreads, so it is safe
  // the delete the PseudoStack.
  delete tlsPseudoStack.get();
  tlsPseudoStack.set(nullptr);

#ifdef MOZ_TASK_TRACER
  mozilla::tasktracer::ShutdownTaskTracer();
#endif
}

mozilla::UniquePtr<char[]>
profiler_get_profile(double aSinceTime)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  if (!gSampler) {
    return nullptr;
  }

  return ToJSON(aSinceTime);
}

JSObject*
profiler_get_profile_jsobject(JSContext *aCx, double aSinceTime)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  if (!gSampler) {
    return nullptr;
  }

  JS::RootedValue val(aCx);
  {
    UniquePtr<char[]> buf = ToJSON(aSinceTime);
    NS_ConvertUTF8toUTF16 js_string(nsDependentCString(buf.get()));
    auto buf16 = static_cast<const char16_t*>(js_string.get());
    MOZ_ALWAYS_TRUE(JS_ParseJSON(aCx, buf16, js_string.Length(), &val));
  }
  return &val.toObject();
}

void
profiler_get_profile_jsobject_async(double aSinceTime,
                                    mozilla::dom::Promise* aPromise)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  if (!gGatherer) {
    return;
  }

  gGatherer->Start(aSinceTime, aPromise);
}

void
profiler_save_profile_to_file_async(double aSinceTime, const char* aFileName)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  nsCString filename(aFileName);
  NS_DispatchToMainThread(NS_NewRunnableFunction([=] () {
    if (!gGatherer) {
      return;
    }

    gGatherer->Start(aSinceTime, filename);
  }));
}

void
profiler_get_start_params(int* aEntries, double* aInterval,
                          mozilla::Vector<const char*>* aFilters,
                          mozilla::Vector<const char*>* aFeatures)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  if (NS_WARN_IF(!aEntries) || NS_WARN_IF(!aInterval) ||
      NS_WARN_IF(!aFilters) || NS_WARN_IF(!aFeatures)) {
    return;
  }

  *aEntries = gEntries;
  *aInterval = gInterval;

  {
    StaticMutexAutoLock lock(gThreadNameFiltersMutex);

    MOZ_ALWAYS_TRUE(aFilters->resize(gThreadNameFilters.length()));
    for (uint32_t i = 0; i < gThreadNameFilters.length(); ++i) {
      (*aFilters)[i] = gThreadNameFilters[i].c_str();
    }
  }

  MOZ_ALWAYS_TRUE(aFeatures->resize(gFeatures.length()));
  for (size_t i = 0; i < gFeatures.length(); ++i) {
    (*aFeatures)[i] = gFeatures[i].c_str();
  }
}

void
profiler_will_gather_OOP_profile()
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  if (!gGatherer) {
    return;
  }

  gGatherer->WillGatherOOPProfile();
}

void
profiler_gathered_OOP_profile()
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  if (!gGatherer) {
    return;
  }

  gGatherer->GatheredOOPProfile();
}

void
profiler_OOP_exit_profile(const nsCString& aProfile)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  if (!gGatherer) {
    return;
  }

  gGatherer->OOPExitProfile(aProfile);
}

void
profiler_save_profile_to_file(const char* aFilename)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  if (!gSampler) {
    return;
  }

  std::ofstream stream;
  stream.open(aFilename);
  if (stream.is_open()) {
    SpliceableJSONWriter w(mozilla::MakeUnique<OStreamJSONWriteFunc>(stream));
    StreamJSON(w, /* sinceTime */ 0);
    stream.close();
    LOGF("Saved to %s", aFilename);
  } else {
    LOG("Fail to open profile log file.");
  }
}

const char**
profiler_get_features()
{
  // This function currently only used on the main thread, but that restriction
  // (and this assertion) could be removed trivially because it doesn't touch
  // data that requires locking.
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  static const char* features[] = {
#if defined(MOZ_PROFILING) && defined(HAVE_NATIVE_UNWIND)
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

  if (!stack_key_initialized)
    return;

  if (!gBuffer) {
    return;
  }

  *aCurrentPosition = gBuffer->mWritePos;
  *aEntries = gEntries;
  *aGeneration = gBuffer->mGeneration;
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

// XXX: an empty class left behind after refactoring. Will be removed soon.
class Sampler {};

// Values are only honored on the first start
void
profiler_start(int aEntries, double aInterval,
               const char** aFeatures, uint32_t aFeatureCount,
               const char** aThreadNameFilters, uint32_t aFilterCount)

{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  LOG("BEGIN profiler_start");

  if (!stack_key_initialized)
    profiler_init(nullptr);

  /* If the sampling interval was set using env vars, use that
     in preference to anything else. */
  if (gEnvVarInterval > 0)
    aInterval = gEnvVarInterval;

  /* If the entry count was set using env vars, use that, too: */
  if (gEnvVarEntries > 0)
    aEntries = gEnvVarEntries;

  // Reset the current state if the profiler is running
  profiler_stop();

  // Deep copy aThreadNameFilters. Must happen before the MaybeSetProfile()
  // call below.
  {
    StaticMutexAutoLock lock(gThreadNameFiltersMutex);

    MOZ_ALWAYS_TRUE(gThreadNameFilters.resize(aFilterCount));
    for (uint32_t i = 0; i < aFilterCount; ++i) {
      gThreadNameFilters[i] = aThreadNameFilters[i];
    }
  }

  // Deep copy aFeatures.
  MOZ_ALWAYS_TRUE(gFeatures.resize(aFeatureCount));
  for (uint32_t i = 0; i < aFeatureCount; ++i) {
    gFeatures[i] = aFeatures[i];
  }

  bool mainThreadIO = hasFeature(aFeatures, aFeatureCount, "mainthreadio");
  bool privacyMode  = hasFeature(aFeatures, aFeatureCount, "privacy");

#if defined(PROFILE_JAVA)
  gProfileJava = mozilla::jni::IsFennec() &&
                      hasFeature(aFeatures, aFeatureCount, "java");
#endif
  gProfileJS   = hasFeature(aFeatures, aFeatureCount, "js");
  gTaskTracer  = hasFeature(aFeatures, aFeatureCount, "tasktracer");

  gAddLeafAddresses = hasFeature(aFeatures, aFeatureCount, "leaf");
  gDisplayListDump  = hasFeature(aFeatures, aFeatureCount, "displaylistdump");
  gLayersDump       = hasFeature(aFeatures, aFeatureCount, "layersdump");
  gProfileGPU       = hasFeature(aFeatures, aFeatureCount, "gpu");
  gProfileMemory    = hasFeature(aFeatures, aFeatureCount, "memory");
  gProfileRestyle   = hasFeature(aFeatures, aFeatureCount, "restyle");
  // Profile non-main threads if we have a filter, because users sometimes ask
  // to filter by a list of threads but forget to explicitly request.
  // gProfileThreads must be set before the MaybeSetProfile() call below.
  gProfileThreads   = hasFeature(aFeatures, aFeatureCount, "threads") ||
                      aFilterCount > 0;
  gUseStackWalk     = hasFeature(aFeatures, aFeatureCount, "stackwalk");

  gEntries = aEntries ? aEntries : PROFILE_DEFAULT_ENTRIES;
  gInterval = aInterval ? aInterval : PROFILE_DEFAULT_INTERVAL;
  gBuffer = new ProfileBuffer(gEntries);
  gSampler = new Sampler();

  bool ignore;
  gStartTime = mozilla::TimeStamp::ProcessCreation(ignore);

  {
    StaticMutexAutoLock lock(gRegisteredThreadsMutex);

    // Set up profiling for each registered thread, if appropriate
    for (uint32_t i = 0; i < gRegisteredThreads->size(); i++) {
      ThreadInfo* info = gRegisteredThreads->at(i);

      MaybeSetProfile(info);
    }
  }

#ifdef MOZ_TASK_TRACER
  if (gTaskTracer) {
    mozilla::tasktracer::StartLogging();
  }
#endif

  gGatherer = new mozilla::ProfileGatherer();

  gIsActive = true;  // Must set this for PlatformStart() to work.
  gIsPaused = false;
  PlatformStart(gInterval);

  if (gProfileJS || privacyMode) {
    mozilla::StaticMutexAutoLock lock(gRegisteredThreadsMutex);

    for (uint32_t i = 0; i < gRegisteredThreads->size(); i++) {
      ThreadInfo* info = (*gRegisteredThreads)[i];
      if (info->IsPendingDelete() || !info->HasProfile()) {
        continue;
      }
      info->Stack()->reinitializeOnResume();
      if (gProfileJS) {
        info->Stack()->enableJSSampling();
      }
      if (privacyMode) {
        info->Stack()->mPrivacyMode = true;
      }
    }
  }

#if defined(PROFILE_JAVA)
  if (gProfileJava) {
    int javaInterval = aInterval;
    // Java sampling doesn't accuratly keep up with 1ms sampling
    if (javaInterval < 10) {
      aInterval = 10;
    }
    mozilla::java::GeckoJavaSampler::Start(javaInterval, 1000);
  }
#endif

  if (mainThreadIO) {
    if (!gInterposeObserver) {
      // Lazily create IO interposer observer
      gInterposeObserver = new mozilla::ProfilerIOInterposeObserver();
    }
    mozilla::IOInterposer::Register(mozilla::IOInterposeObserver::OpAll,
                                    gInterposeObserver);
  }

  gIsProfiling = true;

  if (CanNotifyObservers()) {
    nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
    if (os) {
      nsTArray<nsCString> featuresArray;
      nsTArray<nsCString> threadNameFiltersArray;

      for (size_t i = 0; i < aFeatureCount; ++i) {
        featuresArray.AppendElement(aFeatures[i]);
      }

      for (size_t i = 0; i < aFilterCount; ++i) {
        threadNameFiltersArray.AppendElement(aThreadNameFilters[i]);
      }

      nsCOMPtr<nsIProfilerStartParams> params =
        new nsProfilerStartParams(aEntries, aInterval, featuresArray,
                                  threadNameFiltersArray);

      os->NotifyObservers(params, "profiler-started", nullptr);
    }
  }

  LOG("END   profiler_start");
}

void
profiler_stop()
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  LOG("BEGIN profiler_stop");

  if (!stack_key_initialized)
    return;

  if (!gSampler) {
    LOG("END   profiler_stop-early");
    return;
  }

  bool disableJS = gProfileJS;

  {
    StaticMutexAutoLock lock(gThreadNameFiltersMutex);
    gThreadNameFilters.clear();
  }
  gFeatures.clear();

  gIsActive = false;  // Must clear this for PlatformStop() to work.
  gIsPaused = false;
  PlatformStop();

  gProfileJava      = false;
  gProfileJS        = false;
  gTaskTracer       = false;

  gAddLeafAddresses = false;
  gDisplayListDump  = false;
  gLayersDump       = false;
  gProfileGPU       = false;
  gProfileMemory    = false;
  gProfileRestyle   = false;
  gProfileThreads   = false;
  gUseStackWalk     = false;

  // Destroy ThreadInfo for all threads
  {
    StaticMutexAutoLock lock(gRegisteredThreadsMutex);

    for (uint32_t i = 0; i < gRegisteredThreads->size(); i++) {
      ThreadInfo* info = gRegisteredThreads->at(i);
      // We've stopped profiling. We no longer need to retain
      // information for an old thread.
      if (info->IsPendingDelete()) {
        // The stack was nulled when SetPendingDelete() was called.
        MOZ_ASSERT(!info->Stack());
        delete info;
        gRegisteredThreads->erase(gRegisteredThreads->begin() + i);
        i--;
      }
    }
  }

#ifdef MOZ_TASK_TRACER
  if (gTaskTracer) {
    mozilla::tasktracer::StopLogging();
  }
#endif

  delete gSampler;
  gSampler = nullptr;

  delete gBuffer;
  gBuffer = nullptr;

  gEntries = 0;
  gInterval = 0;

  // Cancel any in-flight async profile gatherering requests.
  gGatherer->Cancel();
  gGatherer = nullptr;

  if (disableJS) {
    PseudoStack *stack = tlsPseudoStack.get();
    MOZ_ASSERT(stack != nullptr);
    stack->disableJSSampling();
  }

  mozilla::IOInterposer::Unregister(mozilla::IOInterposeObserver::OpAll,
                                    gInterposeObserver);
  gInterposeObserver = nullptr;

  gIsProfiling = false;

  if (CanNotifyObservers()) {
    nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
    if (os)
      os->NotifyObservers(nullptr, "profiler-stopped", nullptr);
  }

  LOG("END   profiler_stop");
}

bool
profiler_is_paused()
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  if (!gSampler) {
    return false;
  }

  return gIsPaused;
}

void
profiler_pause()
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  if (!gSampler) {
    return;
  }

  gIsPaused = true;
  if (CanNotifyObservers()) {
    nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
    if (os) {
      os->NotifyObservers(nullptr, "profiler-paused", nullptr);
    }
  }
}

void
profiler_resume()
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  if (!gSampler) {
    return;
  }

  gIsPaused = false;
  if (CanNotifyObservers()) {
    nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
    if (os) {
      os->NotifyObservers(nullptr, "profiler-resumed", nullptr);
    }
  }
}

bool
profiler_feature_active(const char* aName)
{
  // This function runs both on and off the main thread.

  if (!gIsProfiling) {
    return false;
  }

  if (strcmp(aName, "gpu") == 0) {
    return gProfileGPU;
  }

  if (strcmp(aName, "layersdump") == 0) {
    return gLayersDump;
  }

  if (strcmp(aName, "displaylistdump") == 0) {
    return gDisplayListDump;
  }

  if (strcmp(aName, "restyle") == 0) {
    return gProfileRestyle;
  }

  return false;
}

bool
profiler_is_active()
{
  // This function runs both on and off the main thread.

  return gIsProfiling;
}

void
profiler_set_frame_number(int frameNumber)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  gFrameNumber = frameNumber;
}

void
profiler_lock()
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  profiler_stop();
  nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
  if (os)
    os->NotifyObservers(nullptr, "profiler-locked", nullptr);
}

void
profiler_unlock()
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
  if (os)
    os->NotifyObservers(nullptr, "profiler-unlocked", nullptr);
}

void
profiler_register_thread(const char* aName, void* aGuessStackTop)
{
  // This function runs both on and off the main thread.

  if (gInitCount == 0) {
    return;
  }

  MOZ_ASSERT(tlsPseudoStack.get() == nullptr);
  PseudoStack* stack = new PseudoStack();
  tlsPseudoStack.set(stack);
  bool isMainThread = is_main_thread_name(aName);
  void* stackTop = GetStackTop(aGuessStackTop);
  RegisterCurrentThread(aName, stack, isMainThread, stackTop);
}

void
profiler_unregister_thread()
{
  // This function runs both on and off the main thread.

  // Don't check gInitCount count here -- we may be unregistering the
  // thread after the sampler was shut down.
  if (!stack_key_initialized) {
    return;
  }

  {
    StaticMutexAutoLock lock(gRegisteredThreadsMutex);

    if (gRegisteredThreads) {
      Thread::tid_t id = Thread::GetCurrentId();

      for (uint32_t i = 0; i < gRegisteredThreads->size(); i++) {
        ThreadInfo* info = gRegisteredThreads->at(i);
        if (info->ThreadId() == id && !info->IsPendingDelete()) {
          if (profiler_is_active()) {
            // We still want to show the results of this thread if you
            // save the profile shortly after a thread is terminated.
            // For now we will defer the delete to profile stop.
            info->SetPendingDelete();
          } else {
            delete info;
            gRegisteredThreads->erase(gRegisteredThreads->begin() + i);
          }
          break;
        }
      }
    }
  }

  // We just cut the ThreadInfo's PseudoStack pointer (either nulling it via
  // SetPendingDelete() or by deleting the ThreadInfo altogether), so it is
  // safe to delete the PseudoStack.
  delete tlsPseudoStack.get();
  tlsPseudoStack.set(nullptr);
}

void
profiler_thread_sleep()
{
  // This function runs both on and off the main thread.

  if (gInitCount == 0) {
    return;
  }

  PseudoStack *stack = tlsPseudoStack.get();
  if (stack == nullptr) {
    return;
  }
  stack->setSleeping();
}

void
profiler_thread_wake()
{
  // This function runs both on and off the main thread.

  if (gInitCount == 0) {
    return;
  }

  PseudoStack *stack = tlsPseudoStack.get();
  if (stack == nullptr) {
    return;
  }
  stack->setAwake();
}

bool
profiler_thread_is_sleeping()
{
  // This function currently only used on the main thread, but that restriction
  // (and this assertion) could be removed without too much difficulty.
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  if (gInitCount == 0) {
    return false;
  }
  PseudoStack *stack = tlsPseudoStack.get();
  if (stack == nullptr) {
    return false;
  }
  return stack->isSleeping();
}

void
profiler_js_operation_callback()
{
  // This function runs both on and off the main thread.

  PseudoStack *stack = tlsPseudoStack.get();
  if (!stack) {
    return;
  }

  stack->jsOperationCallback();
}

double
profiler_time()
{
  // This function runs both on and off the main thread.

  mozilla::TimeDuration delta =
    mozilla::TimeStamp::Now() - gStartTime;
  return delta.ToMilliseconds();
}

bool
profiler_in_privacy_mode()
{
  // This function runs both on and off the main thread.

  PseudoStack *stack = tlsPseudoStack.get();
  if (!stack) {
    return false;
  }
  return stack->mPrivacyMode;
}

UniqueProfilerBacktrace
profiler_get_backtrace()
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  if (!stack_key_initialized)
    return nullptr;

  // Don't capture a stack if we're not profiling
  if (!profiler_is_active()) {
    return nullptr;
  }

  // Don't capture a stack if we don't want to include personal information
  if (profiler_in_privacy_mode()) {
    return nullptr;
  }

  if (!gSampler) {
    return nullptr;
  }

  PseudoStack* stack = tlsPseudoStack.get();
  if (!stack) {
    MOZ_ASSERT(stack);
    return nullptr;
  }
  Thread::tid_t tid = Thread::GetCurrentId();

  ProfileBuffer* buffer = new ProfileBuffer(GET_BACKTRACE_DEFAULT_ENTRIES);
  ThreadInfo* threadInfo =
    new ThreadInfo("SyncProfile", tid, NS_IsMainThread(), stack,
                   /* stackTop */ nullptr);
  threadInfo->SetHasProfile();

  TickSample sample;
  sample.threadInfo = threadInfo;

#if defined(HAVE_NATIVE_UNWIND)
#if defined(GP_OS_windows) || defined(GP_OS_linux) || defined(GP_OS_android)
  tickcontext_t context;
  sample.PopulateContext(&context);
#elif defined(GP_OS_darwin)
  sample.PopulateContext(nullptr);
#else
# error "unknown platform"
#endif
#endif

  sample.isSamplingCurrentThread = true;
  sample.timestamp = mozilla::TimeStamp::Now();

  Tick(buffer, &sample);

  return UniqueProfilerBacktrace(new ProfilerBacktrace(buffer, threadInfo));
}

void
ProfilerBacktraceDestructor::operator()(ProfilerBacktrace* aBacktrace)
{
  delete aBacktrace;
}

// Fill the output buffer with the following pattern:
// "Lable 1" "\0" "Label 2" "\0" ... "Label N" "\0" "\0"
// TODO: use the unwinder instead of pseudo stack.
void
profiler_get_backtrace_noalloc(char *output, size_t outputSize)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  MOZ_ASSERT(outputSize >= 2);
  char *bound = output + outputSize - 2;
  output[0] = output[1] = '\0';
  PseudoStack *pseudoStack = tlsPseudoStack.get();
  if (!pseudoStack) {
    return;
  }

  volatile js::ProfileEntry *pseudoFrames = pseudoStack->mStack;
  uint32_t pseudoCount = pseudoStack->stackSize();

  for (uint32_t i = 0; i < pseudoCount; i++) {
    size_t len = strlen(pseudoFrames[i].label());
    if (output + len >= bound)
      break;
    strcpy(output, pseudoFrames[i].label());
    output += len;
    *output++ = '\0';
    *output = '\0';
  }
}

void
profiler_tracing(const char* aCategory, const char* aInfo,
                 TracingMetadata aMetaData)
{
  // This function runs both on and off the main thread.

  // Don't insert a marker if we're not profiling, to avoid the heap copy
  // (malloc).
  if (!stack_key_initialized || !profiler_is_active()) {
    return;
  }

  profiler_add_marker(aInfo, new ProfilerMarkerTracing(aCategory, aMetaData));
}

void
profiler_tracing(const char* aCategory, const char* aInfo,
                 UniqueProfilerBacktrace aCause, TracingMetadata aMetaData)
{
  // This function runs both on and off the main thread.

  // Don't insert a marker if we're not profiling, to avoid the heap copy
  // (malloc).
  if (!stack_key_initialized || !profiler_is_active()) {
    return;
  }

  profiler_add_marker(aInfo, new ProfilerMarkerTracing(aCategory, aMetaData,
                                                       mozilla::Move(aCause)));
}

void
profiler_add_marker(const char *aMarker, ProfilerMarkerPayload *aPayload)
{
  // This function runs both on and off the main thread.

  // Note that aPayload may be allocated by the caller, so we need to make sure
  // that we free it at some point.
  mozilla::UniquePtr<ProfilerMarkerPayload> payload(aPayload);

  if (!stack_key_initialized)
    return;

  // Don't insert a marker if we're not profiling to avoid
  // the heap copy (malloc).
  if (!profiler_is_active()) {
    return;
  }

  // Don't add a marker if we don't want to include personal information
  if (profiler_in_privacy_mode()) {
    return;
  }

  PseudoStack *stack = tlsPseudoStack.get();
  if (!stack) {
    return;
  }

  mozilla::TimeStamp origin = (aPayload && !aPayload->GetStartTime().IsNull()) ?
                     aPayload->GetStartTime() : mozilla::TimeStamp::Now();
  mozilla::TimeDuration delta = origin - gStartTime;
  stack->addMarker(aMarker, payload.release(), delta.ToMilliseconds());
}

// END externally visible functions
////////////////////////////////////////////////////////////////////////

void PseudoStack::flushSamplerOnJSShutdown()
{
  MOZ_ASSERT(mContext);

  if (!gIsActive) {
    return;
  }

  gIsPaused = true;

  {
    StaticMutexAutoLock lock(gRegisteredThreadsMutex);

    for (size_t i = 0; i < gRegisteredThreads->size(); i++) {
      // Thread not being profiled, skip it.
      ThreadInfo* info = gRegisteredThreads->at(i);
      if (!info->HasProfile() || info->IsPendingDelete()) {
        continue;
      }

      // Thread not profiling the context that's going away, skip it.
      if (info->Stack()->mContext != mContext) {
        continue;
      }

      MutexAutoLock lock(info->GetMutex());
      info->FlushSamplesAndMarkers(gBuffer, gStartTime);
    }
  }

  gIsPaused = false;
}

// We #include these files directly because it means those files can use
// declarations from this file trivially.
#if defined(GP_OS_windows)
# include "platform-win32.cpp"
#elif defined(GP_OS_darwin)
# include "platform-macos.cpp"
#elif defined(GP_OS_linux) || defined(GP_OS_android)
# include "platform-linux-android.cpp"
#else
# error "bad platform"
#endif

