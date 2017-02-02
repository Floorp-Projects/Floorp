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
#include "mozilla/UniquePtr.h"
#include "GeckoProfiler.h"
#include "ProfilerIOInterposeObserver.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/ThreadLocal.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/Sprintf.h"
#include "PseudoStack.h"
#include "nsIObserverService.h"
#include "nsDirectoryServiceUtils.h"
#include "nsDirectoryServiceDefs.h"
#include "nsXULAppAPI.h"
#include "nsProfilerStartParams.h"
#include "mozilla/Services.h"
#include "nsThreadUtils.h"
#include "ProfilerMarkers.h"

#ifdef MOZ_TASK_TRACER
#include "GeckoTaskTracer.h"
#endif

#if defined(SPS_OS_android) && !defined(MOZ_WIDGET_GONK)
  #include "FennecJNIWrappers.h"
#endif

#if defined(SPS_OS_android) && !defined(MOZ_WIDGET_GONK)
#include "FennecJNINatives.h"
#endif

#if defined(SPS_PLAT_amd64_linux) || defined(SPS_PLAT_x86_linux)
# define USE_LUL_STACKWALK
# include "lul/LulMain.h"
# include "lul/platform-linux-lul.h"
#endif

#if defined(SPS_OS_android) && !defined(MOZ_WIDGET_GONK)
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

MOZ_THREAD_LOCAL(PseudoStack *) tlsPseudoStack;
Sampler* gSampler;

// We need to track whether we've been initialized otherwise
// we end up using tlsStack without initializing it.
// Because tlsStack is totally opaque to us we can't reuse
// it as the flag itself.
bool stack_key_initialized;

static mozilla::TimeStamp   sLastTracerEvent; // is raced on
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

static mozilla::StaticAutoPtr<mozilla::ProfilerIOInterposeObserver>
                                                            sInterposeObserver;

// The name that identifies the gecko thread for calls to
// profiler_register_thread.
static const char * gGeckoThreadName = "GeckoMain";

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
// what profiler_set_verbosity is for.

enum class ProfilerVerbosity : int8_t { UNCHECKED, NOTVERBOSE, VERBOSE };

// Raced on, potentially
static ProfilerVerbosity profiler_verbosity = ProfilerVerbosity::UNCHECKED;

bool profiler_verbose()
{
  if (profiler_verbosity == ProfilerVerbosity::UNCHECKED) {
    if (getenv("MOZ_PROFILER_VERBOSE") != nullptr)
      profiler_verbosity = ProfilerVerbosity::VERBOSE;
    else
      profiler_verbosity = ProfilerVerbosity::NOTVERBOSE;
  }

  return profiler_verbosity == ProfilerVerbosity::VERBOSE;
}

void profiler_set_verbosity(ProfilerVerbosity pv)
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

  const char* interval = getenv(PROFILER_INTERVAL);
  const char* entries = getenv(PROFILER_ENTRIES);
  const char* scanCount = getenv(PROFILER_STACK);

  if (getenv(PROFILER_HELP)) {
     // Enable verbose output
     profiler_set_verbosity(ProfilerVerbosity::VERBOSE);
     profiler_usage();
     // Now force the next enquiry of profiler_verbose to re-query
     // env var MOZ_PROFILER_VERBOSE.
     profiler_set_verbosity(ProfilerVerbosity::UNCHECKED);
  }

  if (!set_profiler_interval(interval) ||
      !set_profiler_entries(entries) ||
      !set_profiler_scan(scanCount)) {
      profiler_usage();
  } else {
    LOG( "Profiler:");
    LOGF("Profiler: Sampling interval = %d ms (zero means \"platform default\")",
        (int)sUnwindInterval);
    LOGF("Profiler: Entry store size  = %d (zero means \"platform default\")",
        (int)sProfileEntries);
    LOGF("Profiler: UnwindStackScan   = %d (max dubious frames per unwind).",
        (int)sUnwindStackScan);
    LOG( "Profiler:");
  }
}

