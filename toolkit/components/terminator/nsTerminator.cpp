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

#include "mozilla/ShutdownPhase.h"
#include "nsTerminator.h"

#include "prthread.h"
#include "prmon.h"
#include "prio.h"

#include "nsString.h"
#include "nsDirectoryServiceUtils.h"
#include "nsAppDirectoryServiceDefs.h"

#include "nsExceptionHandler.h"
#include "GeckoProfiler.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"

#if defined(XP_WIN)
#  include <windows.h>
#else
#  include <unistd.h>
#endif

#include "mozilla/AppShutdown.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/Atomics.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/IntentionalCrash.h"
#include "mozilla/MemoryChecking.h"
#include "mozilla/Preferences.h"
#include "mozilla/SpinEventLoopUntil.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Unused.h"
#include "mozilla/Telemetry.h"

#include "mozilla/dom/IOUtils.h"
#include "mozilla/dom/workerinternals/RuntimeService.h"

// Normally, the number of milliseconds that AsyncShutdown waits until
// it decides to crash is specified as a preference. We use the
// following value as a fallback if for some reason the preference is
// absent.
#define FALLBACK_ASYNCSHUTDOWN_CRASH_AFTER_MS 60000

// Additional number of milliseconds to wait until we decide to exit
// forcefully.
#define ADDITIONAL_WAIT_BEFORE_CRASH_MS 3000

#define HEARTBEAT_INTERVAL_MS 100

namespace mozilla {

namespace {

/**
 * A step during shutdown.
 *
 * Shutdown is divided in steps, which all map to an observer
 * notification. The duration of a step is defined as the number of
 * ticks between the time we receive a notification and the next one.
 */
struct ShutdownStep {
  mozilla::ShutdownPhase mPhase;
  Atomic<int> mTicks;

  constexpr explicit ShutdownStep(mozilla::ShutdownPhase aPhase)
      : mPhase(aPhase), mTicks(-1) {}
};

static ShutdownStep sShutdownSteps[] = {
    ShutdownStep(mozilla::ShutdownPhase::AppShutdownConfirmed),
    ShutdownStep(mozilla::ShutdownPhase::AppShutdownNetTeardown),
    ShutdownStep(mozilla::ShutdownPhase::AppShutdownTeardown),
    ShutdownStep(mozilla::ShutdownPhase::AppShutdown),
    ShutdownStep(mozilla::ShutdownPhase::AppShutdownQM),
    ShutdownStep(mozilla::ShutdownPhase::XPCOMWillShutdown),
    ShutdownStep(mozilla::ShutdownPhase::XPCOMShutdown),
    ShutdownStep(mozilla::ShutdownPhase::XPCOMShutdownThreads),
    ShutdownStep(mozilla::ShutdownPhase::XPCOMShutdownFinal),
    ShutdownStep(mozilla::ShutdownPhase::CCPostLastCycleCollection),
};

int GetStepForPhase(mozilla::ShutdownPhase aPhase) {
  for (size_t i = 0; i < std::size(sShutdownSteps); i++) {
    if (sShutdownSteps[i].mPhase >= aPhase) {
      return (int)i;
    }
  }
  return -1;
}

// Utility function: create a thread that is non-joinable,
// does not prevent the process from terminating, is never
// cooperatively scheduled, and uses a default stack size.
PRThread* CreateSystemThread(void (*start)(void* arg), void* arg) {
  PRThread* thread =
      PR_CreateThread(PR_SYSTEM_THREAD, /* This thread will not prevent the
                                           process from terminating */
                      start, arg, PR_PRIORITY_LOW,
                      PR_GLOBAL_THREAD, /* Make sure that the thread is never
                                           cooperatively scheduled */
                      PR_UNJOINABLE_THREAD, 0 /* Use default stack size */
      );
  MOZ_LSAN_INTENTIONALLY_LEAK_OBJECT(
      thread);  // This pointer will never be deallocated.
  return thread;
}

////////////////////////////////////////////
//
// The watchdog
//
// This nspr thread is in charge of crashing the process if any stage of
// shutdown lasts more than some predefined duration. As a side-effect, it
// measures the duration of each stage of shutdown.
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
void RunWatchdog(void* arg) {
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
    Sleep(HEARTBEAT_INTERVAL_MS /* ms */);
#else
    usleep(HEARTBEAT_INTERVAL_MS * 1000 /* usec */);
#endif

    if (gHeartbeat++ < timeToLive) {
      continue;
    }

    // Arrived here we know we will crash in a way or another.
    NoteIntentionalCrash(XRE_GetProcessTypeString());

    // Until we have general log output for crash annotations in treeherder
    // (bug 1728721) we manually spit out our nested event loop stack.
    // XXX: Remove once bug 1728721 is fixed.
    nsCString stack;
    AutoNestedEventLoopAnnotation::CopyCurrentStack(stack);
    printf_stderr(
        "RunWatchdog: Mainthread nested event loops during hang: \n --- %s\n",
        stack.get());

    // Let's find the last known shutdown phase.
    mozilla::ShutdownPhase lastPhase = mozilla::ShutdownPhase::NotInShutdown;
    // Looping inverse here to make the search more robust in case
    // the observer that triggers UpdateHeartbeat was not called
    // at all or in the expected order on some step. This should
    // give us always the last known ShutdownStep.
    for (int i = ArrayLength(sShutdownSteps) - 1; i >= 0; --i) {
      if (sShutdownSteps[i].mTicks > -1) {
        lastPhase = sShutdownSteps[i].mPhase;
        break;
      }
    }

    if (lastPhase == mozilla::ShutdownPhase::NotInShutdown) {
      // This is not something we expect to ever happen, but still.
      CrashReporter::SetMinidumpAnalysisAllThreads();
      MOZ_CRASH("Shutdown hanging before starting any known phase.");
    }

    // First check if worker shutdown started and is incomplete, in case
    // report running workers.
    mozilla::dom::workerinternals::RuntimeService* runtimeService =
        mozilla::dom::workerinternals::RuntimeService::GetService();
    if (runtimeService) {
      // CrashIfHanging will check if we actually ever asked for worker
      // shutdown, so calling it before is a no-op.
      runtimeService->CrashIfHanging();
    }

    // Otherwise just report our shutdown phase.
    // This string will be leaked.
    nsCString msg;
    msg.AppendPrintf(
        "Shutdown hanging at step %s. "
        "Something is blocking the main-thread.",
        mozilla::AppShutdown::GetShutdownPhaseName(lastPhase));

    CrashReporter::SetMinidumpAnalysisAllThreads();
    MOZ_CRASH_UNSAFE(strdup(msg.BeginReading()));
  }
}

}  // namespace

