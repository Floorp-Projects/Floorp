/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <ostream>
#include <fstream>
#include <sstream>
#include <errno.h>

#include "ProfilerIOInterposeObserver.h"
#include "platform.h"
#include "PlatformMacros.h"
#include "prenv.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/ThreadLocal.h"
#include "PseudoStack.h"
#include "TableTicker.h"
#include "nsIObserverService.h"
#include "nsDirectoryServiceUtils.h"
#include "nsDirectoryServiceDefs.h"
#include "nsProfilerStartParams.h"
#include "mozilla/Services.h"
#include "nsThreadUtils.h"
#include "ProfilerMarkers.h"
#include "nsXULAppAPI.h"

#if defined(SPS_OS_android) && !defined(MOZ_WIDGET_GONK)
  #include "AndroidBridge.h"
#endif

#if defined(SPS_PLAT_amd64_linux) || defined(SPS_PLAT_x86_linux)
# define USE_LUL_STACKWALK
# include "LulMain.h"
# include "platform-linux-lul.h"
#endif

mozilla::ThreadLocal<PseudoStack *> tlsPseudoStack;
mozilla::ThreadLocal<TableTicker *> tlsTicker;
mozilla::ThreadLocal<void *> tlsStackTop;
// We need to track whether we've been initialized otherwise
// we end up using tlsStack without initializing it.
// Because tlsStack is totally opaque to us we can't reuse
// it as the flag itself.
bool stack_key_initialized;

mozilla::TimeStamp   sLastTracerEvent; // is raced on
mozilla::TimeStamp   sStartTime;
int         sFrameNumber = 0;
int         sLastFrameNumber = 0;
int         sInitCount = 0; // Each init must have a matched shutdown.
static bool sIsProfiling = false; // is raced on
static bool sIsGPUProfiling = false; // is raced on
static bool sIsLayersDump = false; // is raced on
static bool sIsDisplayListDump = false; // is raced on
static bool sIsRestyleProfiling = false; // is raced on

// Environment variables to control the profiler
const char* PROFILER_HELP = "MOZ_PROFILER_HELP";
const char* PROFILER_INTERVAL = "MOZ_PROFILER_INTERVAL";
const char* PROFILER_ENTRIES = "MOZ_PROFILER_ENTRIES";
const char* PROFILER_STACK = "MOZ_PROFILER_STACK_SCAN";
const char* PROFILER_FEATURES = "MOZ_PROFILING_FEATURES";

/* we don't need to worry about overflow because we only treat the
 * case of them being the same as special. i.e. we only run into
 * a problem if 2^32 events happen between samples that we need
 * to know are associated with different events */

// Values harvested from env vars, that control the profiler.
static int sUnwindInterval;   /* in milliseconds */
static int sUnwindStackScan;  /* max # of dubious frames allowed */
static int sProfileEntries;   /* how many entries do we store? */

std::vector<ThreadInfo*>* Sampler::sRegisteredThreads = nullptr;
mozilla::Mutex* Sampler::sRegisteredThreadsMutex = nullptr;

TableTicker* Sampler::sActiveSampler;

static mozilla::StaticAutoPtr<mozilla::ProfilerIOInterposeObserver>
                                                            sInterposeObserver;

// The name that identifies the gecko thread for calls to
// profiler_register_thread.
static const char * gGeckoThreadName = "GeckoMain";

void Sampler::Startup() {
  sRegisteredThreads = new std::vector<ThreadInfo*>();
  sRegisteredThreadsMutex = new mozilla::Mutex("sRegisteredThreads mutex");

  // We could create the sLUL object and read unwind info into it at
  // this point.  That would match the lifetime implied by destruction
  // of it in Sampler::Shutdown just below.  However, that gives a big
  // delay on startup, even if no profiling is actually to be done.
  // So, instead, sLUL is created on demand at the first call to
  // Sampler::Start.
}

void Sampler::Shutdown() {
  while (sRegisteredThreads->size() > 0) {
    delete sRegisteredThreads->back();
    sRegisteredThreads->pop_back();
  }

  delete sRegisteredThreadsMutex;
  delete sRegisteredThreads;

  // UnregisterThread can be called after shutdown in XPCShell. Thus
  // we need to point to null to ignore such a call after shutdown.
  sRegisteredThreadsMutex = nullptr;
  sRegisteredThreads = nullptr;

#if defined(USE_LUL_STACKWALK)
  // Delete the sLUL object, if it actually got created.
  if (sLUL) {
    delete sLUL;
    sLUL = nullptr;
  }
#endif
}

