/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ShutdownPhase.h"
#ifdef XP_WIN
#  include <windows.h>
#  include "mozilla/PreXULSkeletonUI.h"
#else
#  include <unistd.h>
#endif

#include "ProfilerControl.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/CmdLineAndEnvUtils.h"
#include "mozilla/PoisonIOInterposer.h"
#include "mozilla/Printf.h"
#include "mozilla/scache/StartupCache.h"
#include "mozilla/SpinEventLoopUntil.h"
#include "mozilla/StartupTimeline.h"
#include "mozilla/StaticPrefs_toolkit.h"
#include "mozilla/LateWriteChecks.h"
#include "mozilla/Services.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsAppRunner.h"
#include "nsDirectoryServiceUtils.h"
#include "nsExceptionHandler.h"
#include "nsICertStorage.h"
#include "nsThreadUtils.h"

#include "AppShutdown.h"

// TODO: understand why on Android we cannot include this and if we should
#ifndef ANDROID
#  include "nsTerminator.h"
#endif
#include "prenv.h"

#ifdef MOZ_BACKGROUNDTASKS
#  include "mozilla/BackgroundTasks.h"
#endif

namespace mozilla {

const char* sPhaseObserverKeys[] = {
    nullptr,                            // NotInShutdown
    "quit-application",                 // AppShutdownConfirmed
    "profile-change-net-teardown",      // AppShutdownNetTeardown
    "profile-change-teardown",          // AppShutdownTeardown
    "profile-before-change",            // AppShutdown
    "profile-before-change-qm",         // AppShutdownQM
    "profile-before-change-telemetry",  // AppShutdownTelemetry
    "xpcom-will-shutdown",              // XPCOMWillShutdown
    "xpcom-shutdown",                   // XPCOMShutdown
    "xpcom-shutdown-threads",           // XPCOMShutdownThreads
    nullptr,                            // XPCOMShutdownFinal
    nullptr                             // CCPostLastCycleCollection
};

static_assert(sizeof(sPhaseObserverKeys) / sizeof(sPhaseObserverKeys[0]) ==
              (size_t)ShutdownPhase::ShutdownPhase_Length);

const char* sPhaseReadableNames[] = {"NotInShutdown",
                                     "AppShutdownConfirmed",
                                     "AppShutdownNetTeardown",
                                     "AppShutdownTeardown",
                                     "AppShutdown",
                                     "AppShutdownQM",
                                     "AppShutdownTelemetry",
                                     "XPCOMWillShutdown",
                                     "XPCOMShutdown",
                                     "XPCOMShutdownThreads",
                                     "XPCOMShutdownFinal",
                                     "CCPostLastCycleCollection"};

static_assert(sizeof(sPhaseReadableNames) / sizeof(sPhaseReadableNames[0]) ==
              (size_t)ShutdownPhase::ShutdownPhase_Length);

#ifndef ANDROID
static nsTerminator* sTerminator = nullptr;
#endif

static ShutdownPhase sFastShutdownPhase = ShutdownPhase::NotInShutdown;
static ShutdownPhase sLateWriteChecksPhase = ShutdownPhase::NotInShutdown;
static AppShutdownMode sShutdownMode = AppShutdownMode::Normal;
static Atomic<ShutdownPhase> sCurrentShutdownPhase(
    ShutdownPhase::NotInShutdown);
static int sExitCode = 0;

// These environment variable strings are all deliberately copied and leaked
// due to requirements of PR_SetEnv and similar.
static char* sSavedXulAppFile = nullptr;
#ifdef XP_WIN
static wchar_t* sSavedProfDEnvVar = nullptr;
static wchar_t* sSavedProfLDEnvVar = nullptr;
#else
static char* sSavedProfDEnvVar = nullptr;
static char* sSavedProfLDEnvVar = nullptr;
#endif

ShutdownPhase GetShutdownPhaseFromPrefValue(int32_t aPrefValue) {
  switch (aPrefValue) {
    case 1:
      return ShutdownPhase::CCPostLastCycleCollection;
    case 2:
      return ShutdownPhase::XPCOMShutdownThreads;
    case 3:
      return ShutdownPhase::XPCOMShutdown;
      // NOTE: the remaining values from the ShutdownPhase enum will be added
      // when we're at least reasonably confident that the world won't come
      // crashing down if we do a fast shutdown at that point.
  }
  return ShutdownPhase::NotInShutdown;
}

ShutdownPhase AppShutdown::GetCurrentShutdownPhase() {
  return sCurrentShutdownPhase;
}

bool AppShutdown::IsInOrBeyond(ShutdownPhase aPhase) {
  return (sCurrentShutdownPhase >= aPhase);
}

int AppShutdown::GetExitCode() { return sExitCode; }

void AppShutdown::SaveEnvVarsForPotentialRestart() {
  const char* s = PR_GetEnv("XUL_APP_FILE");
  if (s) {
    sSavedXulAppFile = Smprintf("%s=%s", "XUL_APP_FILE", s).release();
    MOZ_LSAN_INTENTIONALLY_LEAK_OBJECT(sSavedXulAppFile);
  }
}

const char* AppShutdown::GetObserverKey(ShutdownPhase aPhase) {
  return sPhaseObserverKeys[static_cast<std::underlying_type_t<ShutdownPhase>>(
      aPhase)];
}

const char* AppShutdown::GetShutdownPhaseName(ShutdownPhase aPhase) {
  return sPhaseReadableNames[static_cast<std::underlying_type_t<ShutdownPhase>>(
      aPhase)];
}

void AppShutdown::MaybeDoRestart() {
  if (sShutdownMode == AppShutdownMode::Restart) {
    StopLateWriteChecks();

    // Since we'll be launching our child while we're still alive, make sure
    // we've unlocked the profile first, otherwise the child could hit its
    // profile lock check before we've exited and thus released our lock.
    UnlockProfile();

    if (sSavedXulAppFile) {
      PR_SetEnv(sSavedXulAppFile);
    }

#ifdef XP_WIN
    if (sSavedProfDEnvVar && !EnvHasValue("XRE_PROFILE_PATH")) {
      SetEnvironmentVariableW(L"XRE_PROFILE_PATH", sSavedProfDEnvVar);
    }
    if (sSavedProfLDEnvVar && !EnvHasValue("XRE_PROFILE_LOCAL_PATH")) {
      SetEnvironmentVariableW(L"XRE_PROFILE_LOCAL_PATH", sSavedProfLDEnvVar);
    }
    Unused << NotePreXULSkeletonUIRestarting();
#else
    if (sSavedProfDEnvVar && !EnvHasValue("XRE_PROFILE_PATH")) {
      PR_SetEnv(sSavedProfDEnvVar);
    }
    if (sSavedProfLDEnvVar && !EnvHasValue("XRE_PROFILE_LOCAL_PATH")) {
      PR_SetEnv(sSavedProfLDEnvVar);
    }
#endif

    LaunchChild(true);
  }
}

#ifdef XP_WIN
wchar_t* CopyPathIntoNewWCString(nsIFile* aFile) {
  wchar_t* result = nullptr;
  nsAutoString resStr;
  aFile->GetPath(resStr);
  if (resStr.Length() > 0) {
    result = (wchar_t*)malloc((resStr.Length() + 1) * sizeof(wchar_t));
    if (result) {
      wcscpy(result, resStr.get());
      result[resStr.Length()] = 0;
    }
  }

  return result;
}
#endif

void AppShutdown::Init(AppShutdownMode aMode, int aExitCode,
                       AppShutdownReason aReason) {
  if (sShutdownMode == AppShutdownMode::Normal) {
    sShutdownMode = aMode;
  }
  AppShutdown::AnnotateShutdownReason(aReason);

  sExitCode = aExitCode;

#ifndef ANDROID
  sTerminator = new nsTerminator();
#endif

  // Late-write checks needs to find the profile directory, so it has to
  // be initialized before services::Shutdown or (because of
  // xpcshell tests replacing the service) modules being unloaded.
  InitLateWriteChecks();

  int32_t fastShutdownPref = StaticPrefs::toolkit_shutdown_fastShutdownStage();
  sFastShutdownPhase = GetShutdownPhaseFromPrefValue(fastShutdownPref);
  int32_t lateWriteChecksPref =
      StaticPrefs::toolkit_shutdown_lateWriteChecksStage();
  sLateWriteChecksPhase = GetShutdownPhaseFromPrefValue(lateWriteChecksPref);

  // Very early shutdowns can happen before the startup cache is even
  // initialized; don't bother initializing it during shutdown.
  if (auto* cache = scache::StartupCache::GetSingletonNoInit()) {
    cache->MaybeInitShutdownWrite();
  }
}

void AppShutdown::MaybeFastShutdown(ShutdownPhase aPhase) {
  // For writes which we want to ensure are recorded, we don't want to trip
  // the late write checking code. Anything that writes to disk and which
  // we don't want to skip should be listed out explicitly in this section.
  if (aPhase == sFastShutdownPhase || aPhase == sLateWriteChecksPhase) {
    if (auto* cache = scache::StartupCache::GetSingletonNoInit()) {
      cache->EnsureShutdownWriteComplete();
    }

    nsresult rv;
    nsCOMPtr<nsICertStorage> certStorage =
        do_GetService("@mozilla.org/security/certstorage;1", &rv);
    if (NS_SUCCEEDED(rv)) {
      SpinEventLoopUntil("AppShutdown::MaybeFastShutdown"_ns, [&]() {
        int32_t remainingOps;
        nsresult rv = certStorage->GetRemainingOperationCount(&remainingOps);
        NS_ASSERTION(NS_SUCCEEDED(rv),
                     "nsICertStorage::getRemainingOperationCount failed during "
                     "shutdown");
        return NS_FAILED(rv) || remainingOps <= 0;
      });
    }
  }
  if (aPhase == sFastShutdownPhase) {
    StopLateWriteChecks();
    RecordShutdownEndTimeStamp();
    MaybeDoRestart();

    profiler_shutdown(IsFastShutdown::Yes);

    DoImmediateExit(sExitCode);
  } else if (aPhase == sLateWriteChecksPhase) {
#ifdef XP_MACOSX
    OnlyReportDirtyWrites();
#endif /* XP_MACOSX */
    BeginLateWriteChecks();
  }
}

void AppShutdown::OnShutdownConfirmed() {
  // If we're restarting, we need to save environment variables correctly
  // while everything is still alive to do so.
  if (sShutdownMode == AppShutdownMode::Restart) {
    nsCOMPtr<nsIFile> profD;
    nsCOMPtr<nsIFile> profLD;
    NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR, getter_AddRefs(profD));
    NS_GetSpecialDirectory(NS_APP_USER_PROFILE_LOCAL_50_DIR,
                           getter_AddRefs(profLD));
#ifdef XP_WIN
    sSavedProfDEnvVar = CopyPathIntoNewWCString(profD);
    sSavedProfLDEnvVar = CopyPathIntoNewWCString(profLD);
#else
    nsAutoCString profDStr;
    profD->GetNativePath(profDStr);
    sSavedProfDEnvVar =
        Smprintf("XRE_PROFILE_PATH=%s", profDStr.get()).release();
    nsAutoCString profLDStr;
    profLD->GetNativePath(profLDStr);
    sSavedProfLDEnvVar =
        Smprintf("XRE_PROFILE_LOCAL_PATH=%s", profLDStr.get()).release();
#endif
    MOZ_LSAN_INTENTIONALLY_LEAK_OBJECT(sSavedProfDEnvVar);
    MOZ_LSAN_INTENTIONALLY_LEAK_OBJECT(sSavedProfLDEnvVar);
  }
}

