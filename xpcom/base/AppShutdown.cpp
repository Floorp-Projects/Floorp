/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AppShutdown.h"

#ifdef XP_WIN
#  include <windows.h>
#else
#  include <unistd.h>
#endif

#include "GeckoProfiler.h"
#include "mozilla/CmdLineAndEnvUtils.h"
#include "mozilla/PoisonIOInterposer.h"
#include "mozilla/Printf.h"
#include "mozilla/scache/StartupCache.h"
#include "mozilla/StartupTimeline.h"
#include "mozilla/StaticPrefs_toolkit.h"
#include "mozilla/LateWriteChecks.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsAppRunner.h"
#include "nsDirectoryServiceUtils.h"
#include "prenv.h"

namespace mozilla {

static ShutdownPhase sFastShutdownPhase = ShutdownPhase::NotInShutdown;
static ShutdownPhase sLateWriteChecksPhase = ShutdownPhase::NotInShutdown;
static AppShutdownMode sShutdownMode = AppShutdownMode::Normal;
static bool sIsShuttingDown = false;

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
      return ShutdownPhase::ShutdownPostLastCycleCollection;
    case 2:
      return ShutdownPhase::ShutdownThreads;
    case 3:
      return ShutdownPhase::Shutdown;
      // NOTE: the remaining values from the ShutdownPhase enum will be added
      // when we're at least reasonably confident that the world won't come
      // crashing down if we do a fast shutdown at that point.
  }
  return ShutdownPhase::NotInShutdown;
}

bool AppShutdown::IsShuttingDown() { return sIsShuttingDown; }

void AppShutdown::SaveEnvVarsForPotentialRestart() {
  const char* s = PR_GetEnv("XUL_APP_FILE");
  if (s) {
    sSavedXulAppFile = Smprintf("%s=%s", "XUL_APP_FILE", s).release();
    MOZ_LSAN_INTENTIONALLY_LEAK_OBJECT(sSavedXulAppFile);
  }
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

void AppShutdown::Init(AppShutdownMode aMode) {
  if (sShutdownMode == AppShutdownMode::Normal) {
    sShutdownMode = aMode;
  }

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
  if (aPhase == sFastShutdownPhase) {
    StopLateWriteChecks();
    if (auto* cache = scache::StartupCache::GetSingletonNoInit()) {
      cache->EnsureShutdownWriteComplete();
    }
    RecordShutdownEndTimeStamp();
    MaybeDoRestart();

#ifdef MOZ_GECKO_PROFILER
    profiler_shutdown(IsFastShutdown::Yes);
#endif

    DoImmediateExit();
  } else if (aPhase == sLateWriteChecksPhase) {
#ifdef XP_MACOSX
    OnlyReportDirtyWrites();
#endif /* XP_MACOSX */
    BeginLateWriteChecks();
  }
}

void AppShutdown::OnShutdownConfirmed() {
  sIsShuttingDown = true;
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

void AppShutdown::DoImmediateExit() {
#ifdef XP_WIN
  HANDLE process = ::GetCurrentProcess();
  if (::TerminateProcess(process, 0)) {
    ::WaitForSingleObject(process, INFINITE);
  }
  MOZ_CRASH("TerminateProcess failed.");
#else
  _exit(0);
#endif
}

bool AppShutdown::IsRestarting() {
  return sShutdownMode == AppShutdownMode::Restart;
}

}  // namespace mozilla