NS_IMPL_ISUPPORTS(nsTerminator, nsIObserver, nsITerminatorTest)

nsTerminator::nsTerminator() : mInitialized(false), mCurrentStep(-1) {}

// Actually launch these threads. This takes place at the first sign of
// shutdown.
void nsTerminator::Start() {
  MOZ_ASSERT(!mInitialized);

  StartWatchdog();
  mInitialized = true;
}

NS_IMETHODIMP
nsTerminator::Observe(nsISupports*, const char* aTopic, const char16_t*) {
  // This Observe is now only used for testing purposes.
  // XXX: Check if we should change our testing strategy.
  if (strcmp(aTopic, "terminator-test-quit-application") == 0) {
    AdvancePhase(mozilla::ShutdownPhase::AppShutdownConfirmed);
  } else if (strcmp(aTopic, "terminator-test-profile-change-net-teardown") ==
             0) {
    AdvancePhase(mozilla::ShutdownPhase::AppShutdownNetTeardown);
  } else if (strcmp(aTopic, "terminator-test-profile-change-teardown") == 0) {
    AdvancePhase(mozilla::ShutdownPhase::AppShutdownTeardown);
  } else if (strcmp(aTopic, "terminator-test-profile-before-change") == 0) {
    AdvancePhase(mozilla::ShutdownPhase::AppShutdown);
  } else if (strcmp(aTopic, "terminator-test-profile-before-change-qm") == 0) {
    AdvancePhase(mozilla::ShutdownPhase::AppShutdownQM);
  } else if (strcmp(aTopic,
                    "terminator-test-profile-before-change-telemetry") == 0) {
    AdvancePhase(mozilla::ShutdownPhase::AppShutdownTelemetry);
  } else if (strcmp(aTopic, "terminator-test-xpcom-will-shutdown") == 0) {
    AdvancePhase(mozilla::ShutdownPhase::XPCOMWillShutdown);
  } else if (strcmp(aTopic, "terminator-test-xpcom-shutdown") == 0) {
    AdvancePhase(mozilla::ShutdownPhase::XPCOMShutdown);
  } else if (strcmp(aTopic, "terminator-test-xpcom-shutdown-threads") == 0) {
    AdvancePhase(mozilla::ShutdownPhase::XPCOMShutdownThreads);
  } else if (strcmp(aTopic, "terminator-test-XPCOMShutdownFinal") == 0) {
    AdvancePhase(mozilla::ShutdownPhase::XPCOMShutdownFinal);
  } else if (strcmp(aTopic, "terminator-test-CCPostLastCycleCollection") == 0) {
    AdvancePhase(mozilla::ShutdownPhase::CCPostLastCycleCollection);
  }

  return NS_OK;
}

