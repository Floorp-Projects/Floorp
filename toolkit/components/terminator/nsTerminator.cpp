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
#include "nsString.h"
#include "nsServiceManagerUtils.h"

#include "nsIObserverService.h"
#include "nsIPrefService.h"
#if defined(MOZ_CRASHREPORTER)
#include "nsExceptionHandler.h"
#endif

#include "mozilla/ArrayUtils.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/Preferences.h"
#include "mozilla/Services.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/unused.h"

// Normally, the number of milliseconds that AsyncShutdown waits until
// it decides to crash is specified as a preference. We use the
// following value as a fallback if for some reason the preference is
// absent.
#define FALLBACK_ASYNCSHUTDOWN_CRASH_AFTER_MS 60000

// Additional number of milliseconds to wait until we decide to exit
// forcefully.
#define ADDITIONAL_WAIT_BEFORE_CRASH_MS 3000

// One second, in ticks.
#define TICK_DURATION 1000

namespace mozilla {

namespace {

/**
 * Set to `true` by the main thread whenever we pass a shutdown phase,
 * which means that the shutdown is still ongoing. Reset to `false` by
 * the Terminator thread, once it has acknowledged the progress.
 */
Atomic<bool> gProgress(false);

struct Options {
  int32_t crashAfterMS;
};

void
Run(void* arg)
{
  PR_SetCurrentThreadName("Shutdown Hang Terminator");

  // Let's copy and deallocate options, that's one less leak to worry
  // about.
  UniquePtr<Options> options((Options*)arg);
  int32_t crashAfterMS = options->crashAfterMS;
  options = nullptr;

  int32_t timeToLive = crashAfterMS;
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
    PR_Sleep(TICK_DURATION);
    if (gProgress.exchange(false)) {
      // We have passed at least one shutdown phase while waiting.
      // Shutdown is still alive, reset the countdown.
      timeToLive = crashAfterMS;
      continue;
    }
    timeToLive -= TICK_DURATION;
    if (timeToLive >= 0) {
      continue;
    }

    // Shutdown is apparently dead. Crash the process.
    MOZ_CRASH("Shutdown too long, probably frozen, causing a crash.");
  }
}

} // anonymous namespace

static char const *const sObserverTopics[] = {
  "quit-application",
  "profile-change-teardown",
  "profile-before-change",
  "xpcom-will-shutdown",
  "xpcom-shutdown",
};

NS_IMPL_ISUPPORTS(nsTerminator, nsIObserver)

nsTerminator::nsTerminator()
  : mInitialized(false)
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

  for (size_t i = 0; i < ArrayLength(sObserverTopics); ++i) {
    DebugOnly<nsresult> rv = os->AddObserver(this, sObserverTopics[i], false);
#if defined(DEBUG)
    NS_WARN_IF(NS_FAILED(rv));
#endif // defined(DEBUG)
  }
  return NS_OK;
}

// Actually launch the thread. This takes place at the first sign of shutdown.
void
nsTerminator::Start() {
  // Determine how long we need to wait

  int32_t crashAfterMS =
    Preferences::GetInt("toolkit.asyncshutdown.crash_timeout",
                        FALLBACK_ASYNCSHUTDOWN_CRASH_AFTER_MS);

  // Add a little padding, to ensure that we do not crash before
  // AsyncShutdown.
  crashAfterMS += ADDITIONAL_WAIT_BEFORE_CRASH_MS;

  UniquePtr<Options> options(new Options());
  options->crashAfterMS = crashAfterMS;

  // Allocate and start the thread.
  // By design, it will never finish, nor be deallocated.
  DebugOnly<PRThread*> thread = PR_CreateThread(
    PR_SYSTEM_THREAD, /* This thread will not prevent the process from terminating */
    Run,
    options.release(),
    PR_PRIORITY_LOW,
    PR_GLOBAL_THREAD /* Make sure that the thread is never cooperatively scheduled */,
    PR_UNJOINABLE_THREAD,
    0 /* Use default stack size */
  );

  MOZ_ASSERT(thread);
  mInitialized = true;
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

  // Inform the thread that we have advanced by one phase.
  gProgress.exchange(true);

#if defined(MOZ_CRASHREPORTER)
  // In case of crash, we wish to know where in shutdown we are
  unused << CrashReporter::AnnotateCrashReport(NS_LITERAL_CSTRING("ShutdownProgress"),
                                               nsAutoCString(aTopic));
#endif // defined(MOZ_CRASH_REPORTER)

  nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
  MOZ_RELEASE_ASSERT(os);
  (void)os->RemoveObserver(this, aTopic);
  return NS_OK;
}

} // namespace mozilla