void profiler_usage() {
  LOG( "Profiler: ");
  LOG( "Profiler: Environment variable usage:");
  LOG( "Profiler: ");
  LOG( "Profiler:   MOZ_PROFILER_HELP");
  LOG( "Profiler:   If set to any value, prints this message.");
  LOG( "Profiler: ");
  LOG( "Profiler:   MOZ_PROFILER_INTERVAL=<number>   (milliseconds, 1 to 1000)");
  LOG( "Profiler:   If unset, platform default is used.");
  LOG( "Profiler: ");
  LOG( "Profiler:   MOZ_PROFILER_ENTRIES=<number>    (count, minimum of 1)");
  LOG( "Profiler:   If unset, platform default is used.");
  LOG( "Profiler: ");
  LOG( "Profiler:   MOZ_PROFILER_VERBOSE");
  LOG( "Profiler:   If set to any value, increases verbosity (recommended).");
  LOG( "Profiler: ");
  LOG( "Profiler:   MOZ_PROFILER_STACK_SCAN=<number>   (default is zero)");
  LOG( "Profiler:   The number of dubious (stack-scanned) frames allowed");
  LOG( "Profiler: ");
  LOG( "Profiler:   MOZ_PROFILER_LUL_TEST");
  LOG( "Profiler:   If set to any value, runs LUL unit tests at startup of");
  LOG( "Profiler:   the unwinder thread, and prints a short summary of results.");
  LOG( "Profiler: ");
  LOGF("Profiler:   This platform %s native unwinding.",
       is_native_unwinding_avail() ? "supports" : "does not support");
  LOG( "Profiler: ");

  /* Re-set defaults */
  sUnwindInterval   = 0;  /* We'll have to look elsewhere */
  sProfileEntries   = 0;
  sUnwindStackScan  = 0;

  LOG( "Profiler:");
  LOGF("Profiler: Sampling interval = %d ms (zero means \"platform default\")",
       (int)sUnwindInterval);
  LOGF("Profiler: Entry store size  = %d (zero means \"platform default\")",
       (int)sProfileEntries);
  LOGF("Profiler: UnwindStackScan   = %d (max dubious frames per unwind).",
       (int)sUnwindStackScan);
  LOG( "Profiler:");

  return;
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
profiler_log(const char* str)
{
  profiler_tracing("log", str, TRACING_EVENT);
}

void
profiler_log(const char* fmt, va_list args)
{
  if (profiler_is_active()) {
    // nsAutoCString AppendPrintf would be nicer but
    // this is mozilla external code
    char buf[2048];
    va_list argsCpy;
    VARARGS_ASSIGN(argsCpy, args);
    int required = VsprintfLiteral(buf, fmt, argsCpy);
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

void
profiler_init(void* stackTop)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  sInitCount++;

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
  sStartTime = mozilla::TimeStamp::ProcessCreation(ignore);

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

  set_stderr_callback(profiler_log);

#if defined(SPS_OS_android) && !defined(MOZ_WIDGET_GONK)
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
  LOG("END   profiler_init");
}

void
profiler_shutdown()
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  sInitCount--;

  if (sInitCount > 0)
    return;

  // Save the profile on shutdown if requested.
  if (gSampler) {
    const char *val = getenv("MOZ_PROFILER_SHUTDOWN");
    if (val) {
      std::ofstream stream;
      stream.open(val);
      if (stream.is_open()) {
        gSampler->ToStreamAsJSON(stream);
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

  return gSampler->ToJSON(aSinceTime);
}

JSObject*
profiler_get_profile_jsobject(JSContext *aCx, double aSinceTime)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  if (!gSampler) {
    return nullptr;
  }

  return gSampler->ToJSObject(aCx, aSinceTime);
}

void
profiler_get_profile_jsobject_async(double aSinceTime,
                                    mozilla::dom::Promise* aPromise)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  if (NS_WARN_IF(!gSampler)) {
    return;
  }

  gSampler->ToJSObjectAsync(aSinceTime, aPromise);
}

void
profiler_save_profile_to_file_async(double aSinceTime, const char* aFileName)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  nsCString filename(aFileName);
  NS_DispatchToMainThread(NS_NewRunnableFunction([=] () {
    if (NS_WARN_IF(!gSampler)) {
      return;
    }

    gSampler->ToFileAsync(filename, aSinceTime);
  }));
}

void
profiler_get_start_params(int* aEntrySize,
                          double* aInterval,
                          mozilla::Vector<const char*>* aFilters,
                          mozilla::Vector<const char*>* aFeatures)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  if (NS_WARN_IF(!aEntrySize) || NS_WARN_IF(!aInterval) ||
      NS_WARN_IF(!aFilters) || NS_WARN_IF(!aFeatures)) {
    return;
  }

  if (NS_WARN_IF(!gSampler)) {
    return;
  }

  *aEntrySize = gSampler->EntrySize();
  *aInterval = gSampler->interval();

  const ThreadNameFilterList& threadNameFilterList =
    gSampler->ThreadNameFilters();
  MOZ_ALWAYS_TRUE(aFilters->resize(threadNameFilterList.length()));
  for (uint32_t i = 0; i < threadNameFilterList.length(); ++i) {
    (*aFilters)[i] = threadNameFilterList[i].c_str();
  }

  const FeatureList& featureList = gSampler->Features();
  MOZ_ALWAYS_TRUE(aFeatures->resize(featureList.length()));
  for (size_t i = 0; i < featureList.length(); ++i) {
    (*aFeatures)[i] = featureList[i].c_str();
  }
}

