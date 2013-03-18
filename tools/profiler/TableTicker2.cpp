/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// System
#include <string>
#include <stdio.h>
#include <errno.h>
#include <iostream>
#include <fstream>
#include <sstream>
#if defined(ANDROID)
# include "android-signal-defs.h"
#endif

// Profiler
#include "PlatformMacros.h"
#include "sps_sampler.h"
#include "platform.h"
#include "nsXULAppAPI.h"
#include "nsThreadUtils.h"
#include "prenv.h"
#include "shared-libraries.h"
#include "mozilla/StackWalk.h"
#include "ProfileEntry2.h"
#include "UnwinderThread2.h"

// JSON
#include "JSObjectBuilder.h"
#include "nsIJSRuntimeService.h"

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

// JS
#include "jsdbgapi.h"

// This file's exports are listed in sps_sampler.h.

// Pseudo backtraces are available on all platforms.  Native
// backtraces are available only on selected platforms.  Breakpad is
// the only supported native unwinder.  HAVE_NATIVE_UNWIND is set at
// build time to indicate whether native unwinding is possible on this
// platform.  The actual unwind mode currently in use is stored in
// sUnwindMode.

#undef HAVE_NATIVE_UNWIND
#if defined(MOZ_PROFILING) \
    && (defined(SPS_PLAT_amd64_linux) || defined(SPS_PLAT_arm_android) \
        || defined(SPS_PLAT_x86_linux))
# define HAVE_NATIVE_UNWIND
#endif

#if 0 && defined(MOZ_PROFILING) && !defined(SPS_PLAT_x86_windows)
# warning MOZ_PROFILING
#endif

#if 0 && defined(HAVE_NATIVE_UNWIND)
# warning HAVE_NATIVE_UNWIND
#endif

typedef  enum { UnwINVALID, UnwNATIVE, UnwPSEUDO, UnwCOMBINED }  UnwMode;

/* These will be set to something sensible before we take the first
   sample. */
static UnwMode sUnwindMode     = UnwINVALID;
static int     sUnwindInterval = 0;


using std::string;
using namespace mozilla;

#ifdef XP_WIN
 #include <windows.h>
 #define getpid GetCurrentProcessId
#else
 #include <unistd.h>
#endif

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

#if _MSC_VER
 #define snprintf _snprintf
#endif

class TableTicker2;

mozilla::ThreadLocal<PseudoStack *> tlsPseudoStack;
static mozilla::ThreadLocal<TableTicker2 *> tlsTicker;
// We need to track whether we've been initialized otherwise
// we end up using tlsStack without initializing it.
// Because tlsStack is totally opaque to us we can't reuse
// it as the flag itself.
bool stack_key_initialized;

static TimeStamp sLastTracerEvent; // is raced on
static int       sFrameNumber = 0;
static int       sLastFrameNumber = 0;


static bool
hasFeature(const char** aFeatures, uint32_t aFeatureCount, const char* aFeature)
{
  for (size_t i = 0; i < aFeatureCount; i++) {
    if (0) LOGF("hasFeature %s: %lu %s", aFeature, (unsigned long)i, aFeatures[i]);
    if (strcmp(aFeatures[i], aFeature) == 0)
      return true;
  }
  return false;
}

class TableTicker2: public Sampler {
 public:
  TableTicker2(int aInterval, int aEntrySize, PseudoStack *aStack,
              const char** aFeatures, uint32_t aFeatureCount)
    : Sampler(aInterval, true)
    , mPrimaryThreadProfile(aEntrySize, aStack)
    , mStartTime(TimeStamp::Now())
    , mSaveRequested(false)
  {
    mUseStackWalk = hasFeature(aFeatures, aFeatureCount, "stackwalk");

    //XXX: It's probably worth splitting the jank profiler out from the regular profiler at some point
    mJankOnly = hasFeature(aFeatures, aFeatureCount, "jank");
    mProfileJS = hasFeature(aFeatures, aFeatureCount, "js");
    mPrimaryThreadProfile.addTag(ProfileEntry2('m', "Start"));
  }

