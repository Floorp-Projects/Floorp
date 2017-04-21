/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * A watchdog designed to terminate shutdown if it lasts too long.
 *
 * This watchdog is designed as a worst-case problem container for the
 * common case in which Firefox just won't shutdown.
 *
 * We spawn a thread during quit-application. If any of the shutdown
 * steps takes more than n milliseconds (63000 by default), kill the
 * process as fast as possible, without any cleanup.
 */

#include "nsTerminator.h"

#include "prthread.h"
#include "prmon.h"
#include "plstr.h"
#include "prio.h"

#include "nsString.h"
#include "nsServiceManagerUtils.h"
#include "nsDirectoryServiceUtils.h"
#include "nsAppDirectoryServiceDefs.h"

#include "nsIObserverService.h"
#include "nsIPrefService.h"
#if defined(MOZ_CRASHREPORTER)
#include "nsExceptionHandler.h"
#endif
#include "GeckoProfiler.h"
#include "nsThreadUtils.h"

#if defined(XP_WIN)
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "mozilla/ArrayUtils.h"
#include "mozilla/Attributes.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/MemoryChecking.h"
#include "mozilla/Preferences.h"
#include "mozilla/Services.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Unused.h"
#include "mozilla/Telemetry.h"

// Normally, the number of milliseconds that AsyncShutdown waits until
// it decides to crash is specified as a preference. We use the
// following value as a fallback if for some reason the preference is
// absent.
#define FALLBACK_ASYNCSHUTDOWN_CRASH_AFTER_MS 60000

// Additional number of milliseconds to wait until we decide to exit
// forcefully.
#define ADDITIONAL_WAIT_BEFORE_CRASH_MS 3000

namespace mozilla {

namespace {

// Utility function: create a thread that is non-joinable,
// does not prevent the process from terminating, is never
// cooperatively scheduled, and uses a default stack size.
PRThread* CreateSystemThread(void (*start)(void* arg),
                             void* arg)
{
  PRThread* thread = PR_CreateThread(
    PR_SYSTEM_THREAD, /* This thread will not prevent the process from terminating */
    start,
    arg,
    PR_PRIORITY_LOW,
    PR_GLOBAL_THREAD /* Make sure that the thread is never cooperatively scheduled */,
    PR_UNJOINABLE_THREAD,
    0 /* Use default stack size */
  );
  MOZ_LSAN_INTENTIONALLY_LEAK_OBJECT(thread); // This pointer will never be deallocated.
  return thread;
}


////////////////////////////////////////////
//
// The watchdog
//
// This nspr thread is in charge of crashing the process if any stage of shutdown
// lasts more than some predefined duration. As a side-effect, it measures the
// duration of each stage of shutdown.
//

// The heartbeat of the operation.
//
// Main thread:
//
// * Whenever a shutdown step has been completed, the main thread
// swaps gHeartbeat to 0 to mark that the shutdown process is still
// progressing. The value swapped away indicates the number of ticks
// it took for the shutdown step to advance.
//
// Watchdog thread:
//
// * Every tick, the watchdog thread increments gHearbeat atomically.
//
// A note about precision:
// Since gHeartbeat is generally reset to 0 between two ticks, this means
// that gHeartbeat stays at 0 less than one tick. Consequently, values
// extracted from gHeartbeat must be considered rounded up.
Atomic<uint32_t> gHeartbeat(0);

struct Options {
  /**
   * How many ticks before we should crash the process.
   */
  uint32_t crashAfterTicks;
};

/**
 * Entry point for the watchdog thread
 */
void
RunWatchdog(void* arg)
{
  NS_SetCurrentThreadName("Shutdown Hang Terminator");

  // Let's copy and deallocate options, that's one less leak to worry
  // about.
  UniquePtr<Options> options((Options*)arg);
  uint32_t crashAfterTicks = options->crashAfterTicks;
  options = nullptr;

  const uint32_t timeToLive = crashAfterTicks;
  while (true) {
    //
    // We do not want to sleep for the entire duration,
    // as putting the computer to sleep would suddenly
    // cause us to timeout on wakeup.
    //
    // Rather, we prefer sleeping for at most 1 second
    // at a time. If the computer sleeps then wakes up,
    // we have lost at most one second, which is much
    // more reasonable.
    //
#if defined(XP_WIN)
    Sleep(1000 /* ms */);
#else
    usleep(1000000 /* usec */);
#endif

    if (gHeartbeat++ < timeToLive) {
      continue;
    }

    // Shutdown is apparently dead. Crash the process.
    MOZ_CRASH("Shutdown too long, probably frozen, causing a crash.");
  }
}

////////////////////////////////////////////
//
// Writer thread
//
// This nspr thread is in charge of writing to disk statistics produced by the
// watchdog thread and collected by the main thread. Note that we use a nspr
// thread rather than usual XPCOM I/O simply because we outlive XPCOM and its
// threads.
//

// Utility class, used by UniquePtr<> to close nspr files.
class PR_CloseDelete
{
public:
  constexpr PR_CloseDelete() = default;