void AppShutdown::DoImmediateExit(int aExitCode) {
#ifdef XP_WIN
  HANDLE process = ::GetCurrentProcess();
  if (::TerminateProcess(process, aExitCode)) {
    ::WaitForSingleObject(process, INFINITE);
  }
  MOZ_CRASH("TerminateProcess failed.");
#else
  _exit(aExitCode);
#endif
}

bool AppShutdown::IsRestarting() {
  return sShutdownMode == AppShutdownMode::Restart;
}

void AppShutdown::AnnotateShutdownReason(AppShutdownReason aReason) {
  auto key = CrashReporter::Annotation::ShutdownReason;
  nsCString reasonStr;
  switch (aReason) {
    case AppShutdownReason::AppClose:
      reasonStr = "AppClose"_ns;
      break;
    case AppShutdownReason::AppRestart:
      reasonStr = "AppRestart"_ns;
      break;
    case AppShutdownReason::OSForceClose:
      reasonStr = "OSForceClose"_ns;
      break;
    case AppShutdownReason::OSSessionEnd:
      reasonStr = "OSSessionEnd"_ns;
      break;
    case AppShutdownReason::OSShutdown:
      reasonStr = "OSShutdown"_ns;
      break;
    case AppShutdownReason::WinUnexpectedMozQuit:
      reasonStr = "WinUnexpectedMozQuit"_ns;
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("We should know the given reason for shutdown.");
      reasonStr = "Unknown"_ns;
      break;
  }
  CrashReporter::AnnotateCrashReport(key, reasonStr);
}