  ~TableTicker2() { if (IsActive()) Stop(); }

  virtual void SampleStack(TickSample* sample) {}

  // Called within a signal. This function must be reentrant
  virtual void Tick(TickSample* sample);

  // Called within a signal. This function must be reentrant
  virtual void RequestSave()
  {
    mSaveRequested = true;
  }

  virtual void HandleSaveRequest();

  ThreadProfile2* GetPrimaryThreadProfile()
  {
    return &mPrimaryThreadProfile;
  }

  JSObject *ToJSObject(JSContext *aCx);
  JSCustomObject *GetMetaJSCustomObject(JSAObjectBuilder& b);

  const bool ProfileJS() { return mProfileJS; }

private:
  // Unwind, using a "native" backtrace scheme (non-PseudoStack).
  // Not implemented on platforms which do not support native backtraces
  void doNativeBacktrace(ThreadProfile2 &aProfile, TickSample* aSample);

private:
  // This represent the application's main thread (SAMPLER_INIT)
  ThreadProfile2 mPrimaryThreadProfile;
  TimeStamp mStartTime;
  bool mSaveRequested;
  bool mUseStackWalk;
  bool mJankOnly;
  bool mProfileJS;
};


////////////////////////////////////////////////////////////////////////
// BEGIN SaveProfileTask et al

std::string GetSharedLibraryInfoString();

static JSBool
WriteCallback(const jschar *buf, uint32_t len, void *data)
{
  std::ofstream& stream = *static_cast<std::ofstream*>(data);
  nsAutoCString profile = NS_ConvertUTF16toUTF8(buf, len);
  stream << profile.Data();
  return JS_TRUE;
}

/**
 * This is an event used to save the profile on the main thread
 * to be sure that it is not being modified while saving.
 */
class SaveProfileTask : public nsRunnable {
public:
  SaveProfileTask() {}

  NS_IMETHOD Run() {
    TableTicker2 *t = tlsTicker.get();

    // Pause the profiler during saving.
    // This will prevent us from recording sampling
    // regarding profile saving. This will also
    // prevent bugs caused by the circular buffer not
    // being thread safe. Bug 750989.
    t->SetPaused(true);

    // Get file path
#   if defined(SPS_PLAT_arm_android)
    nsCString tmpPath;
    tmpPath.AppendPrintf("/sdcard/profile_%i_%i.txt", XRE_GetProcessType(), getpid());
#   else
    nsCOMPtr<nsIFile> tmpFile;
    nsAutoCString tmpPath;
    if (NS_FAILED(NS_GetSpecialDirectory(NS_OS_TEMP_DIR, getter_AddRefs(tmpFile)))) {
      LOG("Failed to find temporary directory.");
      return NS_ERROR_FAILURE;
    }
    tmpPath.AppendPrintf("profile_%i_%i.txt", XRE_GetProcessType(), getpid());

    nsresult rv = tmpFile->AppendNative(tmpPath);
    if (NS_FAILED(rv))
      return rv;

    rv = tmpFile->GetNativePath(tmpPath);
    if (NS_FAILED(rv))
      return rv;
#   endif

    // Create a JSContext to run a JSObjectBuilder :(
    // Based on XPCShellEnvironment
    JSRuntime *rt;
    JSContext *cx;
    nsCOMPtr<nsIJSRuntimeService> rtsvc 
      = do_GetService("@mozilla.org/js/xpc/RuntimeService;1");
    if (!rtsvc || NS_FAILED(rtsvc->GetRuntime(&rt)) || !rt) {
      LOG("failed to get RuntimeService");
      return NS_ERROR_FAILURE;;
    }

    cx = JS_NewContext(rt, 8192);
    if (!cx) {
      LOG("Failed to get context");
      return NS_ERROR_FAILURE;
    }

    {
      JSAutoRequest ar(cx);
      static JSClass c = {
          "global", JSCLASS_GLOBAL_FLAGS,
          JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
          JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub
      };
      JSObject *obj = JS_NewGlobalObject(cx, &c, NULL);

      std::ofstream stream;
      stream.open(tmpPath.get());
      // Pause the profiler during saving.
      // This will prevent us from recording sampling
      // regarding profile saving. This will also
      // prevent bugs caused by the circular buffer not
      // being thread safe. Bug 750989.
      t->SetPaused(true);
      if (stream.is_open()) {
        JSAutoCompartment autoComp(cx, obj);
        JSObject* profileObj = mozilla_sampler_get_profile_data2(cx);
        jsval val = OBJECT_TO_JSVAL(profileObj);
        JS_Stringify(cx, &val, nullptr, JSVAL_NULL, WriteCallback, &stream);
        stream.close();
        LOGF("Saved to %s", tmpPath.get());
      } else {
        LOG("Fail to open profile log file.");
      }
    }
    JS_EndRequest(cx);
    JS_DestroyContext(cx);

    t->SetPaused(false);

    return NS_OK;
  }
};

