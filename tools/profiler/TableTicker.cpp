/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <string>
#include <stdio.h>
#include <fstream>
#include <sstream>
#include "GeckoProfilerImpl.h"
#include "SaveProfileTask.h"
#include "ProfileEntry.h"
#include "platform.h"
#include "nsThreadUtils.h"
#include "prenv.h"
#include "shared-libraries.h"
#include "mozilla/StackWalk.h"
#include "TableTicker.h"

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

// JS
#include "jsdbgapi.h"

// we eventually want to make this runtime switchable
#if defined(MOZ_PROFILING) && (defined(XP_UNIX) && !defined(XP_MACOSX))
 #ifndef ANDROID
  #define USE_BACKTRACE
 #endif
#endif
#ifdef USE_BACKTRACE
 #include <execinfo.h>
#endif

#if defined(MOZ_PROFILING) && (defined(XP_MACOSX) || defined(XP_WIN))
 #define USE_NS_STACKWALK
#endif
#ifdef USE_NS_STACKWALK
 #include "nsStackWalk.h"
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

JSCustomObject* TableTicker::GetMetaJSCustomObject(JSAObjectBuilder& b)
{
  JSCustomObject *meta = b.CreateObject();

  b.DefineProperty(meta, "version", 2);
  b.DefineProperty(meta, "interval", interval());
  b.DefineProperty(meta, "stackwalk", mUseStackWalk);
  b.DefineProperty(meta, "jank", mJankOnly);
  b.DefineProperty(meta, "processType", XRE_GetProcessType());

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
  JSCustomObject* profile = b.CreateObject();
  BuildJSObject(b, profile);
  JSObject* jsProfile = b.GetJSObject(profile);

  return jsProfile;
}

void TableTicker::BuildJSObject(JSAObjectBuilder& b, JSCustomObject* profile)
{
  // Put shared library info
  b.DefineProperty(profile, "libs", GetSharedLibraryInfoString().c_str());

  // Put meta data
  JSCustomObject *meta = GetMetaJSCustomObject(b);
  b.DefineProperty(profile, "meta", meta);

  // Lists the samples for each ThreadProfile
  JSCustomArray *threads = b.CreateArray();
  b.DefineProperty(profile, "threads", threads);

  SetPaused(true);

  {
    mozilla::MutexAutoLock lock(*sRegisteredThreadsMutex);

    for (size_t i = 0; i < sRegisteredThreads->size(); i++) {
      // Thread not being profiled, skip it
      if (!sRegisteredThreads->at(i)->Profile())
        continue;

      MutexAutoLock lock(*sRegisteredThreads->at(i)->Profile()->GetMutex());

      JSCustomObject* threadSamples = b.CreateObject();
      sRegisteredThreads->at(i)->Profile()->BuildJSObject(b, threadSamples);
      b.ArrayPush(threads, threadSamples);
    }
  }

  SetPaused(false);
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
        // The JIT only allows the top-most entry to have a NULL pc
        MOZ_ASSERT(&entry == &stack->mStack[stack->stackSize() - 1]);
        // If stack-walking was disabled, then that's just unfortunate
        if (lastpc) {
          jsbytecode *jspc = js::ProfilingGetPC(stack->mRuntime, entry.script(),
                                                lastpc);
          if (jspc) {
            lineno = JS_PCToLineNumber(NULL, entry.script(), jspc);
          }
        }
      } else {
        lineno = JS_PCToLineNumber(NULL, entry.script(), entry.pc());
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

#ifdef USE_BACKTRACE
void TableTicker::doNativeBacktrace(ThreadProfile &aProfile, TickSample* aSample)
{
  void *array[100];
  int count = backtrace (array, 100);

  aProfile.addTag(ProfileEntry('s', "(root)"));

  for (int i = 0; i < count; i++) {
    if( (intptr_t)array[i] == -1 ) break;
    aProfile.addTag(ProfileEntry('l', (void*)array[i]));
  }
}
#endif


#ifdef USE_NS_STACKWALK
typedef struct {
  void** array;
  void** sp_array;
  size_t size;
  size_t count;
} PCArray;

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
  platformData = aSample->context;
#endif // XP_WIN

  nsresult rv = NS_StackWalk(StackWalkCallback, /* skipFrames */ 0, maxFrames,
                             &array, thread, platformData);
#endif
  if (NS_SUCCEEDED(rv)) {
    aProfile.addTag(ProfileEntry('s', "(root)"));

    PseudoStack* stack = aProfile.GetPseudoStack();
    uint32_t pseudoStackPos = 0;

    /* We have two stacks, the native C stack we extracted from unwinding,
     * and the pseudostack we managed during execution. We want to consolidate
     * the two in order. We do so by merging using the approximate stack address
     * when each entry was push. When pushing JS entry we may not now the stack
     * address in which case we have a NULL stack address in which case we assume
     * that it follows immediatly the previous element.
     *
     *  C Stack | Address    --  Pseudo Stack | Address
     *  main()  | 0x100          run_js()     | 0x40
     *  start() | 0x80           jsCanvas()   | NULL
     *  timer() | 0x50           drawLine()   | NULL
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

  // Marker(s) come before the sample
  PseudoStack* stack = currThreadProfile.GetPseudoStack();
  for (int i = 0; stack->getMarker(i) != NULL; i++) {
    addDynamicTag(currThreadProfile, 'm', stack->getMarker(i));
  }
  stack->mQueueClearMarker = true;

  bool recordSample = true;
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

#if defined(USE_BACKTRACE) || defined(USE_NS_STACKWALK)
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
    TimeDuration delta = sample->timestamp - mStartTime;
    currThreadProfile.addTag(ProfileEntry('t', delta.ToMilliseconds()));
  }

  if (sLastFrameNumber != sFrameNumber) {
    currThreadProfile.addTag(ProfileEntry('f', sFrameNumber));
    sLastFrameNumber = sFrameNumber;
  }
}

static void print_callback(const ProfileEntry& entry, const char* tagStringData) {
  switch (entry.getTagName()) {
    case 's':
    case 'c':
      printf_stderr("  %s\n", tagStringData);
  }
}

void mozilla_sampler_print_location1()
{
  if (!stack_key_initialized)
    profiler_init();

  PseudoStack *stack = tlsPseudoStack.get();
  if (!stack) {
    MOZ_ASSERT(false);
    return;
  }

  ThreadProfile threadProfile("Temp", PROFILE_DEFAULT_ENTRY, stack,
                              0, Sampler::AllocPlatformData(0), false);
  doSampleStackTrace(stack, threadProfile, NULL);

  threadProfile.flush();

  printf_stderr("Backtrace:\n");
  threadProfile.IterateTags(print_callback);
}


