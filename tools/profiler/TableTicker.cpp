/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <string>
#include <stdio.h>
#include <fstream>
#include <sstream>
#include "GeckoProfiler.h"
#include "SaveProfileTask.h"
#include "ProfileEntry.h"
#include "SyncProfile.h"
#include "platform.h"
#include "nsThreadUtils.h"
#include "prenv.h"
#include "prtime.h"
#include "shared-libraries.h"
#include "mozilla/StackWalk.h"
#include "TableTicker.h"
#include "nsXULAppAPI.h"

// JSON
#include "JSObjectBuilder.h"
#include "JSCustomObjectBuilder.h"

// Meta
#include "nsXPCOM.h"
#include "nsXPCOMCID.h"
#include "nsIHttpProtocolHandler.h"
#include "nsServiceManagerUtils.h"
#include "nsIXULRuntime.h"
#include "nsIXULAppInfo.h"
#include "nsDirectoryServiceUtils.h"
#include "nsDirectoryServiceDefs.h"
#include "nsIObserverService.h"
#include "mozilla/Services.h"
#include "PlatformMacros.h"

#if defined(SPS_OS_android) && !defined(MOZ_WIDGET_GONK)
  #include "AndroidBridge.h"
#endif

// JS
#include "js/OldDebugAPI.h"

#if defined(MOZ_PROFILING) && (defined(XP_MACOSX) || defined(XP_WIN))
 #define USE_NS_STACKWALK
#endif
#ifdef USE_NS_STACKWALK
 #include "nsStackWalk.h"
#endif

#if defined(XP_WIN)
typedef CONTEXT tickcontext_t;
#elif defined(LINUX)
#include <ucontext.h>
typedef ucontext_t tickcontext_t;
#endif

#if defined(LINUX) || defined(XP_MACOSX)
#include <sys/types.h>
pid_t gettid();
#endif

#if defined(SPS_ARCH_arm) && defined(MOZ_WIDGET_GONK)
 // Should also work on other Android and ARM Linux, but not tested there yet.
 #define USE_EHABI_STACKWALK
#endif
#ifdef USE_EHABI_STACKWALK
 #include "EHABIStackWalk.h"
#endif

using std::string;
using namespace mozilla;

#ifndef MAXPATHLEN
 #ifdef PATH_MAX
  #define MAXPATHLEN PATH_MAX
 #elif defined(MAX_PATH)
  #define MAXPATHLEN MAX_PATH
 #elif defined(_MAX_PATH)
  #define MAXPATHLEN _MAX_PATH
 #elif defined(CCHMAXPATH)
  #define MAXPATHLEN CCHMAXPATH
 #else
  #define MAXPATHLEN 1024
 #endif
#endif

///////////////////////////////////////////////////////////////////////
// BEGIN SaveProfileTask et al

std::string GetSharedLibraryInfoString();

void TableTicker::HandleSaveRequest()
{
  if (!mSaveRequested)
    return;
  mSaveRequested = false;

  // TODO: Use use the ipc/chromium Tasks here to support processes
  // without XPCOM.
  nsCOMPtr<nsIRunnable> runnable = new SaveProfileTask();
  NS_DispatchToMainThread(runnable);
}