void TableTicker2::HandleSaveRequest()
{
  if (!mSaveRequested)
    return;
  mSaveRequested = false;

  // TODO: Use use the ipc/chromium Tasks here to support processes
  // without XPCOM.
  nsCOMPtr<nsIRunnable> runnable = new SaveProfileTask();
  NS_DispatchToMainThread(runnable);
}

JSCustomObject* TableTicker2::GetMetaJSCustomObject(JSAObjectBuilder& b)
{
  JSCustomObject *meta = b.CreateObject();

  b.DefineProperty(meta, "version", 2);
  b.DefineProperty(meta, "interval", interval());
  b.DefineProperty(meta, "stackwalk", mUseStackWalk);
  b.DefineProperty(meta, "jank", mJankOnly);
  b.DefineProperty(meta, "processType", XRE_GetProcessType());

  nsresult res;
  nsCOMPtr<nsIHttpProtocolHandler> http
    = do_GetService(NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "http", &res);
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

JSObject* TableTicker2::ToJSObject(JSContext *aCx)
{
  JSObjectBuilder b(aCx);

  JSCustomObject *profile = b.CreateObject();

  // Put shared library info
  b.DefineProperty(profile, "libs", GetSharedLibraryInfoString().c_str());

  // Put meta data
  JSCustomObject *meta = GetMetaJSCustomObject(b);
  b.DefineProperty(profile, "meta", meta);

  // Lists the samples for each ThreadProfile2
  JSCustomArray *threads = b.CreateArray();
  b.DefineProperty(profile, "threads", threads);

  // For now we only have one thread
  SetPaused(true);
  ThreadProfile2* prof = GetPrimaryThreadProfile();
  prof->GetMutex()->Lock();
  JSCustomObject* threadSamples = prof->ToJSObject(aCx);
  b.ArrayPush(threads, threadSamples);
  prof->GetMutex()->Unlock();
  SetPaused(false);

  return b.GetJSObject(profile);
}

// END SaveProfileTask et al
////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////
// BEGIN take samples.
// Everything in this section RUNS IN SIGHANDLER CONTEXT

// RUNS IN SIGHANDLER CONTEXT
static
void genProfileEntry2(/*MODIFIED*/UnwinderThreadBuffer* utb,
                     volatile StackEntry &entry,
                     PseudoStack *stack, void *lastpc)
{
  int lineno = -1;

  // Add a pseudostack-entry start label
  utb__addEntry( utb, ProfileEntry2('h', 'P') );
  // And the SP value, if it is non-zero
  if (entry.stackAddress() != 0) {
    utb__addEntry( utb, ProfileEntry2('S', entry.stackAddress()) );
  }

  // First entry has tagName 's' (start)
  // Check for magic pointer bit 1 to indicate copy
  const char* sampleLabel = entry.label();
  if (entry.isCopyLabel()) {
    // Store the string using 1 or more 'd' (dynamic) tags
    // that will happen to the preceding tag

    utb__addEntry( utb, ProfileEntry2('c', "") );
    // Add one to store the null termination
    size_t strLen = strlen(sampleLabel) + 1;
    for (size_t j = 0; j < strLen;) {
      // Store as many characters in the void* as the platform allows
      char text[sizeof(void*)];
      for (size_t pos = 0; pos < sizeof(void*) && j+pos < strLen; pos++) {
        text[pos] = sampleLabel[j+pos];
      }
      j += sizeof(void*)/sizeof(char);
      // Cast to *((void**) to pass the text data to a void*
      utb__addEntry( utb, ProfileEntry2('d', *((void**)(&text[0]))) );
    }
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
    utb__addEntry( utb, ProfileEntry2('c', sampleLabel) );
    lineno = entry.line();
  }
  if (lineno != -1) {
    utb__addEntry( utb, ProfileEntry2('n', lineno) );
  }

  // Add a pseudostack-entry end label
  utb__addEntry( utb, ProfileEntry2('h', 'Q') );
}

// RUNS IN SIGHANDLER CONTEXT
// Generate pseudo-backtrace entries and put them in |utb|, with
// the order outermost frame first.
void genPseudoBacktraceEntries(/*MODIFIED*/UnwinderThreadBuffer* utb,
                               PseudoStack *aStack, TickSample *sample)
{
  // Call genProfileEntry2 to generate tags for each profile
  // entry.  Each entry will be bounded by a 'h' 'P' tag to
  // mark the start and a 'h' 'Q' tag to mark the end.
  uint32_t nInStack = aStack->stackSize();
  for (uint32_t i = 0; i < nInStack; i++) {
    genProfileEntry2(utb, aStack->mStack[i], aStack, nullptr);
  }
# ifdef ENABLE_SPS_LEAF_DATA
  if (sample) {
    utb__addEntry( utb, ProfileEntry2('l', (void*)sample->pc) );
#   ifdef ENABLE_ARM_LR_SAVING
    utb__addEntry( utb, ProfileEntry2('L', (void*)sample->lr) );
#   endif
  }
# endif
}

/* used to keep track of the last event that we sampled during */
static unsigned int sLastSampledEventGeneration = 0;

/* a counter that's incremented everytime we get responsiveness event
 * note: it might also be worth tracking everytime we go around
 * the event loop */
static unsigned int sCurrentEventGeneration = 0;
/* we don't need to worry about overflow because we only treat the
 * case of them being the same as special. i.e. we only run into
 * a problem if 2^32 events happen between samples that we need
 * to know are associated with different events */

// RUNS IN SIGHANDLER CONTEXT
void TableTicker2::Tick(TickSample* sample)
{
  /* Get hold of an empty inter-thread buffer into which to park
     the ProfileEntries for this sample. */
  UnwinderThreadBuffer* utb = uwt__acquire_empty_buffer();

  /* This could fail, if no buffers are currently available, in which
     case we must give up right away.  We cannot wait for a buffer to
     become available, as that risks deadlock. */
  if (!utb)
    return;

  /* Manufacture the ProfileEntries that we will give to the unwinder
     thread, and park them in |utb|. */

  // Marker(s) come before the sample
  PseudoStack* stack = mPrimaryThreadProfile.GetPseudoStack();
  for (int i = 0; stack->getMarker(i) != NULL; i++) {
    utb__addEntry( utb, ProfileEntry2('m', stack->getMarker(i)) );
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
      mPrimaryThreadProfile.erase();
    }
    sLastSampledEventGeneration = sCurrentEventGeneration;

    recordSample = false;
    // only record the events when we have a we haven't seen a tracer
    // event for 100ms
    if (!sLastTracerEvent.IsNull()) {
      TimeDuration delta = sample->timestamp - sLastTracerEvent;
      if (delta.ToMilliseconds() > 100.0) {
          recordSample = true;
      }
    }
  }

  // JRS 2012-Sept-27: this logic used to involve mUseStackWalk.
  // That should be reinstated, but for the moment, use the
  // settings in sUnwindMode and sUnwindInterval.
  // Add a native-backtrace request, or add pseudo backtrace entries,
  // or both.
  switch (sUnwindMode) {
    case UnwNATIVE: /* Native only */
      // add a "do native stack trace now" hint.  This will be actioned
      // by the unwinder thread as it processes the entries in this
      // sample.
      utb__addEntry( utb, ProfileEntry2('h'/*hint*/, 'N'/*native-trace*/) );
      break;
    case UnwPSEUDO: /* Pseudo only */
      /* Add into |utb|, the pseudo backtrace entries */
      genPseudoBacktraceEntries(utb, stack, sample);
      break;
    case UnwCOMBINED: /* Both Native and Pseudo */
      utb__addEntry( utb, ProfileEntry2('h'/*hint*/, 'N'/*native-trace*/) );
      genPseudoBacktraceEntries(utb, stack, sample);
      break;
    case UnwINVALID:
    default:
      MOZ_CRASH();
  }

  if (recordSample) {    
    // add a "flush now" hint
    utb__addEntry( utb, ProfileEntry2('h'/*hint*/, 'F'/*flush*/) );
  }

  // Add any extras
  if (!sLastTracerEvent.IsNull() && sample) {
    TimeDuration delta = sample->timestamp - sLastTracerEvent;
    utb__addEntry( utb, ProfileEntry2('r', delta.ToMilliseconds()) );
  }

  if (sample) {
    TimeDuration delta = sample->timestamp - mStartTime;
    utb__addEntry( utb, ProfileEntry2('t', delta.ToMilliseconds()) );
  }

  if (sLastFrameNumber != sFrameNumber) {
    utb__addEntry( utb, ProfileEntry2('f', sFrameNumber) );
    sLastFrameNumber = sFrameNumber;
  }

  /* So now we have, in |utb|, the complete set of entries we want to
     push into the circular buffer.  This may also include a 'h' 'F'
     entry, which is "flush now" hint, and/or a 'h' 'N' entry, which
     is a "generate a native backtrace and add it to the buffer right
     now" hint.  Hand them off to the helper thread, together with
     stack and register context needed to do a native unwind, if that
     is currently enabled. */

  /* If a native unwind has been requested, we'll start it off using
     the context obtained from the signal handler, to avoid the
     problem of having to unwind through the signal frame itself. */

  /* On Linux and Android, the initial register state is in the
     supplied sample->context.  But on MacOS it's not, so we have to
     fake it up here (sigh). */
  if (sUnwindMode == UnwNATIVE || sUnwindMode == UnwCOMBINED) {
#   if defined(SPS_PLAT_amd64_linux) || defined(SPS_PLAT_arm_android) \
       || defined(SPS_PLAT_x86_linux) || defined(SPS_PLAT_x86_android)
    void* ucV = (void*)sample->context;
#   elif defined(SPS_PLAT_amd64_darwin)
    struct __darwin_mcontext64 mc;
    memset(&mc, 0, sizeof(mc));
    ucontext_t uc;
    memset(&uc, 0, sizeof(uc));
    uc.uc_mcontext = &mc;
    mc.__ss.__rip = (uint64_t)sample->pc;
    mc.__ss.__rsp = (uint64_t)sample->sp;
    mc.__ss.__rbp = (uint64_t)sample->fp;
    void* ucV = (void*)&uc;
#   elif defined(SPS_PLAT_x86_darwin)
    struct __darwin_mcontext32 mc;
    memset(&mc, 0, sizeof(mc));
    ucontext_t uc;
    memset(&uc, 0, sizeof(uc));
    uc.uc_mcontext = &mc;
    mc.__ss.__eip = (uint32_t)sample->pc;
    mc.__ss.__esp = (uint32_t)sample->sp;
    mc.__ss.__ebp = (uint32_t)sample->fp;
    void* ucV = (void*)&uc;
#   elif defined(SPS_OS_windows)
    /* Totally fake this up so it at least builds.  No idea if we can
       even ever get here on Windows. */
    void* ucV = NULL;
#   else
#     error "Unsupported platform"
#   endif
    uwt__release_full_buffer(&mPrimaryThreadProfile, utb, ucV);
  } else {
    uwt__release_full_buffer(&mPrimaryThreadProfile, utb, NULL);
  }
}