#ifdef DEBUG
static bool sNotifyingShutdownObservers = false;
static bool sAdvancingShutdownPhase = false;

bool AppShutdown::IsNoOrLegalShutdownTopic(const char* aTopic) {
  if (!XRE_IsParentProcess()) {
    // Until we know what to do with AppShutdown for child processes,
    // we ignore them for now. See bug 1697745.
    return true;
  }
  ShutdownPhase phase = GetShutdownPhaseFromTopic(aTopic);
  return phase == ShutdownPhase::NotInShutdown ||
         (sNotifyingShutdownObservers && phase == sCurrentShutdownPhase);
}
#endif

void AppShutdown::AdvanceShutdownPhaseInternal(
    ShutdownPhase aPhase, bool doNotify, const char16_t* aNotificationData,
    const nsCOMPtr<nsISupports>& aNotificationSubject) {
  AssertIsOnMainThread();
#ifdef DEBUG
  // Prevent us from re-entrance
  MOZ_ASSERT(!sAdvancingShutdownPhase);
  sAdvancingShutdownPhase = true;
  auto exit = MakeScopeExit([] { sAdvancingShutdownPhase = false; });
#endif

  // We ensure that we can move only forward. We cannot
  // MOZ_ASSERT here as there are some tests that fire
  // notifications out of shutdown order.
  // See for example test_sss_sanitizeOnShutdown.js
  if (sCurrentShutdownPhase >= aPhase) {
    return;
  }

  nsCOMPtr<nsIThread> thread = do_GetCurrentThread();

  // AppShutdownConfirmed is special in some ways as
  // - we can be called on top of a nested event loop (and it is the phase for
  //   which SpinEventLoopUntilOrQuit breaks, so we want to return soon)
  // - we can be called from a sync marionette function that wants immediate
  //   feedback, too
  // - in general, AppShutdownConfirmed will fire the "quit-application"
  //   notification which in turn will cause an event to be dispatched that
  //   runs all the rest of our shutdown sequence which we do not want to be
  //   processed on top of the running event.
  // Thus we never do any NS_ProcessPendingEvents for it.
  bool mayProcessPending = (aPhase > ShutdownPhase::AppShutdownConfirmed);

  // Give runnables dispatched between two calls to AdvanceShutdownPhase
  // a chance to run before actually advancing the phase. As we excluded
  // AppShutdownConfirmed above we can be sure that the processing is
  // covered by the terminator's timer of the previous phase during normal
  // shutdown (except out-of-order calls from some test).
  // Note that this affects only main thread runnables, such that the correct
  // way of ensuring shutdown processing remains to have an async shutdown
  // blocker.
  if (mayProcessPending && thread) {
    NS_ProcessPendingEvents(thread);
  }

  // From now on any IsInOrBeyond checks will find the new phase set.
  sCurrentShutdownPhase = aPhase;

#ifndef ANDROID
  if (sTerminator) {
    sTerminator->AdvancePhase(aPhase);
  }
#endif

  AppShutdown::MaybeFastShutdown(aPhase);

  // This will null out the gathered pointers for this phase synchronously.
  // Note that we keep the old order here to avoid breakage, so be aware that
  // the notifications fired below will find these already cleared in case
  // you expected the opposite.
  mozilla::KillClearOnShutdown(aPhase);

  // Empty our MT event queue to process any side effects thereof.
  if (mayProcessPending && thread) {
    NS_ProcessPendingEvents(thread);
  }

  if (doNotify) {
    const char* aTopic = AppShutdown::GetObserverKey(aPhase);
    if (aTopic) {
      nsCOMPtr<nsIObserverService> obsService =
          mozilla::services::GetObserverService();
      if (obsService) {
#ifdef DEBUG
        sNotifyingShutdownObservers = true;
        auto reset = MakeScopeExit([] { sNotifyingShutdownObservers = false; });
#endif
        obsService->NotifyObservers(aNotificationSubject, aTopic,
                                    aNotificationData);
        // Empty our MT event queue again after the notification has finished
        if (mayProcessPending && thread) {
          NS_ProcessPendingEvents(thread);
        }
      }
    }
  }
}

/**
 * XXX: Before tackling bug 1697745 we need the
 * possibility to advance the phase without notification
 * in the content process.
 */
void AppShutdown::AdvanceShutdownPhaseWithoutNotify(ShutdownPhase aPhase) {
  AdvanceShutdownPhaseInternal(aPhase, /* doNotify */ false, nullptr, nullptr);
}

void AppShutdown::AdvanceShutdownPhase(
    ShutdownPhase aPhase, const char16_t* aNotificationData,
    const nsCOMPtr<nsISupports>& aNotificationSubject) {
  AdvanceShutdownPhaseInternal(aPhase, /* doNotify */ true, aNotificationData,
                               aNotificationSubject);
}

ShutdownPhase AppShutdown::GetShutdownPhaseFromTopic(const char* aTopic) {
  for (size_t i = 0; i < ArrayLength(sPhaseObserverKeys); ++i) {
    if (sPhaseObserverKeys[i] && !strcmp(sPhaseObserverKeys[i], aTopic)) {
      return static_cast<ShutdownPhase>(i);
    }
  }
  return ShutdownPhase::NotInShutdown;
}

}  // namespace mozilla