ThreadInfo::ThreadInfo(const char* aName, int aThreadId,
                       bool aIsMainThread, PseudoStack* aPseudoStack,
                       void* aStackTop)
  : mName(strdup(aName))
  , mThreadId(aThreadId)
  , mIsMainThread(aIsMainThread)
  , mPseudoStack(aPseudoStack)
  , mPlatformData(Sampler::AllocPlatformData(aThreadId))
  , mProfile(nullptr)
  , mStackTop(aStackTop)
  , mPendingDelete(false)
{
  mThread = NS_GetCurrentThread();
}

ThreadInfo::~ThreadInfo() {
  free(mName);

  if (mProfile)
    delete mProfile;

  Sampler::FreePlatformData(mPlatformData);
}

void
ThreadInfo::SetPendingDelete()
{
  mPendingDelete = true;
  // We don't own the pseudostack so disconnect it.
  mPseudoStack = nullptr;
  if (mProfile) {
    mProfile->SetPendingDelete();
  }
}

StackOwningThreadInfo::StackOwningThreadInfo(const char* aName, int aThreadId,
                                             bool aIsMainThread,
                                             PseudoStack* aPseudoStack,
                                             void* aStackTop)
  : ThreadInfo(aName, aThreadId, aIsMainThread, aPseudoStack, aStackTop)
{
  aPseudoStack->ref();
}

StackOwningThreadInfo::~StackOwningThreadInfo()
{
  PseudoStack* stack = Stack();
  if (stack) {
    stack->deref();
  }
}

void
StackOwningThreadInfo::SetPendingDelete()
{
  PseudoStack* stack = Stack();
  if (stack) {
    stack->deref();
  }
  ThreadInfo::SetPendingDelete();
}

ProfilerMarker::ProfilerMarker(const char* aMarkerName,
    ProfilerMarkerPayload* aPayload,
    float aTime)
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

float
ProfilerMarker::GetTime() const {
  return mTime;
}

void ProfilerMarker::StreamJSON(SpliceableJSONWriter& aWriter,
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
          mPayload->StreamPayload(aWriter, aUniqueStacks);
      }
      aWriter.EndObject();
    }
  }
  aWriter.EndArray();
}

/* Has MOZ_PROFILER_VERBOSE been set? */

// Verbosity control for the profiler.  The aim is to check env var
// MOZ_PROFILER_VERBOSE only once.  However, we may need to temporarily
// override that so as to print the profiler's help message.  That's
// what moz_profiler_set_verbosity is for.

enum class ProfilerVerbosity : int8_t { UNCHECKED, NOTVERBOSE, VERBOSE };

// Raced on, potentially
static ProfilerVerbosity profiler_verbosity = ProfilerVerbosity::UNCHECKED;

bool moz_profiler_verbose()
{
  if (profiler_verbosity == ProfilerVerbosity::UNCHECKED) {
    if (PR_GetEnv("MOZ_PROFILER_VERBOSE") != nullptr)
      profiler_verbosity = ProfilerVerbosity::VERBOSE;
    else
      profiler_verbosity = ProfilerVerbosity::NOTVERBOSE;
  }

  return profiler_verbosity == ProfilerVerbosity::VERBOSE;
}

void moz_profiler_set_verbosity(ProfilerVerbosity pv)
{
   MOZ_ASSERT(pv == ProfilerVerbosity::UNCHECKED ||
              pv == ProfilerVerbosity::VERBOSE);
   profiler_verbosity = pv;
}


bool set_profiler_interval(const char* interval) {
  if (interval) {
    errno = 0;
    long int n = strtol(interval, (char**)nullptr, 10);
    if (errno == 0 && n >= 1 && n <= 1000) {
      sUnwindInterval = n;
      return true;
    }
    return false;
  }

  return true;
}

bool set_profiler_entries(const char* entries) {
  if (entries) {
    errno = 0;
    long int n = strtol(entries, (char**)nullptr, 10);
    if (errno == 0 && n > 0) {
      sProfileEntries = n;
      return true;
    }
    return false;
  }

  return true;
}