// END take samples
////////////////////////////////////////////////////////////////////////


std::ostream& operator<<(std::ostream& stream, const ThreadProfile2& profile)
{
  int readPos = profile.mReadPos;
  while (readPos != profile.mLastFlushPos) {
    stream << profile.mEntries[readPos];
    readPos = (readPos + 1) % profile.mEntrySize;
  }
  return stream;
}

std::ostream& operator<<(std::ostream& stream, const ProfileEntry2& entry)
{
  if (entry.mTagName == 'r' || entry.mTagName == 't') {
    stream << entry.mTagName << "-" << std::fixed << entry.mTagFloat << "\n";
  } else if (entry.mTagName == 'l' || entry.mTagName == 'L') {
    // Bug 739800 - Force l-tag addresses to have a "0x" prefix on all platforms
    // Additionally, stringstream seemed to be ignoring formatter flags.
    char tagBuff[1024];
    unsigned long long pc = (unsigned long long)(uintptr_t)entry.mTagPtr;
    snprintf(tagBuff, 1024, "%c-%#llx\n", entry.mTagName, pc);
    stream << tagBuff;
  } else if (entry.mTagName == 'd') {
    // TODO implement 'd' tag for text profile
  } else {
    stream << entry.mTagName << "-" << entry.mTagData << "\n";
  }
  return stream;
}