void
profiler_get_gatherer(nsISupports** aRetVal)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  if (!aRetVal) {
    return;
  }

  if (NS_WARN_IF(!profiler_is_active())) {
    *aRetVal = nullptr;
    return;
  }

  if (NS_WARN_IF(!gSampler)) {
    *aRetVal = nullptr;
    return;
  }

  gSampler->GetGatherer(aRetVal);
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
    gSampler->ToStreamAsJSON(stream);
    stream.close();
    LOGF("Saved to %s", aFilename);
  } else {
    LOG("Fail to open profile log file.");
  }
}

const char**
profiler_get_features()
{
  static const char* features[] = {
#if defined(MOZ_PROFILING) && defined(HAVE_NATIVE_UNWIND)
    // Walk the C++ stack.
    "stackwalk",
#endif
#if defined(ENABLE_LEAF_DATA)
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
    nullptr
  };

  return features;
}

void
profiler_get_buffer_info_helper(uint32_t *aCurrentPosition,
                                uint32_t *aTotalSize,
                                uint32_t *aGeneration)
{
  // This function is called by profiler_get_buffer_info(), which has already
  // zeroed the outparams.

  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  if (!stack_key_initialized)
    return;

  if (!gSampler) {
    return;
  }

  gSampler->GetBufferInfo(aCurrentPosition, aTotalSize, aGeneration);
}

// Values are only honored on the first start
void
profiler_start(int aProfileEntries, double aInterval,
               const char** aFeatures, uint32_t aFeatureCount,
               const char** aThreadNameFilters, uint32_t aFilterCount)

{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  LOG("BEGIN profiler_start");

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

  gSampler =
    new Sampler(aInterval ? aInterval : PROFILE_DEFAULT_INTERVAL,
                aProfileEntries ? aProfileEntries : PROFILE_DEFAULT_ENTRY,
                aFeatures, aFeatureCount, aThreadNameFilters, aFilterCount);

  gSampler->Start();
  if (gSampler->ProfileJS() || gSampler->InPrivacyMode()) {
    mozilla::MutexAutoLock lock(*Sampler::sRegisteredThreadsMutex);
    const std::vector<ThreadInfo*>& threads = gSampler->GetRegisteredThreads();

    for (uint32_t i = 0; i < threads.size(); i++) {
      ThreadInfo* info = threads[i];
      if (info->IsPendingDelete() || !info->hasProfile()) {
        continue;
      }
      info->Stack()->reinitializeOnResume();
      if (gSampler->ProfileJS()) {
        info->Stack()->enableJSSampling();
      }
      if (gSampler->InPrivacyMode()) {
        info->Stack()->mPrivacyMode = true;
      }
    }
  }

#if defined(SPS_OS_android) && !defined(MOZ_WIDGET_GONK)
  if (gSampler->ProfileJava()) {
    int javaInterval = aInterval;
    // Java sampling doesn't accuratly keep up with 1ms sampling
    if (javaInterval < 10) {
      aInterval = 10;
    }
    java::GeckoJavaSampler::Start(javaInterval, 1000);
  }
#endif

  if (gSampler->AddMainThreadIO()) {
    if (!sInterposeObserver) {
      // Lazily create IO interposer observer
      sInterposeObserver = new mozilla::ProfilerIOInterposeObserver();
    }
    mozilla::IOInterposer::Register(mozilla::IOInterposeObserver::OpAll,
                                    sInterposeObserver);
  }

  sIsProfiling = true;
  sIsGPUProfiling = gSampler->ProfileGPU();
  sIsLayersDump = gSampler->LayersDump();
  sIsDisplayListDump = gSampler->DisplayListDump();
  sIsRestyleProfiling = gSampler->ProfileRestyle();

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

  bool disableJS = gSampler->ProfileJS();

  gSampler->Stop();
  delete gSampler;
  gSampler = nullptr;

  if (disableJS) {
    PseudoStack *stack = tlsPseudoStack.get();
    MOZ_ASSERT(stack != nullptr);
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

  LOG("END   profiler_stop");
}

bool
profiler_is_paused()
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  if (!gSampler) {
    return false;
  }

  return gSampler->IsPaused();
}

