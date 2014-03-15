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
#include "mozilla/StaticPtr.h"
#include "mozilla/ThreadLocal.h"
#include "PseudoStack.h"
#include "TableTicker.h"
#include "UnwinderThread2.h"
#include "nsIObserverService.h"
#include "nsDirectoryServiceUtils.h"
#include "nsDirectoryServiceDefs.h"
#include "mozilla/Services.h"
#include "nsThreadUtils.h"
#include "ProfilerMarkers.h"
#include "nsXULAppAPI.h"

#if defined(SPS_OS_android) && !defined(MOZ_WIDGET_GONK)
  #include "AndroidBridge.h"
  using namespace mozilla::widget::android;
#endif

mozilla::ThreadLocal<PseudoStack *> tlsPseudoStack;
mozilla::ThreadLocal<TableTicker *> tlsTicker;
mozilla::ThreadLocal<void *> tlsStackTop;
// We need to track whether we've been initialized otherwise
// we end up using tlsStack without initializing it.
// Because tlsStack is totally opaque to us we can't reuse
// it as the flag itself.
bool stack_key_initialized;

TimeStamp   sLastTracerEvent; // is raced on
TimeStamp   sStartTime;
int         sFrameNumber = 0;
int         sLastFrameNumber = 0;
int         sInitCount = 0; // Each init must have a matched shutdown.
static bool sIsProfiling = false; // is raced on

// env variables to control the profiler
const char* PROFILER_MODE = "MOZ_PROFILER_MODE";
const char* PROFILER_INTERVAL = "MOZ_PROFILER_INTERVAL";
const char* PROFILER_ENTRIES = "MOZ_PROFILER_ENTRIES";
const char* PROFILER_STACK = "MOZ_PROFILER_STACK_SCAN";
const char* PROFILER_FEATURES = "MOZ_PROFILING_FEATURES";

/* used to keep track of the last event that we sampled during */
unsigned int sLastSampledEventGeneration = 0;

/* a counter that's incremented everytime we get responsiveness event
 * note: it might also be worth trackplaing everytime we go around
 * the event loop */
unsigned int sCurrentEventGeneration = 0;
/* we don't need to worry about overflow because we only treat the
 * case of them being the same as special. i.e. we only run into
 * a problem if 2^32 events happen between samples that we need
 * to know are associated with different events */

std::vector<ThreadInfo*>* Sampler::sRegisteredThreads = nullptr;
mozilla::Mutex* Sampler::sRegisteredThreadsMutex = nullptr;

TableTicker* Sampler::sActiveSampler;

static mozilla::StaticAutoPtr<mozilla::ProfilerIOInterposeObserver>
                                                            sInterposeObserver;

// The name that identifies the gecko thread for calls to
// profiler_register_thread. For all platform except metro
// the thread that calls mozilla_sampler_init is considered
// the gecko thread.  With metro the gecko thread is
// registered later based on this thread name.
static const char * gGeckoThreadName = "GeckoMain";

void Sampler::Startup() {
  sRegisteredThreads = new std::vector<ThreadInfo*>();
  sRegisteredThreadsMutex = new mozilla::Mutex("sRegisteredThreads mutex");
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
}