static inline const char* name_UnwMode(UnwMode m)
{
  switch (m) {
    case UnwINVALID:  return "invalid";
    case UnwNATIVE:   return "native";
    case UnwPSEUDO:   return "pseudo";
    case UnwCOMBINED: return "combined";
    default:          return "??name_UnwMode??";
  }
}

// Read env vars at startup, so as to set sUnwindMode and sInterval.
static void read_env_vars()
{
  bool nativeAvail = false;
# if defined(HAVE_NATIVE_UNWIND)
  nativeAvail = true;
# endif

  MOZ_ASSERT(sUnwindMode     == UnwINVALID);
  MOZ_ASSERT(sUnwindInterval == 0);

  /* Set defaults */
  sUnwindMode     = nativeAvail ? UnwCOMBINED : UnwPSEUDO;
  sUnwindInterval = 0;  /* We'll have to look elsewhere */

  const char* strM = PR_GetEnv("MOZ_PROFILER_MODE");
  const char* strI = PR_GetEnv("MOZ_PROFILER_INTERVAL");

  if (strM) {
    if (0 == strcmp(strM, "pseudo"))
      sUnwindMode = UnwPSEUDO;
    else if (0 == strcmp(strM, "native") && nativeAvail)
      sUnwindMode = UnwNATIVE;
    else if (0 == strcmp(strM, "combined") && nativeAvail)
      sUnwindMode = UnwCOMBINED;
    else goto usage;
  }

  if (strI) {
    errno = 0;
    long int n = strtol(strI, (char**)NULL, 10);
    if (errno == 0 && n >= 1 && n <= 1000) {
      sUnwindInterval = n;
    }
    else goto usage;
  }

  goto out;

 usage:
  LOG( "SPS: ");
  LOG( "SPS: Environment variable usage:");
  LOG( "SPS: ");
  LOG( "SPS:   MOZ_PROFILER_MODE=native    for native unwind only");
  LOG( "SPS:   MOZ_PROFILER_MODE=pseudo    for pseudo unwind only");
  LOG( "SPS:   MOZ_PROFILER_MODE=combined  for combined native & pseudo unwind");
  LOG( "SPS:   If unset, default is 'combined' on native-capable");
  LOG( "SPS:     platforms, 'pseudo' on others.");
  LOG( "SPS: ");
  LOG( "SPS:   MOZ_PROFILER_INTERVAL=<number>   (milliseconds, 1 to 1000)");
  LOG( "SPS:   If unset, platform default is used.");
  LOG( "SPS: ");
  LOGF("SPS:   This platform %s native unwinding.",
       nativeAvail ? "supports" : "does not support");
  LOG( "SPS: ");
  /* Re-set defaults */
  sUnwindMode       = nativeAvail ? UnwCOMBINED : UnwPSEUDO;
  sUnwindInterval   = 0;  /* We'll have to look elsewhere */

 out:
  LOG( "SPS:");
  LOGF("SPS: Unwind mode       = %s", name_UnwMode(sUnwindMode));
  LOGF("SPS: Sampling interval = %d ms (zero means \"platform default\")",
       (int)sUnwindInterval);
  LOG( "SPS: Use env var MOZ_PROFILER_MODE=help for further information.");
  LOG( "SPS:");

  return;
}