template <typename Builder>
typename Builder::Object TableTicker::GetMetaJSCustomObject(Builder& b)
{
  typename Builder::RootedObject meta(b.context(), b.CreateObject());

  b.DefineProperty(meta, "version", 2);
  b.DefineProperty(meta, "interval", interval());
  b.DefineProperty(meta, "stackwalk", mUseStackWalk);
  b.DefineProperty(meta, "jank", mJankOnly);
  b.DefineProperty(meta, "processType", XRE_GetProcessType());

  TimeDuration delta = TimeStamp::Now() - sStartTime;
  b.DefineProperty(meta, "startTime", PR_Now()/1000.0 - delta.ToMilliseconds());

  nsresult res;
  nsCOMPtr<nsIHttpProtocolHandler> http = do_GetService(NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "http", &res);
  if (!NS_FAILED(res)) {
    nsAutoCString string;

    res = http->GetPlatform(string);
    if (!NS_FAILED(res))
      b.DefineProperty(meta, "platform", string.Data());

    res = http->GetOscpu(string);
    if (!NS_FAILED(res))
      b.DefineProperty(meta, "oscpu", string.Data());

    res = http->GetMisc(string);
    if (!NS_FAILED(res))
      b.DefineProperty(meta, "misc", string.Data());
  }

  nsCOMPtr<nsIXULRuntime> runtime = do_GetService("@mozilla.org/xre/runtime;1");
  if (runtime) {
    nsAutoCString string;

    res = runtime->GetXPCOMABI(string);
    if (!NS_FAILED(res))
      b.DefineProperty(meta, "abi", string.Data());

    res = runtime->GetWidgetToolkit(string);
    if (!NS_FAILED(res))
      b.DefineProperty(meta, "toolkit", string.Data());
  }

  nsCOMPtr<nsIXULAppInfo> appInfo = do_GetService("@mozilla.org/xre/app-info;1");
  if (appInfo) {
    nsAutoCString string;

    res = appInfo->GetName(string);
    if (!NS_FAILED(res))
      b.DefineProperty(meta, "product", string.Data());
  }

  return meta;
}

void TableTicker::ToStreamAsJSON(std::ostream& stream)
{
  JSCustomObjectBuilder b;
  JSCustomObject* profile = b.CreateObject();
  BuildJSObject(b, profile);
  b.Serialize(profile, stream);
  b.DeleteObject(profile);
}

JSObject* TableTicker::ToJSObject(JSContext *aCx)
{
  JSObjectBuilder b(aCx);
  JS::RootedObject profile(aCx, b.CreateObject());
  BuildJSObject(b, profile);
  return profile;
}

template <typename Builder>
struct SubprocessClosure {
  SubprocessClosure(Builder *aBuilder, typename Builder::ArrayHandle aThreads)
    : mBuilder(aBuilder), mThreads(aThreads)
  {}

  Builder* mBuilder;
  typename Builder::ArrayHandle mThreads;
};

template <typename Builder>
void SubProcessCallback(const char* aProfile, void* aClosure)
{
  // Called by the observer to get their profile data included
  // as a sub profile
  SubprocessClosure<Builder>* closure = (SubprocessClosure<Builder>*)aClosure;

  closure->mBuilder->ArrayPush(closure->mThreads, aProfile);
}

#if defined(SPS_OS_android) && !defined(MOZ_WIDGET_GONK)
template <typename Builder>
static
typename Builder::Object BuildJavaThreadJSObject(Builder& b)
{
  typename Builder::RootedObject javaThread(b.context(), b.CreateObject());
  b.DefineProperty(javaThread, "name", "Java Main Thread");

  typename Builder::RootedArray samples(b.context(), b.CreateArray());
  b.DefineProperty(javaThread, "samples", samples);

  int sampleId = 0;
  while (true) {
    int frameId = 0;
    typename Builder::RootedObject sample(b.context());
    typename Builder::RootedArray frames(b.context());
    while (true) {
      nsCString result;
      bool hasFrame = AndroidBridge::Bridge()->GetFrameNameJavaProfiling(0, sampleId, frameId, result);
      if (!hasFrame) {
        if (frames) {
          b.DefineProperty(sample, "frames", frames);
        }
        break;
      }
      if (!sample) {
        sample = b.CreateObject();
        frames = b.CreateArray();
        b.DefineProperty(sample, "frames", frames);
        b.ArrayPush(samples, sample);

        double sampleTime =
          mozilla::widget::android::GeckoJavaSampler::GetSampleTimeJavaProfiling(0, sampleId);
        b.DefineProperty(sample, "time", sampleTime);
      }
      typename Builder::RootedObject frame(b.context(), b.CreateObject());
      b.DefineProperty(frame, "location", result.BeginReading());
      b.ArrayPush(frames, frame);
      frameId++;
    }
    if (frameId == 0) {
      break;
    }
    sampleId++;
  }

  return javaThread;
}
#endif