  PR_CloseDelete(const PR_CloseDelete& aOther) = default;

  void operator()(PRFileDesc* aPtr) const
  {
    PR_Close(aPtr);
  }
};

//
// Communication between the main thread and the writer thread.
//
// Main thread:
//
// * Whenever a shutdown step has been completed, the main thread
// obtains the number of ticks from the watchdog threads, builds
// a string representing all the data gathered so far, places
// this string in `gWriteData`, and wakes up the writer thread
// using `gWriteReady`. If `gWriteData` already contained a non-null
// pointer, this means that the writer thread is lagging behind the
// main thread, and the main thread cleans up the memory.
//
// Writer thread:
//
// * When awake, the writer thread swaps `gWriteData` to nullptr. If
// `gWriteData` contained data to write, the . If so, the writer
// thread writes the data to a file named "ShutdownDuration.json.tmp",
// then moves that file to "ShutdownDuration.json" and cleans up the
// data. If `gWriteData` contains a nullptr, the writer goes to sleep
// until it is awkened using `gWriteReady`.
//
//
// The data written by the writer thread will be read by another
// module upon the next restart and fed to Telemetry.
//
Atomic<nsCString*> gWriteData(nullptr);
PRMonitor* gWriteReady = nullptr;

void RunWriter(void* arg)
{
  AutoProfilerRegister registerThread("Shutdown Statistics Writer");
  NS_SetCurrentThreadName("Shutdown Statistics Writer");

  MOZ_LSAN_INTENTIONALLY_LEAK_OBJECT(arg);
  // Shutdown will generally complete before we have a chance to
  // deallocate. This is not a leak.

  // Setup destinationPath and tmpFilePath

  nsCString destinationPath(static_cast<char*>(arg));
  nsAutoCString tmpFilePath;
  tmpFilePath.Append(destinationPath);
  tmpFilePath.AppendLiteral(".tmp");

  // Cleanup any file leftover from a previous run
  Unused << PR_Delete(tmpFilePath.get());
  Unused << PR_Delete(destinationPath.get());

  while (true) {
    //
    // Check whether we have received data from the main thread.
    //
    // We perform the check before waiting on `gWriteReady` as we may
    // have received data while we were busy writing.
    //
    // Also note that gWriteData may have been modified several times
    // since we last checked. That's ok, we are not losing any important
    // data (since we keep adding data), and we are not leaking memory
    // (since the main thread deallocates any data that hasn't been
    // consumed by the writer thread).
    //
    UniquePtr<nsCString> data(gWriteData.exchange(nullptr));
    if (!data) {
      // Data is not available yet.
      // Wait until the main thread provides it.
      PR_EnterMonitor(gWriteReady);
      PR_Wait(gWriteReady, PR_INTERVAL_NO_TIMEOUT);
      PR_ExitMonitor(gWriteReady);
      continue;
    }

    MOZ_LSAN_INTENTIONALLY_LEAK_OBJECT(data.get());
    // Shutdown may complete before we have a chance to deallocate.
    // This is not a leak.

    //
    // Write to a temporary file
    //
    // In case of any error, we simply give up. Since the data is
    // hardly critical, we don't want to spend too much effort
    // salvaging it.
    //
    UniquePtr<PRFileDesc, PR_CloseDelete>
      tmpFileDesc(PR_Open(tmpFilePath.get(),
                          PR_WRONLY | PR_TRUNCATE | PR_CREATE_FILE,
                          00600));

    // Shutdown may complete before we have a chance to close the file.
    // This is not a leak.
    MOZ_LSAN_INTENTIONALLY_LEAK_OBJECT(tmpFileDesc.get());

    if (tmpFileDesc == nullptr) {
      break;
    }
    if (PR_Write(tmpFileDesc.get(), data->get(), data->Length()) == -1) {
      break;
    }
    tmpFileDesc.reset();

    //
    // Rename on top of destination file.
    //
    // This is not sufficient to guarantee that the destination file
    // will be written correctly, but, again, we don't care enough
    // about the data to make more efforts.
    //
    if (PR_Rename(tmpFilePath.get(), destinationPath.get()) != PR_SUCCESS) {
      break;
    }
  }
}

/**
 * A step during shutdown.
 *
 * Shutdown is divided in steps, which all map to an observer
 * notification. The duration of a step is defined as the number of
 * ticks between the time we receive a notification and the next one.
 */
struct ShutdownStep
{
  char const* const mTopic;
  int mTicks;