////////////////////////////////////////////////////////////////////////
// BEGIN externally visible functions

void mozilla_sampler_init2()
{
  if (stack_key_initialized)
    return;

  LOG("BEGIN mozilla_sampler_init2");
  if (!tlsPseudoStack.init() || !tlsTicker.init()) {
    LOG("Failed to init.");
    return;
  }
  stack_key_initialized = true;

  PseudoStack *stack = new PseudoStack();
  tlsPseudoStack.set(stack);

  // Read mode settings from MOZ_PROFILER_MODE and interval
  // settings from MOZ_PROFILER_INTERVAL.
  read_env_vars();

  // Create the unwinder thread.  ATM there is only one.
  uwt__init();

# if defined(SPS_PLAT_amd64_linux) || defined(SPS_PLAT_arm_android) \
     || defined(SPS_PLAT_x86_linux) || defined(SPS_PLAT_x86_android) \
     || defined(SPS_PLAT_x86_windows) || defined(SPS_PLAT_amd64_windows) /* no idea if windows is correct */
  // On Linuxes, register this thread (temporarily) for profiling
  int aLocal;
  uwt__register_thread_for_profiling( &aLocal );
# elif defined(SPS_PLAT_amd64_darwin) || defined(SPS_PLAT_x86_darwin)
  // Registration is done in platform-macos.cc
# else
#   error "Unknown plat"
# endif

  // Allow the profiler to be started using signals
  OS::RegisterStartHandler();

  // We can't open pref so we use an environment variable
  // to know if we should trigger the profiler on startup
  // NOTE: Default
  const char *val = PR_GetEnv("MOZ_PROFILER_STARTUP");
  if (!val || !*val) {
    return;
  }

  const char* features[] = {"js"
#if defined(XP_WIN) || defined(XP_MACOSX)
                         , "stackwalk"
#endif
                         };
  mozilla_sampler_start2(PROFILE_DEFAULT_ENTRY, PROFILE_DEFAULT_INTERVAL,
                         features, sizeof(features)/sizeof(const char*));
  LOG("END   mozilla_sampler_init2");
}