void
profiler_pause()
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  if (!gSampler) {
    return;
  }

  gSampler->SetPaused(true);
  if (Sampler::CanNotifyObservers()) {
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

  gSampler->SetPaused(false);
  if (Sampler::CanNotifyObservers()) {
    nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
    if (os) {
      os->NotifyObservers(nullptr, "profiler-resumed", nullptr);
    }
  }
}

bool
profiler_feature_active(const char* aName)
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

bool
profiler_is_active()
{
  return sIsProfiling;
}

void
profiler_responsiveness(const mozilla::TimeStamp& aTime)
{
  sLastTracerEvent = aTime;
}

void
profiler_set_frame_number(int frameNumber)
{
  sFrameNumber = frameNumber;
}

void
profiler_lock()
{
  profiler_stop();
  nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
  if (os)
    os->NotifyObservers(nullptr, "profiler-locked", nullptr);
}

void
profiler_unlock()
{
  nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
  if (os)
    os->NotifyObservers(nullptr, "profiler-unlocked", nullptr);
}

void
profiler_register_thread(const char* aName, void* aGuessStackTop)
{
  if (sInitCount == 0) {
    return;
  }

#if defined(MOZ_WIDGET_GONK) && !defined(MOZ_PROFILING)
  // The only way to profile secondary threads on b2g
  // is to build with profiling OR have the profiler
  // running on startup.
  if (!profiler_is_active()) {
    return;
  }
#endif

  MOZ_ASSERT(tlsPseudoStack.get() == nullptr);
  PseudoStack* stack = PseudoStack::create();
  tlsPseudoStack.set(stack);
  bool isMainThread = is_main_thread_name(aName);
  void* stackTop = GetStackTop(aGuessStackTop);
  Sampler::RegisterCurrentThread(aName, stack, isMainThread, stackTop);
}

void
profiler_unregister_thread()
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

void
profiler_sleep_start()
{
  if (sInitCount == 0) {
    return;
  }

  PseudoStack *stack = tlsPseudoStack.get();
  if (stack == nullptr) {
    return;
  }
  stack->setSleeping(1);
}

void
profiler_sleep_end()
{
  if (sInitCount == 0) {
    return;
  }

  PseudoStack *stack = tlsPseudoStack.get();
  if (stack == nullptr) {
    return;
  }
  stack->setSleeping(0);
}

bool
profiler_is_sleeping()
{
  if (sInitCount == 0) {
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
  PseudoStack *stack = tlsPseudoStack.get();
  if (!stack) {
    return;
  }

  stack->jsOperationCallback();
}

double
profiler_time(const mozilla::TimeStamp& aTime)
{
  mozilla::TimeDuration delta = aTime - sStartTime;
  return delta.ToMilliseconds();
}

double
profiler_time()
{
  return profiler_time(mozilla::TimeStamp::Now());
}

bool
profiler_in_privacy_mode()
{
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

  return UniqueProfilerBacktrace(
    new ProfilerBacktrace(gSampler->GetBacktrace()));
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
  MOZ_ASSERT(outputSize >= 2);
  char *bound = output + outputSize - 2;
  output[0] = output[1] = '\0';
  PseudoStack *pseudoStack = tlsPseudoStack.get();
  if (!pseudoStack) {
    return;
  }

  volatile StackEntry *pseudoFrames = pseudoStack->mStack;
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
  mozilla::TimeDuration delta = origin - sStartTime;
  stack->addMarker(aMarker, payload.release(), delta.ToMilliseconds());
}

// END externally visible functions
////////////////////////////////////////////////////////////////////////