  constexpr explicit ShutdownStep(const char *const topic)
    : mTopic(topic)
    , mTicks(-1)
  {}

};

static ShutdownStep sShutdownSteps[] = {
  ShutdownStep("quit-application"),
  ShutdownStep("profile-change-teardown"),
  ShutdownStep("profile-before-change"),
  ShutdownStep("xpcom-will-shutdown"),
  ShutdownStep("xpcom-shutdown"),
};

} // namespace

NS_IMPL_ISUPPORTS(nsTerminator, nsIObserver)

nsTerminator::nsTerminator()
  : mInitialized(false)
  , mCurrentStep(-1)
{
}

// During startup, register as an observer for all interesting topics.
nsresult
nsTerminator::SelfInit()
{
  nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
  if (!os) {
    return NS_ERROR_UNEXPECTED;
  }

  for (auto& shutdownStep : sShutdownSteps) {
    DebugOnly<nsresult> rv = os->AddObserver(this, shutdownStep.mTopic, false);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "AddObserver failed");
  }

  return NS_OK;
}

// Actually launch these threads. This takes place at the first sign of shutdown.
void
nsTerminator::Start()
{
  MOZ_ASSERT(!mInitialized);
  StartWatchdog();
#if !defined(DEBUG)
  // Only allow nsTerminator to write on non-debug builds so we don't get leak warnings on
  // shutdown for intentional leaks (see bug 1242084). This will be enabled again by bug
  // 1255484 when 1255478 lands.
  StartWriter();
#endif // !defined(DEBUG)
  mInitialized = true;
}

// Prepare, allocate and start the watchdog thread.
// By design, it will never finish, nor be deallocated.
void
nsTerminator::StartWatchdog()
{
  int32_t crashAfterMS =
    Preferences::GetInt("toolkit.asyncshutdown.crash_timeout",
                        FALLBACK_ASYNCSHUTDOWN_CRASH_AFTER_MS);
  // Ignore negative values
  if (crashAfterMS <= 0) {
    crashAfterMS = FALLBACK_ASYNCSHUTDOWN_CRASH_AFTER_MS;
  }

  // Add a little padding, to ensure that we do not crash before
  // AsyncShutdown.
  if (crashAfterMS > INT32_MAX - ADDITIONAL_WAIT_BEFORE_CRASH_MS) {
    // Defend against overflow
    crashAfterMS = INT32_MAX;
  } else {
    crashAfterMS += ADDITIONAL_WAIT_BEFORE_CRASH_MS;
  }

  UniquePtr<Options> options(new Options());
  const PRIntervalTime ticksDuration = PR_MillisecondsToInterval(1000);
  options->crashAfterTicks = crashAfterMS / ticksDuration;

  DebugOnly<PRThread*> watchdogThread = CreateSystemThread(RunWatchdog,
                                                options.release());
  MOZ_ASSERT(watchdogThread);
}