template <typename Builder>
void TableTicker::BuildJSObject(Builder& b, typename Builder::ObjectHandle profile)
{
  // Put shared library info
  b.DefineProperty(profile, "libs", GetSharedLibraryInfoString().c_str());

  // Put meta data
  typename Builder::RootedObject meta(b.context(), GetMetaJSCustomObject(b));
  b.DefineProperty(profile, "meta", meta);

  // Lists the samples for each ThreadProfile
  typename Builder::RootedArray threads(b.context(), b.CreateArray());
  b.DefineProperty(profile, "threads", threads);

  SetPaused(true);

  {
    mozilla::MutexAutoLock lock(*sRegisteredThreadsMutex);

    for (size_t i = 0; i < sRegisteredThreads->size(); i++) {
      // Thread not being profiled, skip it
      if (!sRegisteredThreads->at(i)->Profile())
        continue;

      MutexAutoLock lock(*sRegisteredThreads->at(i)->Profile()->GetMutex());

      typename Builder::RootedObject threadSamples(b.context(), b.CreateObject());
      sRegisteredThreads->at(i)->Profile()->BuildJSObject(b, threadSamples);
      b.ArrayPush(threads, threadSamples);
    }
  }

#if defined(SPS_OS_android) && !defined(MOZ_WIDGET_GONK)
  if (ProfileJava()) {
    mozilla::widget::android::GeckoJavaSampler::PauseJavaProfiling();

    typename Builder::RootedObject javaThread(b.context(), BuildJavaThreadJSObject(b));
    b.ArrayPush(threads, javaThread);

    mozilla::widget::android::GeckoJavaSampler::UnpauseJavaProfiling();
  }
#endif

  SetPaused(false);

  // Send a event asking any subprocesses (plugins) to
  // give us their information
  SubprocessClosure<Builder> closure(&b, threads);
  nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
  if (os) {
    nsRefPtr<ProfileSaveEvent> pse = new ProfileSaveEvent(SubProcessCallback<Builder>, &closure);
    os->NotifyObservers(pse, "profiler-subprocess", nullptr);
  }
}

// END SaveProfileTask et al
////////////////////////////////////////////////////////////////////////

static
void addDynamicTag(ThreadProfile &aProfile, char aTagName, const char *aStr)
{
  aProfile.addTag(ProfileEntry(aTagName, ""));
  // Add one to store the null termination
  size_t strLen = strlen(aStr) + 1;
  for (size_t j = 0; j < strLen;) {
    // Store as many characters in the void* as the platform allows
    char text[sizeof(void*)];
    size_t len = sizeof(void*)/sizeof(char);
    if (j+len >= strLen) {
      len = strLen - j;
    }
    memcpy(text, &aStr[j], len);
    j += sizeof(void*)/sizeof(char);
    // Cast to *((void**) to pass the text data to a void*
    aProfile.addTag(ProfileEntry('d', *((void**)(&text[0]))));
  }
}

static
void addProfileEntry(volatile StackEntry &entry, ThreadProfile &aProfile,
                     PseudoStack *stack, void *lastpc)
{
  int lineno = -1;

  // First entry has tagName 's' (start)
  // Check for magic pointer bit 1 to indicate copy
  const char* sampleLabel = entry.label();
  if (entry.isCopyLabel()) {
    // Store the string using 1 or more 'd' (dynamic) tags
    // that will happen to the preceding tag

    addDynamicTag(aProfile, 'c', sampleLabel);
    if (entry.js()) {
      if (!entry.pc()) {
        // The JIT only allows the top-most entry to have a nullptr pc
        MOZ_ASSERT(&entry == &stack->mStack[stack->stackSize() - 1]);
        // If stack-walking was disabled, then that's just unfortunate
        if (lastpc) {
          jsbytecode *jspc = js::ProfilingGetPC(stack->mRuntime, entry.script(),
                                                lastpc);
          if (jspc) {
            lineno = JS_PCToLineNumber(nullptr, entry.script(), jspc);
          }
        }
      } else {
        lineno = JS_PCToLineNumber(nullptr, entry.script(), entry.pc());
      }
    } else {
      lineno = entry.line();
    }
  } else {
    aProfile.addTag(ProfileEntry('c', sampleLabel));
    lineno = entry.line();
  }
  if (lineno != -1) {
    aProfile.addTag(ProfileEntry('n', lineno));
  }
}