bool set_profiler_scan(const char* scanCount) {
  if (scanCount) {
    errno = 0;
    long int n = strtol(scanCount, (char**)nullptr, 10);
    if (errno == 0 && n >= 0 && n <= 100) {
      sUnwindStackScan = n;
      return true;
    }
    return false;
  }

  return true;
}

bool is_native_unwinding_avail() {
# if defined(HAVE_NATIVE_UNWIND)
  return true;
#else
  return false;
#endif
}

// Read env vars at startup, so as to set:
//   sUnwindInterval, sProfileEntries, sUnwindStackScan.
void read_profiler_env_vars()
{
  /* Set defaults */
  sUnwindInterval = 0;  /* We'll have to look elsewhere */
  sProfileEntries = 0;

  const char* interval = PR_GetEnv(PROFILER_INTERVAL);
  const char* entries = PR_GetEnv(PROFILER_ENTRIES);
  const char* scanCount = PR_GetEnv(PROFILER_STACK);

  if (PR_GetEnv(PROFILER_HELP)) {
     // Enable verbose output
     moz_profiler_set_verbosity(ProfilerVerbosity::VERBOSE);
     profiler_usage();
     // Now force the next enquiry of moz_profiler_verbose to re-query
     // env var MOZ_PROFILER_VERBOSE.
     moz_profiler_set_verbosity(ProfilerVerbosity::UNCHECKED);
  }

  if (!set_profiler_interval(interval) ||
      !set_profiler_entries(entries) ||
      !set_profiler_scan(scanCount)) {
      profiler_usage();
  } else {
    LOG( "SPS:");
    LOGF("SPS: Sampling interval = %d ms (zero means \"platform default\")",
        (int)sUnwindInterval);
    LOGF("SPS: Entry store size  = %d (zero means \"platform default\")",
        (int)sProfileEntries);
    LOGF("SPS: UnwindStackScan   = %d (max dubious frames per unwind).",
        (int)sUnwindStackScan);
    LOG( "SPS:");
  }
}

void profiler_usage() {
  LOG( "SPS: ");
  LOG( "SPS: Environment variable usage:");
  LOG( "SPS: ");
  LOG( "SPS:   MOZ_PROFILER_HELP");
  LOG( "SPS:   If set to any value, prints this message.");
  LOG( "SPS: ");
  LOG( "SPS:   MOZ_PROFILER_INTERVAL=<number>   (milliseconds, 1 to 1000)");
  LOG( "SPS:   If unset, platform default is used.");
  LOG( "SPS: ");
  LOG( "SPS:   MOZ_PROFILER_ENTRIES=<number>    (count, minimum of 1)");
  LOG( "SPS:   If unset, platform default is used.");
  LOG( "SPS: ");
  LOG( "SPS:   MOZ_PROFILER_VERBOSE");
  LOG( "SPS:   If set to any value, increases verbosity (recommended).");
  LOG( "SPS: ");
  LOG( "SPS:   MOZ_PROFILER_STACK_SCAN=<number>   (default is zero)");
  LOG( "SPS:   The number of dubious (stack-scanned) frames allowed");
  LOG( "SPS: ");
  LOG( "SPS:   MOZ_PROFILER_LUL_TEST");
  LOG( "SPS:   If set to any value, runs LUL unit tests at startup of");
  LOG( "SPS:   the unwinder thread, and prints a short summary of results.");
  LOG( "SPS: ");
  LOGF("SPS:   This platform %s native unwinding.",
       is_native_unwinding_avail() ? "supports" : "does not support");
  LOG( "SPS: ");

  /* Re-set defaults */
  sUnwindInterval   = 0;  /* We'll have to look elsewhere */
  sProfileEntries   = 0;
  sUnwindStackScan  = 0;

  LOG( "SPS:");
  LOGF("SPS: Sampling interval = %d ms (zero means \"platform default\")",
       (int)sUnwindInterval);
  LOGF("SPS: Entry store size  = %d (zero means \"platform default\")",
       (int)sProfileEntries);
  LOGF("SPS: UnwindStackScan   = %d (max dubious frames per unwind).",
       (int)sUnwindStackScan);
  LOG( "SPS:");

  return;
}