void mozilla_sampler_shutdown2()
{
  // Shut down and reap the unwinder thread.  We have to do this
  // before stopping the sampler, so as to guarantee that the unwinder
  // thread doesn't try to access memory that the subsequent call to
  // mozilla_sampler_stop2 causes to be freed.
  uwt__deinit();

  mozilla_sampler_stop2();
  // We can't delete the Stack because we can be between a
  // sampler call_enter/call_exit point.
  // TODO Need to find a safe time to delete Stack
}

void mozilla_sampler_save2()
{
  TableTicker2 *t = tlsTicker.get();
  if (!t) {
    return;
  }

  t->RequestSave();
  // We're on the main thread already so we don't
  // have to wait to handle the save request.
  t->HandleSaveRequest();
}

char* mozilla_sampler_get_profile2()
{
  TableTicker2 *t = tlsTicker.get();
  if (!t) {
    return NULL;
  }

  std::stringstream profile;
  t->SetPaused(true);
  profile << *(t->GetPrimaryThreadProfile());
  t->SetPaused(false);

  std::string profileString = profile.str();
  char *rtn = (char*)malloc( (profileString.length() + 1) * sizeof(char) );
  strcpy(rtn, profileString.c_str());
  return rtn;
}

JSObject *mozilla_sampler_get_profile_data2(JSContext *aCx)
{
  TableTicker2 *t = tlsTicker.get();
  if (!t) {
    return NULL;
  }

  return t->ToJSObject(aCx);
}