// Prepare, allocate and start the watchdog thread.
// By design, it will never finish, nor be deallocated.
void nsTerminator::StartWatchdog() {
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

#ifdef MOZ_VALGRIND
  // If we're running on Valgrind, we'll be making forward progress at a
  // rate of somewhere between 1/25th and 1/50th of normal.  This can cause
  // timeouts frequently enough to be a problem for the Valgrind runs on
  // automation: see bug 1296819.  As an attempt to avoid the worst of this,
  // scale up the presented timeout by a factor of three.  For a
  // non-Valgrind-enabled build, or for an enabled build which isn't running
  // on Valgrind, the timeout is unchanged.
  if (RUNNING_ON_VALGRIND) {
    const int32_t scaleUp = 3;
    if (crashAfterMS >= (INT32_MAX / scaleUp) - 1) {
      // Defend against overflow
      crashAfterMS = INT32_MAX;
    } else {
      crashAfterMS *= scaleUp;
    }
  }
#endif

  UniquePtr<Options> options(new Options());
  // crashAfterTicks is guaranteed to be > 0 as
  // crashAfterMS >= ADDITIONAL_WAIT_BEFORE_CRASH_MS >> HEARTBEAT_INTERVAL_MS
  options->crashAfterTicks = crashAfterMS / HEARTBEAT_INTERVAL_MS;

  DebugOnly<PRThread*> watchdogThread =
      CreateSystemThread(RunWatchdog, options.release());
  MOZ_ASSERT(watchdogThread);
}

// This helper is here to preserve the existing crash reporting behavior
// based on observer topic names, using the shutdown phase name only for
// phases without associated topic.
const char* GetReadableNameForPhase(mozilla::ShutdownPhase aPhase) {
  const char* readableName = mozilla::AppShutdown::GetObserverKey(aPhase);
  if (!readableName) {
    readableName = mozilla::AppShutdown::GetShutdownPhaseName(aPhase);
  }
  return readableName;
}

void nsTerminator::AdvancePhase(mozilla::ShutdownPhase aPhase) {
  // If the phase is unknown, just ignore it.
  auto step = GetStepForPhase(aPhase);
  if (step < 0) {
    return;
  }

  // As we have seen examples in the wild of shutdown notifications
  // not being sent (or not being sent in the expected order), we do
  // not assume a specific order.
  if (!mInitialized) {
    Start();
  }

  UpdateHeartbeat(step);
  UpdateCrashReport(GetReadableNameForPhase(aPhase));
}

void nsTerminator::UpdateHeartbeat(int32_t aStep) {
  MOZ_ASSERT(aStep >= mCurrentStep);

  if (aStep > mCurrentStep) {
    // Reset the clock, find out how long the current phase has lasted.
    uint32_t ticks = gHeartbeat.exchange(0);
    if (mCurrentStep >= 0) {
      sShutdownSteps[mCurrentStep].mTicks = ticks;
    }
    sShutdownSteps[aStep].mTicks = 0;

    mCurrentStep = aStep;
  }
}

void nsTerminator::UpdateCrashReport(const char* aTopic) {
  // In case of crash, we wish to know where in shutdown we are
  CrashReporter::RecordAnnotationCString(
      CrashReporter::Annotation::ShutdownProgress, aTopic);
}

NS_IMETHODIMP
nsTerminator::GetTicksForShutdownPhases(JSContext* aCx,
                                        JS::MutableHandle<JS::Value> aRetval) {
  JS::Rooted<JSObject*> obj(aCx, JS_NewPlainObject(aCx));
  aRetval.setObject(*obj);

  for (auto& shutdownStep : sShutdownSteps) {
    if (shutdownStep.mTicks >= 0) {
      JS_DefineProperty(aCx, obj, GetReadableNameForPhase(shutdownStep.mPhase),
                        shutdownStep.mTicks, JSPROP_ENUMERATE);
    }
  }

  return NS_OK;
}  // namespace mozilla
}  // namespace mozilla