void set_tls_stack_top(void* stackTop)
{
  // Round |stackTop| up to the end of the containing page.  We may
  // as well do this -- there's no danger of a fault, and we might
  // get a few more base-of-the-stack frames as a result.  This
  // assumes that no target has a page size smaller than 4096.
  uintptr_t stackTopR = (uintptr_t)stackTop;
  if (stackTop) {
    stackTopR = (stackTopR & ~(uintptr_t)4095) + (uintptr_t)4095;
  }
  tlsStackTop.set((void*)stackTopR);
}

bool is_main_thread_name(const char* aName) {
  if (!aName) {
    return false;
  }
  return strcmp(aName, gGeckoThreadName) == 0;
}

#ifdef HAVE_VA_COPY
#define VARARGS_ASSIGN(foo, bar)        VA_COPY(foo,bar)
#elif defined(HAVE_VA_LIST_AS_ARRAY)
#define VARARGS_ASSIGN(foo, bar)     foo[0] = bar[0]
#else
#define VARARGS_ASSIGN(foo, bar)     (foo) = (bar)
#endif

void
mozilla_sampler_log(const char *fmt, va_list args)
{
  if (profiler_is_active()) {
    // nsAutoCString AppendPrintf would be nicer but
    // this is mozilla external code
    char buf[2048];
    va_list argsCpy;
    VARARGS_ASSIGN(argsCpy, args);
    int required = vsnprintf(buf, sizeof(buf), fmt, argsCpy);
    va_end(argsCpy);

    if (required < 0) {
      return; // silently drop for now
    } else if (required < 2048) {
      profiler_tracing("log", buf, TRACING_EVENT);
    } else {
      char* heapBuf = new char[required+1];
      va_list argsCpy;
      VARARGS_ASSIGN(argsCpy, args);
      vsnprintf(heapBuf, required+1, fmt, argsCpy);
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

void mozilla_sampler_init(void* stackTop)
{
  sInitCount++;

  if (stack_key_initialized)
    return;

  LOG("BEGIN mozilla_sampler_init");
  if (!tlsPseudoStack.init() || !tlsTicker.init() || !tlsStackTop.init()) {
    LOG("Failed to init.");
    return;
  }
  stack_key_initialized = true;

  Sampler::Startup();

  PseudoStack *stack = PseudoStack::create();
  tlsPseudoStack.set(stack);

  bool isMainThread = true;
  Sampler::RegisterCurrentThread(isMainThread ?
                                   gGeckoThreadName : "Application Thread",
                                 stack, isMainThread, stackTop);

  // Read interval settings from MOZ_PROFILER_INTERVAL and stack-scan
  // threshhold from MOZ_PROFILER_STACK_SCAN.
  read_profiler_env_vars();

  // platform specific initialization
  OS::Startup();

  set_stderr_callback(mozilla_sampler_log);

  // We can't open pref so we use an environment variable
  // to know if we should trigger the profiler on startup
  // NOTE: Default
  const char *val = PR_GetEnv("MOZ_PROFILER_STARTUP");
  if (!val || !*val) {
    return;
  }

  const char* features[] = {"js"
                         , "leaf"
                         , "threads"
#if defined(XP_WIN) || defined(XP_MACOSX) \
    || (defined(SPS_ARCH_arm) && defined(linux)) \
    || defined(SPS_PLAT_amd64_linux) || defined(SPS_PLAT_x86_linux)
                         , "stackwalk"
#endif
#if defined(SPS_OS_android) && !defined(MOZ_WIDGET_GONK)
                         , "java"
#endif
                         };

  const char* threadFilters[] = { "GeckoMain", "Compositor" };

  profiler_start(PROFILE_DEFAULT_ENTRY, PROFILE_DEFAULT_INTERVAL,
                         features, MOZ_ARRAY_LENGTH(features),
                         threadFilters, MOZ_ARRAY_LENGTH(threadFilters));
  LOG("END   mozilla_sampler_init");
}

void mozilla_sampler_shutdown()
{
  sInitCount--;

  if (sInitCount > 0)
    return;

  // Save the profile on shutdown if requested.
  TableTicker *t = tlsTicker.get();
  if (t) {
    const char *val = PR_GetEnv("MOZ_PROFILER_SHUTDOWN");
    if (val) {
      std::ofstream stream;
      stream.open(val);
      if (stream.is_open()) {
        t->ToStreamAsJSON(stream);
        stream.close();
      }
    }
  }

  profiler_stop();

  set_stderr_callback(nullptr);

  Sampler::Shutdown();

  PseudoStack *stack = tlsPseudoStack.get();
  stack->deref();
  tlsPseudoStack.set(nullptr);
}

void mozilla_sampler_save()
{
  TableTicker *t = tlsTicker.get();
  if (!t) {
    return;
  }

  t->RequestSave();
  // We're on the main thread already so we don't
  // have to wait to handle the save request.
  t->HandleSaveRequest();
}

char* mozilla_sampler_get_profile(float aSinceTime)
{
  TableTicker *t = tlsTicker.get();
  if (!t) {
    return nullptr;
  }

  std::stringstream stream;
  t->ToStreamAsJSON(stream, aSinceTime);
  char* profile = strdup(stream.str().c_str());
  return profile;
}

JSObject *mozilla_sampler_get_profile_data(JSContext *aCx, float aSinceTime)
{
  TableTicker *t = tlsTicker.get();
  if (!t) {
    return nullptr;
  }

  return t->ToJSObject(aCx, aSinceTime);
}

void mozilla_sampler_save_profile_to_file(const char* aFilename)
{
  TableTicker *t = tlsTicker.get();
  if (!t) {
    return;
  }

  std::ofstream stream;
  stream.open(aFilename);
  if (stream.is_open()) {
    t->ToStreamAsJSON(stream);
    stream.close();
    LOGF("Saved to %s", aFilename);
  } else {
    LOG("Fail to open profile log file.");
  }
}


const char** mozilla_sampler_get_features()
{
  static const char* features[] = {
#if defined(MOZ_PROFILING) && defined(HAVE_NATIVE_UNWIND)
    // Walk the C++ stack.
    "stackwalk",
#endif
#if defined(ENABLE_SPS_LEAF_DATA)
    // Include the C++ leaf node if not stackwalking. DevTools
    // profiler doesn't want the native addresses.
    "leaf",
#endif
#if !defined(SPS_OS_windows)
    // Use a seperate thread of walking the stack.
    "unwinder",
#endif
    "java",
    // Only record samples during periods of bad responsiveness
    "jank",
    // Tell the JS engine to emmit pseudostack entries in the
    // pro/epilogue.
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
#ifdef MOZ_TASK_TRACER
    // Start profiling with feature TaskTracer.
    "tasktracer",
#endif
#if defined(XP_WIN)
    // Add power collection
    "power",
#endif
    nullptr
  };

  return features;
}

void mozilla_sampler_get_buffer_info(uint32_t *aCurrentPosition, uint32_t *aTotalSize,
                                     uint32_t *aGeneration)
{
  *aCurrentPosition = 0;
  *aTotalSize = 0;
  *aGeneration = 0;

  if (!stack_key_initialized)
    return;

  TableTicker *t = tlsTicker.get();
  if (!t)
    return;

  t->GetBufferInfo(aCurrentPosition, aTotalSize, aGeneration);
}

// Values are only honored on the first start
void mozilla_sampler_start(int aProfileEntries, double aInterval,
                           const char** aFeatures, uint32_t aFeatureCount,
                           const char** aThreadNameFilters, uint32_t aFilterCount)

{
  LOG("BEGIN mozilla_sampler_start");

  if (!stack_key_initialized)
    profiler_init(nullptr);

  /* If the sampling interval was set using env vars, use that
     in preference to anything else. */
  if (sUnwindInterval > 0)
    aInterval = sUnwindInterval;

  /* If the entry count was set using env vars, use that, too: */
  if (sProfileEntries > 0)
    aProfileEntries = sProfileEntries;

  // Reset the current state if the profiler is running
  profiler_stop();

  TableTicker* t;
  t = new TableTicker(aInterval ? aInterval : PROFILE_DEFAULT_INTERVAL,
                      aProfileEntries ? aProfileEntries : PROFILE_DEFAULT_ENTRY,
                      aFeatures, aFeatureCount,
                      aThreadNameFilters, aFilterCount);

  tlsTicker.set(t);
  t->Start();
  if (t->ProfileJS() || t->InPrivacyMode()) {
      mozilla::MutexAutoLock lock(*Sampler::sRegisteredThreadsMutex);
      std::vector<ThreadInfo*> threads = t->GetRegisteredThreads();

      for (uint32_t i = 0; i < threads.size(); i++) {
        ThreadInfo* info = threads[i];
        if (info->IsPendingDelete()) {
          continue;
        }
        ThreadProfile* thread_profile = info->Profile();
        if (!thread_profile) {
          continue;
        }
        thread_profile->GetPseudoStack()->reinitializeOnResume();
        if (t->ProfileJS()) {
          thread_profile->GetPseudoStack()->enableJSSampling();
        }
        if (t->InPrivacyMode()) {
          thread_profile->GetPseudoStack()->mPrivacyMode = true;
        }
      }
  }

#if defined(SPS_OS_android) && !defined(MOZ_WIDGET_GONK)
  if (t->ProfileJava()) {
    int javaInterval = aInterval;
    // Java sampling doesn't accuratly keep up with 1ms sampling
    if (javaInterval < 10) {
      aInterval = 10;
    }
    mozilla::widget::GeckoJavaSampler::StartJavaProfiling(javaInterval, 1000);
  }
#endif

  if (t->AddMainThreadIO()) {
    if (!sInterposeObserver) {
      // Lazily create IO interposer observer
      sInterposeObserver = new mozilla::ProfilerIOInterposeObserver();
    }
    mozilla::IOInterposer::Register(mozilla::IOInterposeObserver::OpAll,
                                    sInterposeObserver);
  }

  sIsProfiling = true;
  sIsGPUProfiling = t->ProfileGPU();
  sIsLayersDump = t->LayersDump();
  sIsDisplayListDump = t->DisplayListDump();
  sIsRestyleProfiling = t->ProfileRestyle();

  if (Sampler::CanNotifyObservers()) {
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
        new nsProfilerStartParams(aProfileEntries, aInterval, featuresArray,
                                  threadNameFiltersArray);

      os->NotifyObservers(params, "profiler-started", nullptr);
    }
  }

  LOG("END   mozilla_sampler_start");
}

void mozilla_sampler_stop()
{
  LOG("BEGIN mozilla_sampler_stop");

  if (!stack_key_initialized)
    return;

  TableTicker *t = tlsTicker.get();
  if (!t) {
    LOG("END   mozilla_sampler_stop-early");
    return;
  }

  bool disableJS = t->ProfileJS();

  t->Stop();
  delete t;
  tlsTicker.set(nullptr);

  if (disableJS) {
    PseudoStack *stack = tlsPseudoStack.get();
    ASSERT(stack != nullptr);
    stack->disableJSSampling();
  }

  mozilla::IOInterposer::Unregister(mozilla::IOInterposeObserver::OpAll,
                                    sInterposeObserver);
  sInterposeObserver = nullptr;

  sIsProfiling = false;
  sIsGPUProfiling = false;
  sIsLayersDump = false;
  sIsDisplayListDump = false;
  sIsRestyleProfiling = false;

  if (Sampler::CanNotifyObservers()) {
    nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
    if (os)
      os->NotifyObservers(nullptr, "profiler-stopped", nullptr);
  }

  LOG("END   mozilla_sampler_stop");
}

bool mozilla_sampler_is_paused() {
  if (Sampler::GetActiveSampler()) {
    return Sampler::GetActiveSampler()->IsPaused();
  } else {
    return false;
  }
}

void mozilla_sampler_pause() {
  if (Sampler::GetActiveSampler()) {
    Sampler::GetActiveSampler()->SetPaused(true);
  }
}

void mozilla_sampler_resume() {
  if (Sampler::GetActiveSampler()) {
    Sampler::GetActiveSampler()->SetPaused(false);
  }
}

bool mozilla_sampler_feature_active(const char* aName)
{
  if (!profiler_is_active()) {
    return false;
  }

  if (strcmp(aName, "gpu") == 0) {
    return sIsGPUProfiling;
  }

  if (strcmp(aName, "layersdump") == 0) {
    return sIsLayersDump;
  }

  if (strcmp(aName, "displaylistdump") == 0) {
    return sIsDisplayListDump;
  }

  if (strcmp(aName, "restyle") == 0) {
    return sIsRestyleProfiling;
  }

  return false;
}

bool mozilla_sampler_is_active()
{
  return sIsProfiling;
}

void mozilla_sampler_responsiveness(const mozilla::TimeStamp& aTime)
{
  sLastTracerEvent = aTime;
}

void mozilla_sampler_frame_number(int frameNumber)
{
  sFrameNumber = frameNumber;
}

void mozilla_sampler_lock()
{
  profiler_stop();
  nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
  if (os)
    os->NotifyObservers(nullptr, "profiler-locked", nullptr);
}

void mozilla_sampler_unlock()
{
  nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
  if (os)
    os->NotifyObservers(nullptr, "profiler-unlocked", nullptr);
}

bool mozilla_sampler_register_thread(const char* aName, void* stackTop)
{
  if (sInitCount == 0) {
    return false;
  }

#if defined(MOZ_WIDGET_GONK) && !defined(MOZ_PROFILING)
  // The only way to profile secondary threads on b2g
  // is to build with profiling OR have the profiler
  // running on startup.
  if (!profiler_is_active()) {
    return false;
  }
#endif

  MOZ_ASSERT(tlsPseudoStack.get() == nullptr);
  PseudoStack* stack = PseudoStack::create();
  tlsPseudoStack.set(stack);
  bool isMainThread = is_main_thread_name(aName);
  return Sampler::RegisterCurrentThread(aName, stack, isMainThread, stackTop);
}

void mozilla_sampler_unregister_thread()
{
  // Don't check sInitCount count here -- we may be unregistering the
  // thread after the sampler was shut down.
  if (!stack_key_initialized) {
    return;
  }

  PseudoStack *stack = tlsPseudoStack.get();
  if (!stack) {
    return;
  }
  stack->deref();
  tlsPseudoStack.set(nullptr);

  Sampler::UnregisterCurrentThread();
}

void mozilla_sampler_sleep_start() {
    if (sInitCount == 0) {
	return;
    }

    PseudoStack *stack = tlsPseudoStack.get();
    if (stack == nullptr) {
      return;
    }
    stack->setSleeping(1);
}

void mozilla_sampler_sleep_end() {
    if (sInitCount == 0) {
	return;
    }

    PseudoStack *stack = tlsPseudoStack.get();
    if (stack == nullptr) {
      return;
    }
    stack->setSleeping(0);
}

double mozilla_sampler_time(const mozilla::TimeStamp& aTime)
{
  if (!mozilla_sampler_is_active()) {
    return 0.0;
  }
  mozilla::TimeDuration delta = aTime - sStartTime;
  return delta.ToMilliseconds();
}

double mozilla_sampler_time()
{
  return mozilla_sampler_time(mozilla::TimeStamp::Now());
}

ProfilerBacktrace* mozilla_sampler_get_backtrace()
{
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

  TableTicker* t = tlsTicker.get();
  if (!t) {
    return nullptr;
  }

  return new ProfilerBacktrace(t->GetBacktrace());
}

void mozilla_sampler_free_backtrace(ProfilerBacktrace* aBacktrace)
{
  delete aBacktrace;
}

void mozilla_sampler_tracing(const char* aCategory, const char* aInfo,
                             TracingMetadata aMetaData)
{
  mozilla_sampler_add_marker(aInfo, new ProfilerMarkerTracing(aCategory, aMetaData));
}

void mozilla_sampler_tracing(const char* aCategory, const char* aInfo,
                             ProfilerBacktrace* aCause,
                             TracingMetadata aMetaData)
{
  mozilla_sampler_add_marker(aInfo, new ProfilerMarkerTracing(aCategory, aMetaData, aCause));
}

void mozilla_sampler_add_marker(const char *aMarker, ProfilerMarkerPayload *aPayload)
{
  // Note that aPayload may be allocated by the caller, so we need to make sure
  // that we free it at some point.
  nsAutoPtr<ProfilerMarkerPayload> payload(aPayload);

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
  mozilla::TimeDuration delta = origin - sStartTime;
  stack->addMarker(aMarker, payload.forget(), static_cast<float>(delta.ToMilliseconds()));
}

// END externally visible functions
////////////////////////////////////////////////////////////////////////