const char** mozilla_sampler_get_features2()
{
  static const char* features[] = {
#if defined(MOZ_PROFILING) && defined(HAVE_NATIVE_UNWIND)
    "stackwalk",
#endif
    "jank",
    "js",
    NULL
  };

  return features;
}

// Values are only honored on the first start
void mozilla_sampler_start2(int aProfileEntries, int aInterval,
                            const char** aFeatures, uint32_t aFeatureCount)
{
  if (!stack_key_initialized)
    mozilla_sampler_init2();

  /* If the sampling interval was set using env vars, use that
     in preference to anything else. */
  if (sUnwindInterval > 0)
    aInterval = sUnwindInterval;

  PseudoStack *stack = tlsPseudoStack.get();
  if (!stack) {
    ASSERT(false);
    return;
  }

  mozilla_sampler_stop2();
  TableTicker2 *t
    = new TableTicker2(aInterval ? aInterval : PROFILE_DEFAULT_INTERVAL,
                      aProfileEntries ? aProfileEntries : PROFILE_DEFAULT_ENTRY,
                      stack, aFeatures, aFeatureCount);
  tlsTicker.set(t);
  t->Start();
  if (t->ProfileJS())
      stack->enableJSSampling();

  nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
  if (os)
    os->NotifyObservers(nullptr, "profiler-started", nullptr);
}

void mozilla_sampler_stop2()
{
  if (!stack_key_initialized)
    mozilla_sampler_init2();

  TableTicker2 *t = tlsTicker.get();
  if (!t) {
    return;
  }

  bool disableJS = t->ProfileJS();

  t->Stop();
  delete t;
  tlsTicker.set(NULL);
  PseudoStack *stack = tlsPseudoStack.get();
  ASSERT(stack != NULL);

  if (disableJS)
    stack->disableJSSampling();

  nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
  if (os)
    os->NotifyObservers(nullptr, "profiler-stopped", nullptr);
}

bool mozilla_sampler_is_active2()
{
  if (!stack_key_initialized)
    mozilla_sampler_init2();

  TableTicker2 *t = tlsTicker.get();
  if (!t) {
    return false;
  }

  return t->IsActive();
}

static double sResponsivenessTimes[100];
static unsigned int sResponsivenessLoc = 0;
void mozilla_sampler_responsiveness2(TimeStamp aTime)
{
  if (!sLastTracerEvent.IsNull()) {
    if (sResponsivenessLoc == 100) {
      for(size_t i = 0; i < 100-1; i++) {
        sResponsivenessTimes[i] = sResponsivenessTimes[i+1];
      }
      sResponsivenessLoc--;
    }
    TimeDuration delta = aTime - sLastTracerEvent;
    sResponsivenessTimes[sResponsivenessLoc++] = delta.ToMilliseconds();
  }
  sCurrentEventGeneration++;

  sLastTracerEvent = aTime;
}

const double* mozilla_sampler_get_responsiveness2()
{
  return sResponsivenessTimes;
}

void mozilla_sampler_frame_number2(int frameNumber)
{
  sFrameNumber = frameNumber;
}

void mozilla_sampler_print_location2()
{
  // FIXME
}

void mozilla_sampler_lock2()
{
  // FIXME!
}

void mozilla_sampler_unlock2()
{
  // FIXME!
}

// END externally visible functions
////////////////////////////////////////////////////////////////////////
