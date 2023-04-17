/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AppShutdown_h
#define AppShutdown_h

#include <type_traits>
#include "nsCOMPtr.h"
#include "nsISupports.h"
#include "ShutdownPhase.h"

namespace mozilla {

enum class AppShutdownMode {
  Normal,
  Restart,
};

enum class AppShutdownReason {
  // No reason.
  Unknown,
  // Normal application shutdown.
  AppClose,
  // The application wants to restart.
  AppRestart,
  // The OS is force closing us.
  OSForceClose,
  // The user is logging off from the OS session, the system may stay alive.
  OSSessionEnd,
  // The system is shutting down (and maybe restarting).
  OSShutdown,
  // We unexpectedly received MOZ_WM_APP_QUIT, see bug 1827807.
  WinUnexpectedMozQuit,
};

class AppShutdown {
 public:
  static ShutdownPhase GetCurrentShutdownPhase();
  static bool IsInOrBeyond(ShutdownPhase aPhase);

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
   * Init the shutdown with the requested shutdown mode, exit code and optional
   * a reason (if missing it will be derived from aMode).
   */
  static void Init(AppShutdownMode aMode, int aExitCode,
                   AppShutdownReason aReason);

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
   * Map shutdown phase to observer key
   */
  static const char* GetObserverKey(ShutdownPhase aPhase);

  /**
   * Map shutdown phase to readable name
   */
  static const char* GetShutdownPhaseName(ShutdownPhase aPhase);

  /**
   * Map observer topic key to shutdown phase
   */
  static ShutdownPhase GetShutdownPhaseFromTopic(const char* aTopic);

#ifdef DEBUG
  /**
   * Check, if we are allowed to send a shutdown notification.
   * Shutdown specific topics are only allowed during calls to
   * AdvanceShutdownPhase itself.
   */
  static bool IsNoOrLegalShutdownTopic(const char* aTopic);
#endif

 private:
  /**
   * Set the shutdown reason annotation.
   */
  static void AnnotateShutdownReason(AppShutdownReason aReason);

  /**
   * This will perform a fast shutdown via _exit(0) or similar if the user's
   * prefs are configured to do so at this phase.
   */
  static void MaybeFastShutdown(ShutdownPhase aPhase);

  /**
   * Internal helper function, uses MaybeFastShutdown.
   */
  static void AdvanceShutdownPhaseInternal(
      ShutdownPhase aPhase, bool doNotify, const char16_t* aNotificationData,
      const nsCOMPtr<nsISupports>& aNotificationSubject);
};

}  // namespace mozilla

#endif  // AppShutdown_h