// Prepare, allocate and start the writer thread. By design, it will never
// finish, nor be deallocated. In case of error, we degrade
// gracefully to not writing Telemetry data.
void
nsTerminator::StartWriter()
{
  if (!Telemetry::CanRecordExtended()) {
    return;
  }
  nsCOMPtr<nsIFile> profLD;
  nsresult rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_LOCAL_50_DIR,
                                       getter_AddRefs(profLD));
  if (NS_FAILED(rv)) {
    return;
  }

  rv = profLD->Append(NS_LITERAL_STRING("ShutdownDuration.json"));
  if (NS_FAILED(rv)) {
    return;
  }

  nsAutoString path;
  rv = profLD->GetPath(path);
  if (NS_FAILED(rv)) {
    return;
  }

  gWriteReady = PR_NewMonitor();
  MOZ_LSAN_INTENTIONALLY_LEAK_OBJECT(gWriteReady); // We will never deallocate this object
  PRThread* writerThread = CreateSystemThread(RunWriter,
                                              ToNewUTF8String(path));

  if (!writerThread) {
    return;
  }
}

NS_IMETHODIMP
nsTerminator::Observe(nsISupports *, const char *aTopic, const char16_t *)
{
  if (strcmp(aTopic, "profile-after-change") == 0) {
    return SelfInit();
  }

  // Other notifications are shutdown-related.

  // As we have seen examples in the wild of shutdown notifications
  // not being sent (or not being sent in the expected order), we do
  // not assume a specific order.
  if (!mInitialized) {
    Start();
  }

  UpdateHeartbeat(aTopic);
#if !defined(DEBUG)
  // Only allow nsTerminator to write on non-debug builds so we don't get leak warnings on
  // shutdown for intentional leaks (see bug 1242084). This will be enabled again by bug
  // 1255484 when 1255478 lands.
  UpdateTelemetry();
#endif // !defined(DEBUG)
  UpdateCrashReport(aTopic);

  // Perform a little cleanup
  nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
  MOZ_RELEASE_ASSERT(os);
  (void)os->RemoveObserver(this, aTopic);

  return NS_OK;
}

void
nsTerminator::UpdateHeartbeat(const char* aTopic)
{
  // Reset the clock, find out how long the current phase has lasted.
  uint32_t ticks = gHeartbeat.exchange(0);
  if (mCurrentStep > 0) {
    sShutdownSteps[mCurrentStep].mTicks = ticks;
  }

  // Find out where we now are in the current shutdown.
  // Don't assume that shutdown takes place in the expected order.
  int nextStep = -1;
  for (size_t i = 0; i < ArrayLength(sShutdownSteps); ++i) {
    if (strcmp(sShutdownSteps[i].mTopic, aTopic) == 0) {
      nextStep = i;
      break;
    }
  }
  MOZ_ASSERT(nextStep != -1);
  mCurrentStep = nextStep;
}

void
nsTerminator::UpdateTelemetry()
{
  if (!Telemetry::CanRecordExtended() || !gWriteReady) {
    return;
  }

  //
  // We need Telemetry data on the effective duration of each step,
  // to be able to tune the time-to-crash of each of both the
  // Terminator and AsyncShutdown. However, at this stage, it is too
  // late to record such data into Telemetry, so we write it to disk
  // and read it upon the next startup.
  //

  // Build JSON.
  UniquePtr<nsCString> telemetryData(new nsCString());
  telemetryData->AppendLiteral("{");
  size_t fields = 0;
  for (auto& shutdownStep : sShutdownSteps) {
    if (shutdownStep.mTicks < 0) {
      // Ignore this field.
      continue;
    }
    if (fields++ > 0) {
      telemetryData->Append(", ");
    }
    telemetryData->AppendLiteral(R"(")");
    telemetryData->Append(shutdownStep.mTopic);
    telemetryData->AppendLiteral(R"(": )");
    telemetryData->AppendInt(shutdownStep.mTicks);
  }
  telemetryData->AppendLiteral("}");

  if (fields == 0) {
    // Nothing to write
      return;
  }

  //
  // Send data to the worker thread.
  //
  delete gWriteData.exchange(telemetryData.release()); // Clear any data that hasn't been written yet

  // In case the worker thread was sleeping, wake it up.
  PR_EnterMonitor(gWriteReady);
  PR_Notify(gWriteReady);
  PR_ExitMonitor(gWriteReady);
}

void
nsTerminator::UpdateCrashReport(const char* aTopic)
{
#if defined(MOZ_CRASHREPORTER)
  // In case of crash, we wish to know where in shutdown we are
  nsAutoCString report(aTopic);

  Unused << CrashReporter::AnnotateCrashReport(NS_LITERAL_CSTRING("ShutdownProgress"),
                                               report);
#endif // defined(MOZ_CRASHREPORTER)
}


} // namespace mozilla