#if defined(USE_NS_STACKWALK) || defined(USE_EHABI_STACKWALK)
typedef struct {
  void** array;
  void** sp_array;
  size_t size;
  size_t count;
} PCArray;

static void mergeNativeBacktrace(ThreadProfile &aProfile, const PCArray &array) {
  aProfile.addTag(ProfileEntry('s', "(root)"));

  PseudoStack* stack = aProfile.GetPseudoStack();
  uint32_t pseudoStackPos = 0;

  /* We have two stacks, the native C stack we extracted from unwinding,
   * and the pseudostack we managed during execution. We want to consolidate
   * the two in order. We do so by merging using the approximate stack address
   * when each entry was push. When pushing JS entry we may not now the stack
   * address in which case we have a nullptr stack address in which case we assume
   * that it follows immediatly the previous element.
   *
   *  C Stack | Address    --  Pseudo Stack | Address
   *  main()  | 0x100          run_js()     | 0x40
   *  start() | 0x80           jsCanvas()   | nullptr
   *  timer() | 0x50           drawLine()   | nullptr
   *  azure() | 0x10
   *
   * Merged: main(), start(), timer(), run_js(), jsCanvas(), drawLine(), azure()
   */
  // i is the index in C stack starting at main and decreasing
  // pseudoStackPos is the position in the Pseudo stack starting
  // at the first frame (run_js in the example) and increasing.
  for (size_t i = array.count; i > 0; --i) {
    while (pseudoStackPos < stack->stackSize()) {
      volatile StackEntry& entry = stack->mStack[pseudoStackPos];

      if (entry.stackAddress() < array.sp_array[i-1] && entry.stackAddress())
        break;

      addProfileEntry(entry, aProfile, stack, array.array[0]);
      pseudoStackPos++;
    }

    aProfile.addTag(ProfileEntry('l', (void*)array.array[i-1]));
  }
}

#endif

#ifdef USE_NS_STACKWALK
static
void StackWalkCallback(void* aPC, void* aSP, void* aClosure)
{
  PCArray* array = static_cast<PCArray*>(aClosure);
  MOZ_ASSERT(array->count < array->size);
  array->sp_array[array->count] = aSP;
  array->array[array->count] = aPC;
  array->count++;
}

void TableTicker::doNativeBacktrace(ThreadProfile &aProfile, TickSample* aSample)
{
#ifndef XP_MACOSX
  uintptr_t thread = GetThreadHandle(aSample->threadProfile->GetPlatformData());
  MOZ_ASSERT(thread);
#endif
  void* pc_array[1000];
  void* sp_array[1000];
  PCArray array = {
    pc_array,
    sp_array,
    mozilla::ArrayLength(pc_array),
    0
  };

  // Start with the current function.
  StackWalkCallback(aSample->pc, aSample->sp, &array);

  uint32_t maxFrames = uint32_t(array.size - array.count);
#ifdef XP_MACOSX
  pthread_t pt = GetProfiledThread(aSample->threadProfile->GetPlatformData());
  void *stackEnd = reinterpret_cast<void*>(-1);
  if (pt)
    stackEnd = static_cast<char*>(pthread_get_stackaddr_np(pt));
  nsresult rv = NS_OK;
  if (aSample->fp >= aSample->sp && aSample->fp <= stackEnd)
    rv = FramePointerStackWalk(StackWalkCallback, /* skipFrames */ 0,
                               maxFrames, &array,
                               reinterpret_cast<void**>(aSample->fp), stackEnd);
#else
  void *platformData = nullptr;
#ifdef XP_WIN
  if (aSample->isSamplingCurrentThread) {
    // In this case we want NS_StackWalk to know that it's walking the
    // current thread's stack, so we pass 0 as the thread handle.
    thread = 0;
  }
  platformData = aSample->context;
#endif // XP_WIN

  nsresult rv = NS_StackWalk(StackWalkCallback, /* skipFrames */ 0, maxFrames,
                             &array, thread, platformData);
#endif
  if (NS_SUCCEEDED(rv))
    mergeNativeBacktrace(aProfile, array);
}
#endif

