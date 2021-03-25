/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AppShutdown_h
#define AppShutdown_h

#include <type_traits>
#include "ShutdownPhase.h"

namespace mozilla {

enum class AppShutdownMode {
  Normal,
  Restart,
};

class AppShutdown {
 public:
  static bool IsShuttingDown();
  static ShutdownPhase GetCurrentShutdownPhase();

  /**
   * Returns the current exit code that the process will be terminated with.
   */
  static int GetExitCode();

  /**
   * Save environment variables that we might need if the app initiates a
   * restart later in its lifecycle.
   */
  static void SaveEnvVarsForPotentialRestart();

  /**
   * Init the shutdown with the requested shutdown mode and exit code.
   */
  static void Init(AppShutdownMode aMode, int aExitCode);

  /**
   * Confirm that we are in fact going to be shutting down.
   */
  static void OnShutdownConfirmed();

  /**
   * If we've attempted to initiate a restart, this call will set up the
   * necessary environment variables and launch the new process.
   */
  static void MaybeDoRestart();

  /**
   * The _exit() call is not a safe way to terminate your own process on
   * Windows, because _exit runs DLL detach callbacks which run static
   * destructors for xul.dll.
   *
   * This method terminates the current process without those issues.
   *
   * Optionally a custom exit code can be supplied.
   */
  static void DoImmediateExit(int aExitCode = 0);

  /**
   * True if the application is currently attempting to shut down in order to
   * restart.
   */
  static bool IsRestarting();

  /**
   * Wrapper for shutdown notifications that informs the terminator before
   * we notify other observers. Calls MaybeFastShutdown.
   */
  static void AdvanceShutdownPhase(
      ShutdownPhase aPhase, const char16_t* aNotificationData = nullptr,
      const nsCOMPtr<nsISupports>& aNotificationSubject =
          nsCOMPtr<nsISupports>(nullptr));

  /**
   * XXX: Before tackling bug 1697745 we need the
   * possibility to advance the phase without notification
   * in the content process.
   */
  static void AdvanceShutdownPhaseWithoutNotify(ShutdownPhase aPhase);

  /**
   * This will perform a fast shutdown via _exit(0) or similar if the user's
   * prefs are configured to do so at this phase.
   */
  static void MaybeFastShutdown(ShutdownPhase aPhase);

  /**
   * Map shutdown phases to observer keys
   */
  static const char* GetObserverKey(ShutdownPhase aPhase);
};

}  // namespace mozilla

#endif  // AppShutdown_h