ThreadInfo::~ThreadInfo() {
  free(mName);

  if (mProfile)
    delete mProfile;

  Sampler::FreePlatformData(mPlatformData);
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
ProfilerMarker::SetGeneration(int aGenID) {
  mGenID = aGenID;
}

float
ProfilerMarker::GetTime() {
  return mTime;
}

template<typename Builder> void
ProfilerMarker::BuildJSObject(Builder& b, typename Builder::ArrayHandle markers) const {
  typename Builder::RootedObject marker(b.context(), b.CreateObject());
  b.DefineProperty(marker, "name", GetMarkerName());
  // TODO: Store the callsite for this marker if available:
  // if have location data
  //   b.DefineProperty(marker, "location", ...);
  if (mPayload) {
    typename Builder::RootedObject markerData(b.context(),
                                              mPayload->PreparePayload(b));
    b.DefineProperty(marker, "data", markerData);
  }
  b.DefineProperty(marker, "time", mTime);
  b.ArrayPush(markers, marker);
}

template void
ProfilerMarker::BuildJSObject<JSCustomObjectBuilder>(JSCustomObjectBuilder& b,
                              JSCustomObjectBuilder::ArrayHandle markers) const;
template void
ProfilerMarker::BuildJSObject<JSObjectBuilder>(JSObjectBuilder& b,
                                    JSObjectBuilder::ArrayHandle markers) const;

PendingMarkers::~PendingMarkers() {
  clearMarkers();
  if (mSignalLock != false) {
    // We're releasing the pseudostack while it's still in use.
    // The label macros keep a non ref counted reference to the
    // stack to avoid a TLS. If these are not all cleared we will
    // get a use-after-free so better to crash now.
    abort();
  }
}

void
PendingMarkers::addMarker(ProfilerMarker *aMarker) {
  mSignalLock = true;
  STORE_SEQUENCER();

  MOZ_ASSERT(aMarker);
  mPendingMarkers.insert(aMarker);

  // Clear markers that have been overwritten
  while (mStoredMarkers.peek() &&
         mStoredMarkers.peek()->HasExpired(mGenID)) {
    delete mStoredMarkers.popHead();
  } 
  STORE_SEQUENCER();
  mSignalLock = false;
}

void
PendingMarkers::updateGeneration(int aGenID) {
  mGenID = aGenID;
}

void
PendingMarkers::addStoredMarker(ProfilerMarker *aStoredMarker) {
  aStoredMarker->SetGeneration(mGenID);
  mStoredMarkers.insert(aStoredMarker);
}

bool sps_version2()
{
  static int version = 0; // Raced on, potentially

  if (version == 0) {
    bool allow2 = false; // Is v2 allowable on this platform?
#   if defined(SPS_PLAT_amd64_linux) || defined(SPS_PLAT_arm_android) \
       || defined(SPS_PLAT_x86_linux)
    allow2 = true;
#   elif defined(SPS_PLAT_amd64_darwin) || defined(SPS_PLAT_x86_darwin) \
         || defined(SPS_PLAT_x86_windows) || defined(SPS_PLAT_x86_android) \
         || defined(SPS_PLAT_amd64_windows)
    allow2 = false;
#   else
#     error "Unknown platform"
#   endif

    bool req2 = PR_GetEnv("MOZ_PROFILER_NEW") != nullptr; // Has v2 been requested?

    bool elfhackd = false;
#   if defined(USE_ELF_HACK)
    bool elfhackd = true;
#   endif

    if (req2 && allow2) {
      version = 2;
      LOG("------------------- MOZ_PROFILER_NEW set -------------------");
    } else if (req2 && !allow2) {
      version = 1;
      LOG("--------------- MOZ_PROFILER_NEW requested, ----------------");
      LOG("---------- but is not available on this platform -----------");
    } else if (req2 && elfhackd) {
      version = 1;
      LOG("--------------- MOZ_PROFILER_NEW requested, ----------------");
      LOG("--- but this build was not done with --disable-elf-hack ----");
    } else {
      version = 1;
      LOG("----------------- MOZ_PROFILER_NEW not set -----------------");
    }
  }
  return version == 2;
}

#if !defined(ANDROID)
/* Has MOZ_PROFILER_VERBOSE been set? */
bool moz_profiler_verbose()
{
  /* 0 = not checked, 1 = unset, 2 = set */
  static int status = 0; // Raced on, potentially

  if (status == 0) {
    if (PR_GetEnv("MOZ_PROFILER_VERBOSE") != nullptr)
      status = 2;
    else
      status = 1;
  }

  return status == 2;
}
#endif

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

bool set_profiler_mode(const char* mode) {
  if (mode) {
    if (0 == strcmp(mode, "pseudo")) {
      sUnwindMode = UnwPSEUDO;
      return true;
    }
    else if (0 == strcmp(mode, "native") && is_native_unwinding_avail()) {
      sUnwindMode = UnwNATIVE;
      return true;
    }
    else if (0 == strcmp(mode, "combined") && is_native_unwinding_avail()) {
      sUnwindMode = UnwCOMBINED;
      return true;
    } else {
      return false;
    }
  }

  return true;
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

// Read env vars at startup, so as to set sUnwindMode and sInterval.
void read_profiler_env_vars()
{
  bool nativeAvail = is_native_unwinding_avail();

  /* Set defaults */
  sUnwindMode     = nativeAvail ? UnwCOMBINED : UnwPSEUDO;
  sUnwindInterval = 0;  /* We'll have to look elsewhere */
  sProfileEntries = 0;

  const char* stackMode = PR_GetEnv(PROFILER_MODE);
  const char* interval = PR_GetEnv(PROFILER_INTERVAL);
  const char* entries = PR_GetEnv(PROFILER_ENTRIES);
  const char* scanCount = PR_GetEnv(PROFILER_STACK);

  if (!set_profiler_mode(stackMode) ||
      !set_profiler_interval(interval) ||
      !set_profiler_entries(entries) ||
      !set_profiler_scan(scanCount)) {
      profiler_usage();
  } else {
    LOG( "SPS:");
    LOGF("SPS: Unwind mode       = %s", name_UnwMode(sUnwindMode));
    LOGF("SPS: Sampling interval = %d ms (zero means \"platform default\")",
        (int)sUnwindInterval);
    LOGF("SPS: Entry store size  = %d (zero means \"platform default\")",
        (int)sProfileEntries);
    LOGF("SPS: UnwindStackScan   = %d (max dubious frames per unwind).",
        (int)sUnwindStackScan);
    LOG( "SPS: Use env var MOZ_PROFILER_MODE=help for further information.");
    LOG( "SPS:");
  }
}

void profiler_usage() {
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
  LOG( "SPS:   MOZ_PROFILER_ENTRIES=<number>    (count, minimum of 1)");
  LOG( "SPS:   If unset, platform default is used.");
  LOG( "SPS: ");
  LOG( "SPS:   MOZ_PROFILER_VERBOSE");
  LOG( "SPS:   If set to any value, increases verbosity (recommended).");
  LOG( "SPS: ");
  LOG( "SPS:   MOZ_PROFILER_STACK_SCAN=<number>   (default is zero)");
  LOG( "SPS:   The number of dubious (stack-scanned) frames allowed");
  LOG( "SPS: ");
  LOG( "SPS:   MOZ_PROFILER_NEW");
  LOG( "SPS:   Needs to be set to use Breakpad-based unwinding.");
  LOG( "SPS: ");
  LOGF("SPS:   This platform %s native unwinding.",
       is_native_unwinding_avail() ? "supports" : "does not support");
  LOG( "SPS: ");

  /* Re-set defaults */
  sUnwindMode       = is_native_unwinding_avail() ? UnwCOMBINED : UnwPSEUDO;
  sUnwindInterval   = 0;  /* We'll have to look elsewhere */
  sProfileEntries   = 0;
  sUnwindStackScan  = 0;

  LOG( "SPS:");
  LOGF("SPS: Unwind mode       = %s", name_UnwMode(sUnwindMode));
  LOGF("SPS: Sampling interval = %d ms (zero means \"platform default\")",
       (int)sUnwindInterval);
  LOGF("SPS: Entry store size  = %d (zero means \"platform default\")",
       (int)sProfileEntries);
  LOGF("SPS: UnwindStackScan   = %d (max dubious frames per unwind).",
       (int)sUnwindStackScan);
  LOG( "SPS: Use env var MOZ_PROFILER_MODE=help for further information.");
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

  PseudoStack *stack = new PseudoStack();
  tlsPseudoStack.set(stack);

  bool isMainThread = true;
#ifdef XP_WIN
  // For metrofx, we'll register the main thread once it's created.
  isMainThread = !(XRE_GetWindowsEnvironment() == WindowsEnvironmentType_Metro);
#endif
  Sampler::RegisterCurrentThread(isMainThread ?
                                   gGeckoThreadName : "Application Thread",
                                 stack, isMainThread, stackTop);

  // Read mode settings from MOZ_PROFILER_MODE and interval
  // settings from MOZ_PROFILER_INTERVAL and stack-scan threshhold
  // from MOZ_PROFILER_STACK_SCAN.
  read_profiler_env_vars();

  // platform specific initialization
  OS::Startup();

  // We can't open pref so we use an environment variable
  // to know if we should trigger the profiler on startup
  // NOTE: Default
  const char *val = PR_GetEnv("MOZ_PROFILER_STARTUP");
  if (!val || !*val) {
    return;
  }

  const char* features[] = {"js"
                         , "leaf"
#if defined(XP_WIN) || defined(XP_MACOSX) || (defined(SPS_ARCH_arm) && defined(linux))
                         , "stackwalk"
#endif
#if defined(SPS_OS_android) && !defined(MOZ_WIDGET_GONK)
                         , "java"
#endif
                         };
  profiler_start(PROFILE_DEFAULT_ENTRY, PROFILE_DEFAULT_INTERVAL,
                         features, sizeof(features)/sizeof(const char*),
                         // TODO Add env variable to select threads
                         nullptr, 0);
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

  Sampler::Shutdown();

  // We can't delete the Stack because we can be between a
  // sampler call_enter/call_exit point.
  // TODO Need to find a safe time to delete Stack
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

char* mozilla_sampler_get_profile()
{
  TableTicker *t = tlsTicker.get();
  if (!t) {
    return nullptr;
  }

  std::stringstream stream;
  t->ToStreamAsJSON(stream);
  char* profile = strdup(stream.str().c_str());
  return profile;
}

JSObject *mozilla_sampler_get_profile_data(JSContext *aCx)
{
  TableTicker *t = tlsTicker.get();
  if (!t) {
    return nullptr;
  }

  return t->ToJSObject(aCx);
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
    // Profile the registered secondary threads.
    "threads",
    // Do not include user-identifiable information
    "privacy",
    // Add main thread I/O to the profile
    "mainthreadio",
#if defined(XP_WIN)
    // Add power collection
    "power",
#endif
    nullptr
  };

  return features;
}

// Values are only honored on the first start
void mozilla_sampler_start(int aProfileEntries, double aInterval,
                           const char** aFeatures, uint32_t aFeatureCount,
                           const char** aThreadNameFilters, uint32_t aFilterCount)

{
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
  if (t->HasUnwinderThread()) {
    // Create the unwinder thread.  ATM there is only one.
    uwt__init();
  }

  tlsTicker.set(t);
  t->Start();
  if (t->ProfileJS() || t->InPrivacyMode()) {
      mozilla::MutexAutoLock lock(*Sampler::sRegisteredThreadsMutex);
      std::vector<ThreadInfo*> threads = t->GetRegisteredThreads();

      for (uint32_t i = 0; i < threads.size(); i++) {
        ThreadInfo* info = threads[i];
        ThreadProfile* thread_profile = info->Profile();
        if (!thread_profile) {
          continue;
        }
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
    GeckoJavaSampler::StartJavaProfiling(javaInterval, 1000);
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

  nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
  if (os)
    os->NotifyObservers(nullptr, "profiler-started", nullptr);
}

void mozilla_sampler_stop()
{
  if (!stack_key_initialized)
    profiler_init(nullptr);

  TableTicker *t = tlsTicker.get();
  if (!t) {
    return;
  }

  bool disableJS = t->ProfileJS();
  bool unwinderThreader = t->HasUnwinderThread();

  // Shut down and reap the unwinder thread.  We have to do this
  // before stopping the sampler, so as to guarantee that the unwinder
  // thread doesn't try to access memory that the subsequent call to
  // mozilla_sampler_stop causes to be freed.
  if (unwinderThreader) {
    uwt__stop();
  }

  t->Stop();
  delete t;
  tlsTicker.set(nullptr);

  if (disableJS) {
    PseudoStack *stack = tlsPseudoStack.get();
    ASSERT(stack != nullptr);
    stack->disableJSSampling();
  }

  if (unwinderThreader) {
    uwt__deinit();
  }

  mozilla::IOInterposer::Unregister(mozilla::IOInterposeObserver::OpAll,
                                    sInterposeObserver);
  sInterposeObserver = nullptr;

  sIsProfiling = false;

  nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
  if (os)
    os->NotifyObservers(nullptr, "profiler-stopped", nullptr);
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

bool mozilla_sampler_is_active()
{
  return sIsProfiling;
}

static double sResponsivenessTimes[100];
static unsigned int sResponsivenessLoc = 0;
void mozilla_sampler_responsiveness(const TimeStamp& aTime)
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

const double* mozilla_sampler_get_responsiveness()
{
  return sResponsivenessTimes;
}

void mozilla_sampler_frame_number(int frameNumber)
{
  sFrameNumber = frameNumber;
}

void mozilla_sampler_print_location2()
{
  // FIXME
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
#if defined(MOZ_WIDGET_GONK) && !defined(MOZ_PROFILING)
  // The only way to profile secondary threads on b2g
  // is to build with profiling OR have the profiler
  // running on startup.
  if (!profiler_is_active()) {
    return false;
  }
#endif

  PseudoStack* stack = new PseudoStack();
  tlsPseudoStack.set(stack);
  bool isMainThread = is_main_thread_name(aName);
  return Sampler::RegisterCurrentThread(aName, stack, isMainThread, stackTop);
}

void mozilla_sampler_unregister_thread()
{
  Sampler::UnregisterCurrentThread();

  PseudoStack *stack = tlsPseudoStack.get();
  if (!stack) {
    return;
  }
  delete stack;
  tlsPseudoStack.set(nullptr);
}

double mozilla_sampler_time(const TimeStamp& aTime)
{
  if (!mozilla_sampler_is_active()) {
    return 0.0;
  }
  TimeDuration delta = aTime - sStartTime;
  return delta.ToMilliseconds();
}

double mozilla_sampler_time()
{
  return mozilla_sampler_time(TimeStamp::Now());
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
  TimeDuration delta = TimeStamp::Now() - sStartTime;
  stack->addMarker(aMarker, payload.forget(), static_cast<float>(delta.ToMilliseconds()));
}

// END externally visible functions
////////////////////////////////////////////////////////////////////////