#ifdef USE_EHABI_STACKWALK
void TableTicker::doNativeBacktrace(ThreadProfile &aProfile, TickSample* aSample)
{
  void *pc_array[1000];
  void *sp_array[1000];
  PCArray array = {
    pc_array,
    sp_array,
    mozilla::ArrayLength(pc_array),
    0
  };

  const mcontext_t *mcontext = &reinterpret_cast<ucontext_t *>(aSample->context)->uc_mcontext;
  mcontext_t savedContext;
  PseudoStack *pseudoStack = aProfile.GetPseudoStack();

  array.count = 0;
  // The pseudostack contains an "EnterJIT" frame whenever we enter
  // JIT code with profiling enabled; the stack pointer value points
  // the saved registers.  We use this to unwind resume unwinding
  // after encounting JIT code.
  for (uint32_t i = pseudoStack->stackSize(); i > 0; --i) {
    // The pseudostack grows towards higher indices, so we iterate
    // backwards (from callee to caller).
    volatile StackEntry &entry = pseudoStack->mStack[i - 1];
    if (!entry.js() && strcmp(entry.label(), "EnterJIT") == 0) {
      // Found JIT entry frame.  Unwind up to that point (i.e., force
      // the stack walk to stop before the block of saved registers;
      // note that it yields nondecreasing stack pointers), then restore
      // the saved state.
      uint32_t *vSP = reinterpret_cast<uint32_t*>(entry.stackAddress());

      array.count += EHABIStackWalk(*mcontext,
                                    /* stackBase = */ vSP,
                                    sp_array + array.count,
                                    pc_array + array.count,
                                    array.size - array.count);

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

  // Now unwind whatever's left (starting from either the last EnterJIT
  // frame or, if no EnterJIT was found, the original registers).
  array.count += EHABIStackWalk(*mcontext,
                                aProfile.GetStackTop(),
                                sp_array + array.count,
                                pc_array + array.count,
                                array.size - array.count);

  mergeNativeBacktrace(aProfile, array);
}

#endif

static
void doSampleStackTrace(PseudoStack *aStack, ThreadProfile &aProfile, TickSample *sample)
{
  // Sample
  // 's' tag denotes the start of a sample block
  // followed by 0 or more 'c' tags.
  aProfile.addTag(ProfileEntry('s', "(root)"));
  for (uint32_t i = 0; i < aStack->stackSize(); i++) {
    addProfileEntry(aStack->mStack[i], aProfile, aStack, nullptr);
  }
#ifdef ENABLE_SPS_LEAF_DATA
  if (sample) {
    aProfile.addTag(ProfileEntry('l', (void*)sample->pc));
#ifdef ENABLE_ARM_LR_SAVING
    aProfile.addTag(ProfileEntry('L', (void*)sample->lr));
#endif
  }
#endif
}

void TableTicker::Tick(TickSample* sample)
{
  if (HasUnwinderThread()) {
    UnwinderTick(sample);
  } else {
    InplaceTick(sample);
  }
}

void TableTicker::InplaceTick(TickSample* sample)
{
  ThreadProfile& currThreadProfile = *sample->threadProfile;

  PseudoStack* stack = currThreadProfile.GetPseudoStack();
  bool recordSample = true;
#if defined(XP_WIN)
  bool powerSample = false;
#endif

  /* Don't process the PeudoStack's markers or honour jankOnly if we're
     immediately sampling the current thread. */
  if (!sample->isSamplingCurrentThread) {
    // Marker(s) come before the sample
    ProfilerMarkerLinkedList* pendingMarkersList = stack->getPendingMarkers();
    while (pendingMarkersList && pendingMarkersList->peek()) {
      ProfilerMarker* marker = pendingMarkersList->popHead();
      stack->addStoredMarker(marker);
      currThreadProfile.addTag(ProfileEntry('m', marker));
    }
    stack->updateGeneration(currThreadProfile.GetGenerationID());

#if defined(XP_WIN)
    if (mProfilePower) {
      mIntelPowerGadget->TakeSample();
      powerSample = true;
    }
#endif

    if (mJankOnly) {
      // if we are on a different event we can discard any temporary samples
      // we've kept around
      if (sLastSampledEventGeneration != sCurrentEventGeneration) {
        // XXX: we also probably want to add an entry to the profile to help
        // distinguish which samples are part of the same event. That, or record
        // the event generation in each sample
        currThreadProfile.erase();
      }
      sLastSampledEventGeneration = sCurrentEventGeneration;

      recordSample = false;
      // only record the events when we have a we haven't seen a tracer event for 100ms
      if (!sLastTracerEvent.IsNull()) {
        TimeDuration delta = sample->timestamp - sLastTracerEvent;
        if (delta.ToMilliseconds() > 100.0) {
            recordSample = true;
        }
      }
    }
  }

#if defined(USE_NS_STACKWALK) || defined(USE_EHABI_STACKWALK)
  if (mUseStackWalk) {
    doNativeBacktrace(currThreadProfile, sample);
  } else {
    doSampleStackTrace(stack, currThreadProfile, mAddLeafAddresses ? sample : nullptr);
  }
#else
  doSampleStackTrace(stack, currThreadProfile, mAddLeafAddresses ? sample : nullptr);
#endif

  if (recordSample)
    currThreadProfile.flush();

  if (!sLastTracerEvent.IsNull() && sample && currThreadProfile.IsMainThread()) {
    TimeDuration delta = sample->timestamp - sLastTracerEvent;
    currThreadProfile.addTag(ProfileEntry('r', delta.ToMilliseconds()));
  }

  if (sample) {
    TimeDuration delta = sample->timestamp - sStartTime;
    currThreadProfile.addTag(ProfileEntry('t', delta.ToMilliseconds()));
  }

#if defined(XP_WIN)
  if (powerSample) {
    currThreadProfile.addTag(ProfileEntry('p', mIntelPowerGadget->GetTotalPackagePowerInWatts()));
  }
#endif

  if (sLastFrameNumber != sFrameNumber) {
    currThreadProfile.addTag(ProfileEntry('f', sFrameNumber));
    sLastFrameNumber = sFrameNumber;
  }
}

namespace {

SyncProfile* NewSyncProfile()
{
  PseudoStack* stack = tlsPseudoStack.get();
  if (!stack) {
    MOZ_ASSERT(stack);
    return nullptr;
  }
  Thread::tid_t tid = Thread::GetCurrentId();

  SyncProfile* profile = new SyncProfile("SyncProfile",
                                         GET_BACKTRACE_DEFAULT_ENTRY,
                                         stack, tid, NS_IsMainThread());
  return profile;
}

} // anonymous namespace

SyncProfile* TableTicker::GetBacktrace()
{
  SyncProfile* profile = NewSyncProfile();

  TickSample sample;
  sample.threadProfile = profile;

#if defined(HAVE_NATIVE_UNWIND)
#if defined(XP_WIN) || defined(LINUX)
  tickcontext_t context;
  sample.PopulateContext(&context);
#elif defined(XP_MACOSX)
  sample.PopulateContext(nullptr);
#endif
#endif

  sample.isSamplingCurrentThread = true;
  sample.timestamp = mozilla::TimeStamp::Now();

  if (!HasUnwinderThread()) {
    profile->BeginUnwind();
  }

  Tick(&sample);

  if (!HasUnwinderThread()) {
    profile->EndUnwind();
  }

  return profile;
}

static void print_callback(const ProfileEntry& entry, const char* tagStringData)
{
  switch (entry.getTagName()) {
    case 's':
    case 'c':
      printf_stderr("  %s\n", tagStringData);
  }
}

void mozilla_sampler_print_location1()
{
  if (!stack_key_initialized)
    profiler_init(nullptr);

  SyncProfile* syncProfile = NewSyncProfile();
  if (!syncProfile) {
    return;
  }

  syncProfile->BeginUnwind();
  doSampleStackTrace(syncProfile->GetPseudoStack(), *syncProfile, nullptr);
  syncProfile->EndUnwind();

  printf_stderr("Backtrace:\n");
  syncProfile->IterateTags(print_callback);
  delete syncProfile;
}


