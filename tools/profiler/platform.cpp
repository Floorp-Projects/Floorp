/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <iostream>
#include <fstream>
#include <sstream>
#include <errno.h>

#include "platform.h"
#include "PlatformMacros.h"
#include "prenv.h"
#include "mozilla/ThreadLocal.h"
#include "PseudoStack.h"
#include "TableTicker.h"
#include "UnwinderThread2.h"
#include "nsIObserverService.h"
#include "nsDirectoryServiceUtils.h"
#include "nsDirectoryServiceDefs.h"
#include "mozilla/Services.h"
#include "nsThreadUtils.h"

#if defined(SPS_OS_android) && !defined(MOZ_WIDGET_GONK)
  #include "AndroidBridge.h"
#endif

mozilla::ThreadLocal<PseudoStack *> tlsPseudoStack;
mozilla::ThreadLocal<TableTicker *> tlsTicker;
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

    bool req2 = PR_GetEnv("MOZ_PROFILER_NEW") != NULL; // Has v2 been requested?

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
    if (PR_GetEnv("MOZ_PROFILER_VERBOSE") != NULL)
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

// Read env vars at startup, so as to set sUnwindMode and sInterval.
void read_profiler_env_vars()
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
  const char* strF = PR_GetEnv("MOZ_PROFILER_STACK_SCAN");

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

  if (strF) {
    errno = 0;
    long int n = strtol(strF, (char**)NULL, 10);
    if (errno == 0 && n >= 0 && n <= 100) {
      sUnwindStackScan = n;
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
       nativeAvail ? "supports" : "does not support");
  LOG( "SPS: ");
  /* Re-set defaults */
  sUnwindMode       = nativeAvail ? UnwCOMBINED : UnwPSEUDO;
  sUnwindInterval   = 0;  /* We'll have to look elsewhere */
  sUnwindStackScan  = 0;

 out:
  LOG( "SPS:");
  LOGF("SPS: Unwind mode       = %s", name_UnwMode(sUnwindMode));
  LOGF("SPS: Sampling interval = %d ms (zero means \"platform default\")",
       (int)sUnwindInterval);
  LOGF("SPS: UnwindStackScan   = %d (max dubious frames per unwind).",
       (int)sUnwindStackScan);
  LOG( "SPS: Use env var MOZ_PROFILER_MODE=help for further information.");
  LOG( "SPS:");

  return;
}

////////////////////////////////////////////////////////////////////////
// BEGIN externally visible functions

void mozilla_sampler_init(void* stackTop)
{
  sInitCount++;

  if (stack_key_initialized)
    return;

  LOG("BEGIN mozilla_sampler_init");
  if (!tlsPseudoStack.init() || !tlsTicker.init()) {
    LOG("Failed to init.");
    return;
  }
  stack_key_initialized = true;

  Sampler::Startup();

  PseudoStack *stack = new PseudoStack();
  tlsPseudoStack.set(stack);

  Sampler::RegisterCurrentThread("Gecko", stack, true, stackTop);

  // Read mode settings from MOZ_PROFILER_MODE and interval
  // settings from MOZ_PROFILER_INTERVAL and stack-scan threshhold
  // from MOZ_PROFILER_STACK_SCAN.
  read_profiler_env_vars();

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
                         , "leaf"
#if defined(XP_WIN) || defined(XP_MACOSX)
                         , "stackwalk"
#endif
                         };
  profiler_start(PROFILE_DEFAULT_ENTRY, PROFILE_DEFAULT_INTERVAL,
                         features, sizeof(features)/sizeof(const char*));
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
    return NULL;
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
    return NULL;
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
    NULL
  };

  return features;
}

// Values are only honored on the first start
void mozilla_sampler_start(int aProfileEntries, int aInterval,
                           const char** aFeatures, uint32_t aFeatureCount)
{
  if (!stack_key_initialized)
    profiler_init(NULL);

  /* If the sampling interval was set using env vars, use that
     in preference to anything else. */
  if (sUnwindInterval > 0)
    aInterval = sUnwindInterval;

  PseudoStack *stack = tlsPseudoStack.get();
  if (!stack) {
    ASSERT(false);
    return;
  }

  // Reset the current state if the profiler is running
  profiler_stop();

  TableTicker* t;
  t = new TableTicker(aInterval ? aInterval : PROFILE_DEFAULT_INTERVAL,
                      aProfileEntries ? aProfileEntries : PROFILE_DEFAULT_ENTRY,
                      aFeatures, aFeatureCount);
  if (t->HasUnwinderThread()) {
    // Create the unwinder thread.  ATM there is only one.
    uwt__init();
  }

  tlsTicker.set(t);
  t->Start();
  if (t->ProfileJS()) {
      mozilla::MutexAutoLock lock(*Sampler::sRegisteredThreadsMutex);
      std::vector<ThreadInfo*> threads = t->GetRegisteredThreads();

      for (uint32_t i = 0; i < threads.size(); i++) {
        ThreadInfo* info = threads[i];
        ThreadProfile* thread_profile = info->Profile();
        if (!thread_profile) {
          continue;
        }
        thread_profile->GetPseudoStack()->enableJSSampling();
      }
  }

#if defined(SPS_OS_android) && !defined(MOZ_WIDGET_GONK)
  if (t->ProfileJava()) {
    int javaInterval = aInterval;
    // Java sampling doesn't accuratly keep up with 1ms sampling
    if (javaInterval < 10) {
      aInterval = 10;
    }
    mozilla::AndroidBridge::Bridge()->StartJavaProfiling(javaInterval, 1000);
  }
#endif

  sIsProfiling = true;

  nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
  if (os)
    os->NotifyObservers(nullptr, "profiler-started", nullptr);
}

void mozilla_sampler_stop()
{
  if (!stack_key_initialized)
    profiler_init(NULL);

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
  tlsTicker.set(NULL);
  PseudoStack *stack = tlsPseudoStack.get();
  ASSERT(stack != NULL);

  if (disableJS)
    stack->disableJSSampling();

  if (unwinderThreader) {
    uwt__deinit();
  }

  sIsProfiling = false;

  nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
  if (os)
    os->NotifyObservers(nullptr, "profiler-stopped", nullptr);
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
  PseudoStack* stack = new PseudoStack();
  tlsPseudoStack.set(stack);

  return Sampler::RegisterCurrentThread(aName, stack, false, stackTop);
}

void mozilla_sampler_unregister_thread()
{
  Sampler::UnregisterCurrentThread();
}

double mozilla_sampler_time()
{
  TimeDuration delta = TimeStamp::Now() - sStartTime;
  return delta.ToMilliseconds();
}

// END externally visible functions
////////////////////////////////////////////////////////////////////////


