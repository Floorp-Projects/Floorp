/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#define MOZ_USE_LAUNCHER_ERROR

#include "sandboxBroker.h"

#include <shlobj.h>
#include <string>

#include "base/win/windows_version.h"
#include "mozilla/Assertions.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/ImportDir.h"
#include "mozilla/Logging.h"
#include "mozilla/NSPRLogModulesParser.h"
#include "mozilla/Preferences.h"
#include "mozilla/SandboxSettings.h"
#include "mozilla/StaticPrefs_network.h"
#include "mozilla/StaticPrefs_security.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Telemetry.h"
#include "mozilla/WinDllServices.h"
#include "mozilla/WindowsVersion.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsCOMPtr.h"
#include "nsDirectoryServiceDefs.h"
#include "nsIFile.h"
#include "nsIProperties.h"
#include "nsServiceManagerUtils.h"
#include "nsString.h"
#include "nsTHashtable.h"
#include "sandbox/win/src/sandbox.h"
#include "sandbox/win/src/security_level.h"
#include "WinUtils.h"

namespace mozilla {

sandbox::BrokerServices* SandboxBroker::sBrokerService = nullptr;

// This is set to true in Initialize when our exe file name has a drive type of
// DRIVE_REMOTE, so that we can tailor the sandbox policy as some settings break
// fundamental things when running from a network drive. We default to false in
// case those checks fail as that gives us the strongest policy.
bool SandboxBroker::sRunningFromNetworkDrive = false;

// Cached special directories used for adding policy rules.
static UniquePtr<nsString> sBinDir;
static UniquePtr<nsString> sProfileDir;
static UniquePtr<nsString> sContentTempDir;
static UniquePtr<nsString> sRoamingAppDataDir;
static UniquePtr<nsString> sLocalAppDataDir;
#ifdef ENABLE_SYSTEM_EXTENSION_DIRS
static UniquePtr<nsString> sUserExtensionsDir;
#endif

static LazyLogModule sSandboxBrokerLog("SandboxBroker");

#define LOG_E(...) MOZ_LOG(sSandboxBrokerLog, LogLevel::Error, (__VA_ARGS__))
#define LOG_W(...) MOZ_LOG(sSandboxBrokerLog, LogLevel::Warning, (__VA_ARGS__))

// Used to store whether we have accumulated an error combination for this
// session.
static UniquePtr<nsTHashtable<nsCStringHashKey>> sLaunchErrors;

// This helper function is our version of SandboxWin::AddWin32kLockdownPolicy
// of Chromium, making sure the MITIGATION_WIN32K_DISABLE flag is set before
// adding the SUBSYS_WIN32K_LOCKDOWN rule which is required by
// PolicyBase::AddRuleInternal.
static sandbox::ResultCode AddWin32kLockdownPolicy(
    sandbox::TargetPolicy* aPolicy, bool aEnableOpm) {
  // On Windows 7, where Win32k lockdown is not supported, the Chromium
  // sandbox does something weird that breaks COM instantiation.
  if (!IsWin8OrLater()) {
    return sandbox::SBOX_ALL_OK;
  }

  sandbox::MitigationFlags flags = aPolicy->GetProcessMitigations();
  MOZ_ASSERT(!(flags & sandbox::MITIGATION_WIN32K_DISABLE),
             "Check not enabling twice.  Should not happen.");

  flags |= sandbox::MITIGATION_WIN32K_DISABLE;
  sandbox::ResultCode result = aPolicy->SetProcessMitigations(flags);
  if (result != sandbox::SBOX_ALL_OK) {
    return result;
  }

  result =
      aPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_WIN32K_LOCKDOWN,
                       aEnableOpm ? sandbox::TargetPolicy::IMPLEMENT_OPM_APIS
                                  : sandbox::TargetPolicy::FAKE_USER_GDI_INIT,
                       nullptr);
  if (result != sandbox::SBOX_ALL_OK) {
    return result;
  }
  if (aEnableOpm) {
    aPolicy->SetEnableOPMRedirection();
  }

  return result;
}

/* static */
void SandboxBroker::Initialize(sandbox::BrokerServices* aBrokerServices) {
  sBrokerService = aBrokerServices;

  sRunningFromNetworkDrive = widget::WinUtils::RunningFromANetworkDrive();
}

static void CacheDirAndAutoClear(nsIProperties* aDirSvc, const char* aDirKey,
                                 UniquePtr<nsString>* cacheVar) {
  nsCOMPtr<nsIFile> dirToCache;
  nsresult rv =
      aDirSvc->Get(aDirKey, NS_GET_IID(nsIFile), getter_AddRefs(dirToCache));
  if (NS_FAILED(rv)) {
    // This can only be an NS_WARNING, because it can fail for xpcshell tests.
    NS_WARNING("Failed to get directory to cache.");
    LOG_E("Failed to get directory to cache, key: %s.", aDirKey);
    return;
  }

  *cacheVar = MakeUnique<nsString>();
  ClearOnShutdown(cacheVar);
  MOZ_ALWAYS_SUCCEEDS(dirToCache->GetPath(**cacheVar));

  // Convert network share path to format for sandbox policy.
  if (Substring(**cacheVar, 0, 2).Equals(u"\\\\"_ns)) {
    (*cacheVar)->InsertLiteral(u"??\\UNC", 1);
  }
}

/* static */
void SandboxBroker::GeckoDependentInitialize() {
  MOZ_ASSERT(NS_IsMainThread());

  bool haveXPCOM = XRE_GetProcessType() != GeckoProcessType_RemoteSandboxBroker;
  if (haveXPCOM) {
    // Cache directory paths for use in policy rules, because the directory
    // service must be called on the main thread.
    nsresult rv;
    nsCOMPtr<nsIProperties> dirSvc =
        do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID, &rv);
    if (NS_FAILED(rv)) {
      MOZ_ASSERT(false,
                 "Failed to get directory service, cannot cache directories "
                 "for rules.");
      LOG_E(
          "Failed to get directory service, cannot cache directories for "
          "rules.");
      return;
    }

    CacheDirAndAutoClear(dirSvc, NS_GRE_DIR, &sBinDir);
    CacheDirAndAutoClear(dirSvc, NS_APP_USER_PROFILE_50_DIR, &sProfileDir);
    CacheDirAndAutoClear(dirSvc, NS_APP_CONTENT_PROCESS_TEMP_DIR,
                         &sContentTempDir);
    CacheDirAndAutoClear(dirSvc, NS_WIN_APPDATA_DIR, &sRoamingAppDataDir);
    CacheDirAndAutoClear(dirSvc, NS_WIN_LOCAL_APPDATA_DIR, &sLocalAppDataDir);
#ifdef ENABLE_SYSTEM_EXTENSION_DIRS
    CacheDirAndAutoClear(dirSvc, XRE_USER_SYS_EXTENSION_DIR,
                         &sUserExtensionsDir);
#endif
  }

  // Create sLaunchErrors up front because ClearOnShutdown must be called on the
  // main thread.
  sLaunchErrors = MakeUnique<nsTHashtable<nsCStringHashKey>>();
  ClearOnShutdown(&sLaunchErrors);
}

SandboxBroker::SandboxBroker() {
  if (sBrokerService) {
    scoped_refptr<sandbox::TargetPolicy> policy =
        sBrokerService->CreatePolicy();
    mPolicy = policy.get();
    mPolicy->AddRef();
    if (sRunningFromNetworkDrive) {
      mPolicy->SetDoNotUseRestrictingSIDs();
    }
  } else {
    mPolicy = nullptr;
  }
}

#define WSTRING(STRING) L"" STRING

static void AddMozLogRulesToPolicy(sandbox::TargetPolicy* aPolicy,
                                   const base::EnvironmentMap& aEnvironment) {
  auto it = aEnvironment.find(ENVIRONMENT_LITERAL("MOZ_LOG_FILE"));
  if (it == aEnvironment.end()) {
    it = aEnvironment.find(ENVIRONMENT_LITERAL("NSPR_LOG_FILE"));
  }
  if (it == aEnvironment.end()) {
    return;
  }

  char const* logFileModules = getenv("MOZ_LOG");
  if (!logFileModules) {
    return;
  }

  // MOZ_LOG files have a standard file extension appended.
  std::wstring logFileName(it->second);
  logFileName.append(WSTRING(MOZ_LOG_FILE_EXTENSION));

  // Allow for rotation number if rotate is on in the MOZ_LOG settings.
  bool rotate = false;
  NSPRLogModulesParser(
      logFileModules,
      [&rotate](const char* aName, LogLevel aLevel, int32_t aValue) {
        if (strcmp(aName, "rotate") == 0) {
          // Less or eq zero means to turn rotate off.
          rotate = aValue > 0;
        }
      });
  if (rotate) {
    logFileName.append(L".?");
  }

  // Allow for %PID token in the filename. We don't allow it in the dir path, if
  // specified, because we have to use a wildcard as we don't know the PID yet.
  auto pidPos = logFileName.find(WSTRING(MOZ_LOG_PID_TOKEN));
  auto lastSlash = logFileName.find_last_of(L"/\\");
  if (pidPos != std::wstring::npos &&
      (lastSlash == std::wstring::npos || lastSlash < pidPos)) {
    logFileName.replace(pidPos, strlen(MOZ_LOG_PID_TOKEN), L"*");
  }

  aPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_FILES,
                   sandbox::TargetPolicy::FILES_ALLOW_ANY, logFileName.c_str());
}

#undef WSTRING

bool SandboxBroker::LaunchApp(const wchar_t* aPath, const wchar_t* aArguments,
                              base::EnvironmentMap& aEnvironment,
                              GeckoProcessType aProcessType,
                              const bool aEnableLogging,
                              const IMAGE_THUNK_DATA* aCachedNtdllThunk,
                              void** aProcessHandle) {
  if (!sBrokerService || !mPolicy) {
    return false;
  }

  // Set stdout and stderr, to allow inheritance for logging.
  mPolicy->SetStdoutHandle(::GetStdHandle(STD_OUTPUT_HANDLE));
  mPolicy->SetStderrHandle(::GetStdHandle(STD_ERROR_HANDLE));

  // If logging enabled, set up the policy.
  if (aEnableLogging) {
    ApplyLoggingPolicy();
  }

#if defined(DEBUG)
  // Allow write access to TEMP directory in debug builds for logging purposes.
  // The path from GetTempPathW can have a length up to MAX_PATH + 1, including
  // the null, so we need MAX_PATH + 2, so we can add an * to the end.
  wchar_t tempPath[MAX_PATH + 2];
  uint32_t pathLen = ::GetTempPathW(MAX_PATH + 1, tempPath);
  if (pathLen > 0) {
    // GetTempPath path ends with \ and returns the length without the null.
    tempPath[pathLen] = L'*';
    tempPath[pathLen + 1] = L'\0';
    mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_FILES,
                     sandbox::TargetPolicy::FILES_ALLOW_ANY, tempPath);
  }
#endif

  // Enable the child process to write log files when setup
  AddMozLogRulesToPolicy(mPolicy, aEnvironment);

  // Create the sandboxed process
  PROCESS_INFORMATION targetInfo = {0};
  sandbox::ResultCode result;
  sandbox::ResultCode last_warning = sandbox::SBOX_ALL_OK;
  DWORD last_error = ERROR_SUCCESS;
  result = sBrokerService->SpawnTarget(aPath, aArguments, aEnvironment, mPolicy,
                                       &last_warning, &last_error, &targetInfo);
  if (sandbox::SBOX_ALL_OK != result) {
    nsAutoCString key;
    key.AppendASCII(XRE_GeckoProcessTypeToString(aProcessType));
    key.AppendLiteral("/0x");
    key.AppendInt(static_cast<uint32_t>(last_error), 16);

    // Only accumulate for each combination once per session.
    if (sLaunchErrors) {
      if (!sLaunchErrors->Contains(key)) {
        Telemetry::Accumulate(Telemetry::SANDBOX_FAILED_LAUNCH_KEYED, key,
                              result);
        sLaunchErrors->PutEntry(key);
      }
    } else {
      // If sLaunchErrors not created yet then always accumulate.
      Telemetry::Accumulate(Telemetry::SANDBOX_FAILED_LAUNCH_KEYED, key,
                            result);
    }

    LOG_E(
        "Failed (ResultCode %d) to SpawnTarget with last_error=%d, "
        "last_warning=%d",
        result, last_error, last_warning);

    return false;
  } else if (sandbox::SBOX_ALL_OK != last_warning) {
    // If there was a warning (but the result was still ok), log it and proceed.
    LOG_W("Warning on SpawnTarget with last_error=%d, last_warning=%d",
          last_error, last_warning);
  }

#ifdef MOZ_THUNDERBIRD
  // In Thunderbird, mInitDllBlocklistOOP is null, so InitDllBlocklistOOP would
  // hit MOZ_RELEASE_ASSERT.
  constexpr bool isThunderbird = true;
#else
  constexpr bool isThunderbird = false;
#endif

  if (!isThunderbird &&
      XRE_GetChildProcBinPathType(aProcessType) == BinPathType::Self) {
    RefPtr<DllServices> dllSvc(DllServices::Get());
    LauncherVoidResultWithLineInfo blocklistInitOk =
        dllSvc->InitDllBlocklistOOP(aPath, targetInfo.hProcess,
                                    aCachedNtdllThunk);
    if (blocklistInitOk.isErr()) {
      dllSvc->HandleLauncherError(blocklistInitOk.unwrapErr(),
                                  XRE_GeckoProcessTypeToString(aProcessType));
      LOG_E("InitDllBlocklistOOP failed at %s:%d with HRESULT 0x%08lX",
            blocklistInitOk.unwrapErr().mFile,
            blocklistInitOk.unwrapErr().mLine,
            blocklistInitOk.unwrapErr().mError.AsHResult());
      TerminateProcess(targetInfo.hProcess, 1);
      CloseHandle(targetInfo.hThread);
      CloseHandle(targetInfo.hProcess);
      return false;
    }
  } else {
    // Load the child executable as a datafile so that we can examine its
    // headers without doing a full load with dependencies and such.
    nsModuleHandle moduleHandle(
        ::LoadLibraryExW(aPath, nullptr, LOAD_LIBRARY_AS_DATAFILE));
    if (moduleHandle) {
      nt::CrossExecTransferManager transferMgr(targetInfo.hProcess,
                                               moduleHandle);
      if (!!transferMgr) {
        LauncherVoidResult importsRestored =
            RestoreImportDirectory(aPath, transferMgr);
        if (importsRestored.isErr()) {
          RefPtr<DllServices> dllSvc(DllServices::Get());
          dllSvc->HandleLauncherError(
              importsRestored.unwrapErr(),
              XRE_GeckoProcessTypeToString(aProcessType));
          LOG_E("Failed to restore import directory with HRESULT 0x%08lX",
                importsRestored.unwrapErr().mError.AsHResult());
          TerminateProcess(targetInfo.hProcess, 1);
          CloseHandle(targetInfo.hThread);
          CloseHandle(targetInfo.hProcess);
          return false;
        }
      }
    }
  }

  // The sandboxed process is started in a suspended state, resume it now that
  // we've set things up.
  ResumeThread(targetInfo.hThread);
  CloseHandle(targetInfo.hThread);

  // Return the process handle to the caller
  *aProcessHandle = targetInfo.hProcess;

  return true;
}

static void AddCachedDirRule(sandbox::TargetPolicy* aPolicy,
                             sandbox::TargetPolicy::Semantics aAccess,
                             const UniquePtr<nsString>& aBaseDir,
                             const nsLiteralString& aRelativePath) {
  if (!aBaseDir) {
    // This can only be an NS_WARNING, because it can null for xpcshell tests.
    NS_WARNING("Tried to add rule with null base dir.");
    LOG_E("Tried to add rule with null base dir. Relative path: %S, Access: %d",
          aRelativePath.get(), aAccess);
    return;
  }

  nsAutoString rulePath(*aBaseDir);
  rulePath.Append(aRelativePath);

  sandbox::ResultCode result = aPolicy->AddRule(
      sandbox::TargetPolicy::SUBSYS_FILES, aAccess, rulePath.get());
  if (sandbox::SBOX_ALL_OK != result) {
    NS_ERROR("Failed to add file policy rule.");
    LOG_E("Failed (ResultCode %d) to add %d access to: %S", result, aAccess,
          rulePath.get());
  }
}

// This function caches and returns an array of NT paths of the executable's
// dependent modules.
// If this returns Nothing(), it means the retrieval of the modules failed
// (e.g. when the launcher process is disabled), so the process should not
// enable pre-spawn CIG.
static const Maybe<Vector<const wchar_t*>>& GetPrespawnCigExceptionModules() {
  // sDependentModules points to a shared section created in the launcher
  // process and the mapped address is static in each process, so we cache
  // it as a static variable instead of retrieving it every time.
  static Maybe<Vector<const wchar_t*>> sDependentModules =
      []() -> Maybe<Vector<const wchar_t*>> {
    using GetDependentModulePathsFn = const wchar_t* (*)();
    GetDependentModulePathsFn getDependentModulePaths =
        reinterpret_cast<GetDependentModulePathsFn>(::GetProcAddress(
            ::GetModuleHandleW(nullptr), "GetDependentModulePaths"));
    if (!getDependentModulePaths) {
      return Nothing();
    }

    const wchar_t* arrayBase = getDependentModulePaths();
    if (!arrayBase) {
      return Nothing();
    }

    // Convert a null-delimited string set to a string vector.
    Vector<const wchar_t*> paths;
    for (const wchar_t* p = arrayBase; *p;) {
      Unused << paths.append(p);
      while (*p) {
        ++p;
      }
      ++p;
    }

    return Some(std::move(paths));
  }();

  return sDependentModules;
}

static sandbox::ResultCode InitSignedPolicyRulesToBypassCig(
    sandbox::TargetPolicy* aPolicy,
    const Vector<const wchar_t*>& aExceptionModules) {
  // Allow modules in the directory containing the executable such as
  // mozglue.dll, nss3.dll, etc.
  nsAutoString rulePath(*sBinDir);
  rulePath.Append(u"\\*");
  auto result = aPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_SIGNED_BINARY,
                                 sandbox::TargetPolicy::SIGNED_ALLOW_LOAD,
                                 rulePath.get());
  if (result != sandbox::SBOX_ALL_OK) {
    return result;
  }

  if (aExceptionModules.empty()) {
    return sandbox::SBOX_ALL_OK;
  }

  for (const wchar_t* path : aExceptionModules) {
    result = aPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_SIGNED_BINARY,
                              sandbox::TargetPolicy::SIGNED_ALLOW_LOAD, path);
    if (result != sandbox::SBOX_ALL_OK) {
      return result;
    }
  }

  return sandbox::SBOX_ALL_OK;
}

// Checks whether we can use a job object as part of the sandbox.
static bool CanUseJob() {
  // Windows 8 and later allows nested jobs, no need for further checks.
  if (IsWin8OrLater()) {
    return true;
  }

  BOOL inJob = true;
  // If we can't determine if we are in a job then assume we can use one.
  if (!::IsProcessInJob(::GetCurrentProcess(), nullptr, &inJob)) {
    return true;
  }

  // If there is no job then we are fine to use one.
  if (!inJob) {
    return true;
  }

  JOBOBJECT_EXTENDED_LIMIT_INFORMATION job_info = {};
  // If we can't get the job object flags then again assume we can use a job.
  if (!::QueryInformationJobObject(nullptr, JobObjectExtendedLimitInformation,
                                   &job_info, sizeof(job_info), nullptr)) {
    return true;
  }

  // If we can break away from the current job then we are free to set our own.
  if (job_info.BasicLimitInformation.LimitFlags &
      JOB_OBJECT_LIMIT_BREAKAWAY_OK) {
    return true;
  }

  // Chromium added a command line flag to allow no job to be used, which was
  // originally supposed to only be used for remote sessions. If you use runas
  // to start Firefox then this also uses a separate job and we would fail to
  // start on Windows 7. An unknown number of people use (or used to use) runas
  // with Firefox for some security benefits (see bug 1228880). This is now a
  // counterproductive technique, but allowing both the remote and local case
  // for now and adding telemetry to see if we can restrict this to just remote.
  nsAutoString localRemote(::GetSystemMetrics(SM_REMOTESESSION) ? u"remote"
                                                                : u"local");
  Telemetry::ScalarSet(Telemetry::ScalarID::SANDBOX_NO_JOB, localRemote, true);

  // Allow running without the job object in this case. This slightly reduces
  // the ability of the sandbox to protect its children from spawning new
  // processes or preventing them from shutting down Windows or accessing the
  // clipboard.
  return false;
}

static sandbox::ResultCode SetJobLevel(sandbox::TargetPolicy* aPolicy,
                                       sandbox::JobLevel aJobLevel,
                                       uint32_t aUiExceptions) {
  static bool sCanUseJob = CanUseJob();
  if (sCanUseJob) {
    return aPolicy->SetJobLevel(aJobLevel, aUiExceptions);
  }

  return aPolicy->SetJobLevel(sandbox::JOB_NONE, 0);
}

void SandboxBroker::SetSecurityLevelForContentProcess(int32_t aSandboxLevel,
                                                      bool aIsFileProcess) {
  MOZ_RELEASE_ASSERT(mPolicy, "mPolicy must be set before this call.");

  sandbox::JobLevel jobLevel;
  sandbox::TokenLevel accessTokenLevel;
  sandbox::IntegrityLevel initialIntegrityLevel;
  sandbox::IntegrityLevel delayedIntegrityLevel;

  // The setting of these levels is pretty arbitrary, but they are a useful (if
  // crude) tool while we are tightening the policy. Gaps are left to try and
  // avoid changing their meaning.
  MOZ_RELEASE_ASSERT(aSandboxLevel >= 1,
                     "Should not be called with aSandboxLevel < 1");
  if (aSandboxLevel >= 20) {
    jobLevel = sandbox::JOB_LOCKDOWN;
    accessTokenLevel = sandbox::USER_LOCKDOWN;
    initialIntegrityLevel = sandbox::INTEGRITY_LEVEL_LOW;
    delayedIntegrityLevel = sandbox::INTEGRITY_LEVEL_UNTRUSTED;
  } else if (aSandboxLevel >= 4) {
    jobLevel = sandbox::JOB_LOCKDOWN;
    accessTokenLevel = sandbox::USER_LIMITED;
    initialIntegrityLevel = sandbox::INTEGRITY_LEVEL_LOW;
    delayedIntegrityLevel = sandbox::INTEGRITY_LEVEL_LOW;
  } else if (aSandboxLevel >= 3) {
    jobLevel = sandbox::JOB_RESTRICTED;
    accessTokenLevel = sandbox::USER_LIMITED;
    initialIntegrityLevel = sandbox::INTEGRITY_LEVEL_LOW;
    delayedIntegrityLevel = sandbox::INTEGRITY_LEVEL_LOW;
  } else if (aSandboxLevel == 2) {
    jobLevel = sandbox::JOB_INTERACTIVE;
    accessTokenLevel = sandbox::USER_INTERACTIVE;
    initialIntegrityLevel = sandbox::INTEGRITY_LEVEL_LOW;
    delayedIntegrityLevel = sandbox::INTEGRITY_LEVEL_LOW;
  } else {
    MOZ_ASSERT(aSandboxLevel == 1);

    jobLevel = sandbox::JOB_NONE;
    accessTokenLevel = sandbox::USER_NON_ADMIN;
    initialIntegrityLevel = sandbox::INTEGRITY_LEVEL_LOW;
    delayedIntegrityLevel = sandbox::INTEGRITY_LEVEL_LOW;
  }

  // If the process will handle file: URLs, don't allow settings that
  // block reads.
  if (aIsFileProcess) {
    if (accessTokenLevel < sandbox::USER_NON_ADMIN) {
      accessTokenLevel = sandbox::USER_NON_ADMIN;
    }
    if (delayedIntegrityLevel > sandbox::INTEGRITY_LEVEL_LOW) {
      delayedIntegrityLevel = sandbox::INTEGRITY_LEVEL_LOW;
    }
  }

#if defined(DEBUG)
  // This is required for a MOZ_ASSERT check in WindowsMessageLoop.cpp
  // WinEventHook, see bug 1366694 for details.
  DWORD uiExceptions = JOB_OBJECT_UILIMIT_HANDLES;
#else
  DWORD uiExceptions = 0;
#endif
  sandbox::ResultCode result = SetJobLevel(mPolicy, jobLevel, uiExceptions);
  MOZ_RELEASE_ASSERT(sandbox::SBOX_ALL_OK == result,
                     "Setting job level failed, have you set memory limit when "
                     "jobLevel == JOB_NONE?");

  // If the delayed access token is not restricted we don't want the initial one
  // to be either, because it can interfere with running from a network drive.
  sandbox::TokenLevel initialAccessTokenLevel =
      (accessTokenLevel == sandbox::USER_UNPROTECTED ||
       accessTokenLevel == sandbox::USER_NON_ADMIN)
          ? sandbox::USER_UNPROTECTED
          : sandbox::USER_RESTRICTED_SAME_ACCESS;

  result = mPolicy->SetTokenLevel(initialAccessTokenLevel, accessTokenLevel);
  MOZ_RELEASE_ASSERT(sandbox::SBOX_ALL_OK == result,
                     "Lockdown level cannot be USER_UNPROTECTED or USER_LAST "
                     "if initial level was USER_RESTRICTED_SAME_ACCESS");

  result = mPolicy->SetIntegrityLevel(initialIntegrityLevel);
  MOZ_RELEASE_ASSERT(sandbox::SBOX_ALL_OK == result,
                     "SetIntegrityLevel should never fail, what happened?");
  result = mPolicy->SetDelayedIntegrityLevel(delayedIntegrityLevel);
  MOZ_RELEASE_ASSERT(
      sandbox::SBOX_ALL_OK == result,
      "SetDelayedIntegrityLevel should never fail, what happened?");

  if (aSandboxLevel > 5) {
    mPolicy->SetLockdownDefaultDacl();
    mPolicy->AddRestrictingRandomSid();
  }

  if (aSandboxLevel > 4) {
    result = mPolicy->SetAlternateDesktop(false);
    if (NS_WARN_IF(result != sandbox::SBOX_ALL_OK)) {
      LOG_W("SetAlternateDesktop failed, result: %i, last error: %x", result,
            ::GetLastError());
    }
  }

  sandbox::MitigationFlags mitigations =
      sandbox::MITIGATION_BOTTOM_UP_ASLR | sandbox::MITIGATION_HEAP_TERMINATE |
      sandbox::MITIGATION_SEHOP | sandbox::MITIGATION_DEP_NO_ATL_THUNK |
      sandbox::MITIGATION_DEP | sandbox::MITIGATION_EXTENSION_POINT_DISABLE |
      sandbox::MITIGATION_IMAGE_LOAD_PREFER_SYS32;

#if defined(_M_ARM64)
  // Disable CFG on older versions of ARM64 Windows to avoid a crash in COM.
  if (!IsWin10Sep2018UpdateOrLater()) {
    mitigations |= sandbox::MITIGATION_CONTROL_FLOW_GUARD_DISABLE;
  }
#endif

  if (aSandboxLevel > 3) {
    // If we're running from a network drive then we can't block loading from
    // remote locations. Strangely using MITIGATION_IMAGE_LOAD_NO_LOW_LABEL in
    // this situation also means the process fails to start (bug 1423296).
    if (!sRunningFromNetworkDrive) {
      mitigations |= sandbox::MITIGATION_IMAGE_LOAD_NO_REMOTE |
                     sandbox::MITIGATION_IMAGE_LOAD_NO_LOW_LABEL;
    }
  }

  if (StaticPrefs::security_sandbox_content_shadow_stack_enabled()) {
    mitigations |= sandbox::MITIGATION_CET_COMPAT_MODE;
  }

  result = mPolicy->SetProcessMitigations(mitigations);
  MOZ_RELEASE_ASSERT(sandbox::SBOX_ALL_OK == result,
                     "Invalid flags for SetProcessMitigations.");

  ContentWin32kLockdownState win32kLockdownState =
      GetContentWin32kLockdownState();

  LOG_W("Win32k Lockdown State: '%s'",
        ContentWin32kLockdownStateToString(win32kLockdownState));

  // The file content process has some win32k usage particular to it, for
  // example at least moz-icon usage, but we don't want to block enabling for
  // other content processes. We might want to use moz-icon in the privileged
  // about content process in the future, so we would need to exclude that as
  // well or remote moz-icon.
  if (!aIsFileProcess &&
      (win32kLockdownState == ContentWin32kLockdownState::LockdownEnabled)) {
    result = AddWin32kLockdownPolicy(mPolicy, false);
    MOZ_RELEASE_ASSERT(result == sandbox::SBOX_ALL_OK,
                       "Failed to add the win32k lockdown policy");
  }

  mitigations = sandbox::MITIGATION_STRICT_HANDLE_CHECKS |
                sandbox::MITIGATION_DLL_SEARCH_ORDER;

  result = mPolicy->SetDelayedProcessMitigations(mitigations);
  MOZ_RELEASE_ASSERT(sandbox::SBOX_ALL_OK == result,
                     "Invalid flags for SetDelayedProcessMitigations.");

  // Add rule to allow read / write access to content temp dir. If for some
  // reason the addition of the content temp failed, this will give write access
  // to the normal TEMP dir. However such failures should be pretty rare and
  // without this printing will not currently work.
  AddCachedDirRule(mPolicy, sandbox::TargetPolicy::FILES_ALLOW_ANY,
                   sContentTempDir, u"\\*"_ns);

  // We still have edge cases where the child at low integrity can't read some
  // files, so add a rule to allow read access to everything when required.
  if (aSandboxLevel == 1 || aIsFileProcess) {
    result =
        mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_FILES,
                         sandbox::TargetPolicy::FILES_ALLOW_READONLY, L"*");
    MOZ_RELEASE_ASSERT(sandbox::SBOX_ALL_OK == result,
                       "With these static arguments AddRule should never fail, "
                       "what happened?");
  } else {
    // Add rule to allow access to user specific fonts.
    AddCachedDirRule(mPolicy, sandbox::TargetPolicy::FILES_ALLOW_READONLY,
                     sLocalAppDataDir, u"\\Microsoft\\Windows\\Fonts\\*"_ns);

    // Add rule to allow read access to installation directory.
    AddCachedDirRule(mPolicy, sandbox::TargetPolicy::FILES_ALLOW_READONLY,
                     sBinDir, u"\\*"_ns);

    // Add rule to allow read access to the chrome directory within profile.
    AddCachedDirRule(mPolicy, sandbox::TargetPolicy::FILES_ALLOW_READONLY,
                     sProfileDir, u"\\chrome\\*"_ns);

    // Add rule to allow read access to the extensions directory within profile.
    AddCachedDirRule(mPolicy, sandbox::TargetPolicy::FILES_ALLOW_READONLY,
                     sProfileDir, u"\\extensions\\*"_ns);

#ifdef ENABLE_SYSTEM_EXTENSION_DIRS
    // Add rule to allow read access to the per-user extensions directory.
    AddCachedDirRule(mPolicy, sandbox::TargetPolicy::FILES_ALLOW_READONLY,
                     sUserExtensionsDir, u"\\*"_ns);
#endif
  }

  // Add the policy for the client side of a pipe. It is just a file
  // in the \pipe\ namespace. We restrict it to pipes that start with
  // "chrome." so the sandboxed process cannot connect to system services.
  result = mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_FILES,
                            sandbox::TargetPolicy::FILES_ALLOW_ANY,
                            L"\\??\\pipe\\chrome.*");
  MOZ_RELEASE_ASSERT(
      sandbox::SBOX_ALL_OK == result,
      "With these static arguments AddRule should never fail, what happened?");

  // Add the policy for the client side of the crash server pipe.
  result = mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_FILES,
                            sandbox::TargetPolicy::FILES_ALLOW_ANY,
                            L"\\??\\pipe\\gecko-crash-server-pipe.*");
  MOZ_RELEASE_ASSERT(
      sandbox::SBOX_ALL_OK == result,
      "With these static arguments AddRule should never fail, what happened?");

  // The content process needs to be able to duplicate named pipes back to the
  // broker and other child processes, which are File type handles.
  result = mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_HANDLES,
                            sandbox::TargetPolicy::HANDLES_DUP_BROKER, L"File");
  MOZ_RELEASE_ASSERT(
      sandbox::SBOX_ALL_OK == result,
      "With these static arguments AddRule should never fail, what happened?");
  result = mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_HANDLES,
                            sandbox::TargetPolicy::HANDLES_DUP_ANY, L"File");
  MOZ_RELEASE_ASSERT(
      sandbox::SBOX_ALL_OK == result,
      "With these static arguments AddRule should never fail, what happened?");

  // The content process needs to be able to duplicate shared memory handles,
  // which are Section handles, to the broker process and other child processes.
  result =
      mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_HANDLES,
                       sandbox::TargetPolicy::HANDLES_DUP_BROKER, L"Section");
  MOZ_RELEASE_ASSERT(
      sandbox::SBOX_ALL_OK == result,
      "With these static arguments AddRule should never fail, what happened?");
  result = mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_HANDLES,
                            sandbox::TargetPolicy::HANDLES_DUP_ANY, L"Section");
  MOZ_RELEASE_ASSERT(
      sandbox::SBOX_ALL_OK == result,
      "With these static arguments AddRule should never fail, what happened?");

  // The content process needs to be able to duplicate semaphore handles,
  // to the broker process and other child processes.
  result =
      mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_HANDLES,
                       sandbox::TargetPolicy::HANDLES_DUP_BROKER, L"Semaphore");
  MOZ_RELEASE_ASSERT(
      sandbox::SBOX_ALL_OK == result,
      "With these static arguments AddRule should never fail, what happened?");
  result =
      mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_HANDLES,
                       sandbox::TargetPolicy::HANDLES_DUP_ANY, L"Semaphore");
  MOZ_RELEASE_ASSERT(
      sandbox::SBOX_ALL_OK == result,
      "With these static arguments AddRule should never fail, what happened?");

  // Allow content processes to use complex line breaking brokering.
  result = mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_LINE_BREAK,
                            sandbox::TargetPolicy::LINE_BREAK_ALLOW, nullptr);
  MOZ_RELEASE_ASSERT(
      sandbox::SBOX_ALL_OK == result,
      "With these static arguments AddRule should never fail, what happened?");

  if (aSandboxLevel >= 20) {
    // Content process still needs to be able to read fonts.
    wchar_t* fontsPath;
    if (SUCCEEDED(
            ::SHGetKnownFolderPath(FOLDERID_Fonts, 0, nullptr, &fontsPath))) {
      std::wstring fontsStr = fontsPath;
      ::CoTaskMemFree(fontsPath);
      result = mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_FILES,
                                sandbox::TargetPolicy::FILES_ALLOW_READONLY,
                                fontsStr.c_str());
      if (sandbox::SBOX_ALL_OK != result) {
        NS_ERROR("Failed to add fonts dir read access policy rule.");
        LOG_E("Failed (ResultCode %d) to add read access to: %S", result,
              fontsStr.c_str());
      }

      fontsStr += L"\\*";
      result = mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_FILES,
                                sandbox::TargetPolicy::FILES_ALLOW_READONLY,
                                fontsStr.c_str());
      if (sandbox::SBOX_ALL_OK != result) {
        NS_ERROR("Failed to add fonts read access policy rule.");
        LOG_E("Failed (ResultCode %d) to add read access to: %S", result,
              fontsStr.c_str());
      }
    }

    // We still currently create IPC named pipes in the content process.
    result = mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_NAMED_PIPES,
                              sandbox::TargetPolicy::NAMEDPIPES_ALLOW_ANY,
                              L"\\\\.\\pipe\\chrome.*");
    MOZ_RELEASE_ASSERT(
        sandbox::SBOX_ALL_OK == result,
        "With these static arguments AddRule should never fail.");
  }
}

void SandboxBroker::SetSecurityLevelForGPUProcess(
    int32_t aSandboxLevel, const nsCOMPtr<nsIFile>& aProfileDir) {
  MOZ_RELEASE_ASSERT(mPolicy, "mPolicy must be set before this call.");

  sandbox::JobLevel jobLevel;
  sandbox::TokenLevel accessTokenLevel;
  sandbox::IntegrityLevel initialIntegrityLevel;
  sandbox::IntegrityLevel delayedIntegrityLevel;

  // The setting of these levels is pretty arbitrary, but they are a useful (if
  // crude) tool while we are tightening the policy. Gaps are left to try and
  // avoid changing their meaning.
  if (aSandboxLevel >= 2) {
    jobLevel = sandbox::JOB_NONE;
    accessTokenLevel = sandbox::USER_LIMITED;
    initialIntegrityLevel = sandbox::INTEGRITY_LEVEL_LOW;
    delayedIntegrityLevel = sandbox::INTEGRITY_LEVEL_LOW;
  } else {
    MOZ_RELEASE_ASSERT(aSandboxLevel >= 1,
                       "Should not be called with aSandboxLevel < 1");
    jobLevel = sandbox::JOB_NONE;
    accessTokenLevel = sandbox::USER_NON_ADMIN;
    initialIntegrityLevel = sandbox::INTEGRITY_LEVEL_LOW;
    delayedIntegrityLevel = sandbox::INTEGRITY_LEVEL_LOW;
  }

  sandbox::ResultCode result =
      SetJobLevel(mPolicy, jobLevel, 0 /* ui_exceptions */);
  MOZ_RELEASE_ASSERT(sandbox::SBOX_ALL_OK == result,
                     "Setting job level failed, have you set memory limit when "
                     "jobLevel == JOB_NONE?");

  // If the delayed access token is not restricted we don't want the initial one
  // to be either, because it can interfere with running from a network drive.
  sandbox::TokenLevel initialAccessTokenLevel =
      (accessTokenLevel == sandbox::USER_UNPROTECTED ||
       accessTokenLevel == sandbox::USER_NON_ADMIN)
          ? sandbox::USER_UNPROTECTED
          : sandbox::USER_RESTRICTED_SAME_ACCESS;

  result = mPolicy->SetTokenLevel(initialAccessTokenLevel, accessTokenLevel);
  MOZ_RELEASE_ASSERT(sandbox::SBOX_ALL_OK == result,
                     "Lockdown level cannot be USER_UNPROTECTED or USER_LAST "
                     "if initial level was USER_RESTRICTED_SAME_ACCESS");

  result = mPolicy->SetIntegrityLevel(initialIntegrityLevel);
  MOZ_RELEASE_ASSERT(sandbox::SBOX_ALL_OK == result,
                     "SetIntegrityLevel should never fail, what happened?");
  result = mPolicy->SetDelayedIntegrityLevel(delayedIntegrityLevel);
  MOZ_RELEASE_ASSERT(
      sandbox::SBOX_ALL_OK == result,
      "SetDelayedIntegrityLevel should never fail, what happened?");

  mPolicy->SetLockdownDefaultDacl();
  mPolicy->AddRestrictingRandomSid();

  sandbox::MitigationFlags mitigations =
      sandbox::MITIGATION_BOTTOM_UP_ASLR | sandbox::MITIGATION_HEAP_TERMINATE |
      sandbox::MITIGATION_SEHOP | sandbox::MITIGATION_DEP_NO_ATL_THUNK |
      sandbox::MITIGATION_DEP;

  if (StaticPrefs::security_sandbox_gpu_shadow_stack_enabled()) {
    mitigations |= sandbox::MITIGATION_CET_COMPAT_MODE;
  }

  result = mPolicy->SetProcessMitigations(mitigations);
  MOZ_RELEASE_ASSERT(sandbox::SBOX_ALL_OK == result,
                     "Invalid flags for SetProcessMitigations.");

  mitigations = sandbox::MITIGATION_STRICT_HANDLE_CHECKS |
                sandbox::MITIGATION_DLL_SEARCH_ORDER;

  result = mPolicy->SetDelayedProcessMitigations(mitigations);
  MOZ_RELEASE_ASSERT(sandbox::SBOX_ALL_OK == result,
                     "Invalid flags for SetDelayedProcessMitigations.");

  // Add the policy for the client side of a pipe. It is just a file
  // in the \pipe\ namespace. We restrict it to pipes that start with
  // "chrome." so the sandboxed process cannot connect to system services.
  result = mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_FILES,
                            sandbox::TargetPolicy::FILES_ALLOW_ANY,
                            L"\\??\\pipe\\chrome.*");
  MOZ_RELEASE_ASSERT(
      sandbox::SBOX_ALL_OK == result,
      "With these static arguments AddRule should never fail, what happened?");

  // Add the policy for the client side of the crash server pipe.
  result = mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_FILES,
                            sandbox::TargetPolicy::FILES_ALLOW_ANY,
                            L"\\??\\pipe\\gecko-crash-server-pipe.*");
  MOZ_RELEASE_ASSERT(
      sandbox::SBOX_ALL_OK == result,
      "With these static arguments AddRule should never fail, what happened?");

  // The GPU process needs to write to a shader cache for performance reasons
  // Note that we can't use the sProfileDir variable stored above because
  // the GPU process is created very early in Gecko initialization before
  // SandboxBroker::GeckoDependentInitialize() is called
  if (aProfileDir) {
    if (![&aProfileDir, this] {
          nsString shaderCacheRulePath;
          nsresult rv = aProfileDir->GetPath(shaderCacheRulePath);
          if (NS_FAILED(rv)) {
            return false;
          }
          if (shaderCacheRulePath.IsEmpty()) {
            return false;
          }

          if (Substring(shaderCacheRulePath, 0, 2).Equals(u"\\\\")) {
            shaderCacheRulePath.InsertLiteral(u"??\\UNC", 1);
          }

          shaderCacheRulePath.Append(u"\\shader-cache");

          sandbox::ResultCode result =
              mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_FILES,
                               sandbox::TargetPolicy::FILES_ALLOW_DIR_ANY,
                               shaderCacheRulePath.get());

          if (result != sandbox::SBOX_ALL_OK) {
            return false;
          }

          shaderCacheRulePath.Append(u"\\*");

          result = mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_FILES,
                                    sandbox::TargetPolicy::FILES_ALLOW_ANY,
                                    shaderCacheRulePath.get());

          if (result != sandbox::SBOX_ALL_OK) {
            return false;
          }

          return true;
        }()) {
      NS_WARNING(
          "Failed to add rule enabling GPU shader cache. Performance will be "
          "negatively affected");
    }
  }

  // The process needs to be able to duplicate shared memory handles,
  // which are Section handles, to the broker process and other child processes.
  result =
      mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_HANDLES,
                       sandbox::TargetPolicy::HANDLES_DUP_BROKER, L"Section");
  MOZ_RELEASE_ASSERT(
      sandbox::SBOX_ALL_OK == result,
      "With these static arguments AddRule should never fail, what happened?");
  result = mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_HANDLES,
                            sandbox::TargetPolicy::HANDLES_DUP_ANY, L"Section");
  MOZ_RELEASE_ASSERT(
      sandbox::SBOX_ALL_OK == result,
      "With these static arguments AddRule should never fail, what happened?");
}

#define SANDBOX_ENSURE_SUCCESS(result, message)          \
  do {                                                   \
    MOZ_ASSERT(sandbox::SBOX_ALL_OK == result, message); \
    if (sandbox::SBOX_ALL_OK != result) return false;    \
  } while (0)

bool SandboxBroker::SetSecurityLevelForRDDProcess() {
  if (!mPolicy) {
    return false;
  }

  auto result =
      SetJobLevel(mPolicy, sandbox::JOB_LOCKDOWN, 0 /* ui_exceptions */);
  SANDBOX_ENSURE_SUCCESS(
      result,
      "SetJobLevel should never fail with these arguments, what happened?");

  result = mPolicy->SetTokenLevel(sandbox::USER_RESTRICTED_SAME_ACCESS,
                                  sandbox::USER_LIMITED);
  SANDBOX_ENSURE_SUCCESS(
      result,
      "SetTokenLevel should never fail with these arguments, what happened?");

  result = mPolicy->SetAlternateDesktop(true);
  if (NS_WARN_IF(result != sandbox::SBOX_ALL_OK)) {
    LOG_W("SetAlternateDesktop failed, result: %i, last error: %x", result,
          ::GetLastError());
  }

  result = mPolicy->SetIntegrityLevel(sandbox::INTEGRITY_LEVEL_LOW);
  SANDBOX_ENSURE_SUCCESS(result,
                         "SetIntegrityLevel should never fail with these "
                         "arguments, what happened?");

  result = mPolicy->SetDelayedIntegrityLevel(sandbox::INTEGRITY_LEVEL_LOW);
  SANDBOX_ENSURE_SUCCESS(result,
                         "SetDelayedIntegrityLevel should never fail with "
                         "these arguments, what happened?");

  mPolicy->SetLockdownDefaultDacl();
  mPolicy->AddRestrictingRandomSid();

  sandbox::MitigationFlags mitigations =
      sandbox::MITIGATION_BOTTOM_UP_ASLR | sandbox::MITIGATION_HEAP_TERMINATE |
      sandbox::MITIGATION_SEHOP | sandbox::MITIGATION_EXTENSION_POINT_DISABLE |
      sandbox::MITIGATION_DEP_NO_ATL_THUNK | sandbox::MITIGATION_DEP |
      sandbox::MITIGATION_IMAGE_LOAD_PREFER_SYS32;

  const Maybe<Vector<const wchar_t*>>& exceptionModules =
      GetPrespawnCigExceptionModules();
  if (exceptionModules.isSome()) {
    mitigations |= sandbox::MITIGATION_FORCE_MS_SIGNED_BINS;
  }

  if (StaticPrefs::security_sandbox_rdd_shadow_stack_enabled()) {
    mitigations |= sandbox::MITIGATION_CET_COMPAT_MODE;
  }

  result = mPolicy->SetProcessMitigations(mitigations);
  SANDBOX_ENSURE_SUCCESS(result, "Invalid flags for SetProcessMitigations.");

  if (exceptionModules.isSome()) {
    // This needs to be called after MITIGATION_FORCE_MS_SIGNED_BINS is set
    // because of DCHECK in PolicyBase::AddRuleInternal.
    result = InitSignedPolicyRulesToBypassCig(mPolicy, exceptionModules.ref());
    SANDBOX_ENSURE_SUCCESS(result, "Failed to initialize signed policy rules.");
  }

  mitigations = sandbox::MITIGATION_STRICT_HANDLE_CHECKS |
                sandbox::MITIGATION_DLL_SEARCH_ORDER;

  if (exceptionModules.isNothing()) {
    mitigations |= sandbox::MITIGATION_FORCE_MS_SIGNED_BINS;
  }

  result = mPolicy->SetDelayedProcessMitigations(mitigations);
  SANDBOX_ENSURE_SUCCESS(result,
                         "Invalid flags for SetDelayedProcessMitigations.");

  // Add the policy for the client side of a pipe. It is just a file
  // in the \pipe\ namespace. We restrict it to pipes that start with
  // "chrome." so the sandboxed process cannot connect to system services.
  result = mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_FILES,
                            sandbox::TargetPolicy::FILES_ALLOW_ANY,
                            L"\\??\\pipe\\chrome.*");
  SANDBOX_ENSURE_SUCCESS(
      result,
      "With these static arguments AddRule should never fail, what happened?");

  // Add the policy for the client side of the crash server pipe.
  result = mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_FILES,
                            sandbox::TargetPolicy::FILES_ALLOW_ANY,
                            L"\\??\\pipe\\gecko-crash-server-pipe.*");
  SANDBOX_ENSURE_SUCCESS(
      result,
      "With these static arguments AddRule should never fail, what happened?");

  // The process needs to be able to duplicate shared memory handles,
  // which are Section handles, to the content processes.
  result = mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_HANDLES,
                            sandbox::TargetPolicy::HANDLES_DUP_ANY, L"Section");
  SANDBOX_ENSURE_SUCCESS(
      result,
      "With these static arguments AddRule should never fail, what happened?");

  // This section is needed to avoid an assert during crash reporting code
  // when running mochitests.  The assertion is here:
  // toolkit/crashreporter/nsExceptionHandler.cpp:2041
  result =
      mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_HANDLES,
                       sandbox::TargetPolicy::HANDLES_DUP_BROKER, L"Section");
  SANDBOX_ENSURE_SUCCESS(
      result,
      "With these static arguments AddRule should never fail, what happened?");

  return true;
}

bool SandboxBroker::SetSecurityLevelForSocketProcess() {
  if (!mPolicy) {
    return false;
  }

  auto result =
      SetJobLevel(mPolicy, sandbox::JOB_LOCKDOWN, 0 /* ui_exceptions */);
  SANDBOX_ENSURE_SUCCESS(
      result,
      "SetJobLevel should never fail with these arguments, what happened?");

  result = mPolicy->SetTokenLevel(sandbox::USER_RESTRICTED_SAME_ACCESS,
                                  sandbox::USER_LIMITED);
  SANDBOX_ENSURE_SUCCESS(
      result,
      "SetTokenLevel should never fail with these arguments, what happened?");

  result = mPolicy->SetAlternateDesktop(true);
  if (NS_WARN_IF(result != sandbox::SBOX_ALL_OK)) {
    LOG_W("SetAlternateDesktop failed, result: %i, last error: %x", result,
          ::GetLastError());
  }

  result = mPolicy->SetIntegrityLevel(sandbox::INTEGRITY_LEVEL_LOW);
  SANDBOX_ENSURE_SUCCESS(result,
                         "SetIntegrityLevel should never fail with these "
                         "arguments, what happened?");

  result =
      mPolicy->SetDelayedIntegrityLevel(sandbox::INTEGRITY_LEVEL_UNTRUSTED);
  SANDBOX_ENSURE_SUCCESS(result,
                         "SetDelayedIntegrityLevel should never fail with "
                         "these arguments, what happened?");

  mPolicy->SetLockdownDefaultDacl();
  mPolicy->AddRestrictingRandomSid();

  sandbox::MitigationFlags mitigations =
      sandbox::MITIGATION_BOTTOM_UP_ASLR | sandbox::MITIGATION_HEAP_TERMINATE |
      sandbox::MITIGATION_SEHOP | sandbox::MITIGATION_EXTENSION_POINT_DISABLE |
      sandbox::MITIGATION_DEP_NO_ATL_THUNK | sandbox::MITIGATION_DEP |
      sandbox::MITIGATION_IMAGE_LOAD_PREFER_SYS32;

  if (StaticPrefs::security_sandbox_socket_shadow_stack_enabled()) {
    mitigations |= sandbox::MITIGATION_CET_COMPAT_MODE;
  }

  result = mPolicy->SetProcessMitigations(mitigations);
  SANDBOX_ENSURE_SUCCESS(result, "Invalid flags for SetProcessMitigations.");

  if (StaticPrefs::security_sandbox_socket_win32k_disable()) {
    result = AddWin32kLockdownPolicy(mPolicy, false);
    SANDBOX_ENSURE_SUCCESS(result, "Failed to add the win32k lockdown policy");
  }

  mitigations = sandbox::MITIGATION_STRICT_HANDLE_CHECKS |
                sandbox::MITIGATION_DLL_SEARCH_ORDER |
                sandbox::MITIGATION_FORCE_MS_SIGNED_BINS;

  // TODO: MITIGATION_DYNAMIC_CODE_DISABLE will be always added to mitigations
  // in bug 1734470.
  if (!StaticPrefs::network_proxy_parse_pac_on_socket_process()) {
    mitigations |= sandbox::MITIGATION_DYNAMIC_CODE_DISABLE;
  }

  result = mPolicy->SetDelayedProcessMitigations(mitigations);
  SANDBOX_ENSURE_SUCCESS(result,
                         "Invalid flags for SetDelayedProcessMitigations.");

  // Add the policy for the client side of a pipe. It is just a file
  // in the \pipe\ namespace. We restrict it to pipes that start with
  // "chrome." so the sandboxed process cannot connect to system services.
  result = mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_FILES,
                            sandbox::TargetPolicy::FILES_ALLOW_ANY,
                            L"\\??\\pipe\\chrome.*");
  SANDBOX_ENSURE_SUCCESS(
      result,
      "With these static arguments AddRule should never fail, what happened?");

  // Add the policy for the client side of the crash server pipe.
  result = mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_FILES,
                            sandbox::TargetPolicy::FILES_ALLOW_ANY,
                            L"\\??\\pipe\\gecko-crash-server-pipe.*");
  SANDBOX_ENSURE_SUCCESS(
      result,
      "With these static arguments AddRule should never fail, what happened?");

  // This section is needed to avoid an assert during crash reporting code
  // when running mochitests.  The assertion is here:
  // toolkit/crashreporter/nsExceptionHandler.cpp:2041
  result =
      mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_HANDLES,
                       sandbox::TargetPolicy::HANDLES_DUP_BROKER, L"Section");
  SANDBOX_ENSURE_SUCCESS(
      result,
      "With these static arguments AddRule should never fail, what happened?");

  return true;
}

bool SandboxBroker::SetSecurityLevelForGMPlugin(SandboxLevel aLevel,
                                                bool aIsRemoteLaunch) {
  if (!mPolicy) {
    return false;
  }

  auto result =
      SetJobLevel(mPolicy, sandbox::JOB_LOCKDOWN, 0 /* ui_exceptions */);
  SANDBOX_ENSURE_SUCCESS(
      result,
      "SetJobLevel should never fail with these arguments, what happened?");
  auto level = (aLevel == Restricted) ? sandbox::USER_RESTRICTED
                                      : sandbox::USER_LOCKDOWN;
  result = mPolicy->SetTokenLevel(sandbox::USER_RESTRICTED_SAME_ACCESS, level);
  SANDBOX_ENSURE_SUCCESS(
      result,
      "SetTokenLevel should never fail with these arguments, what happened?");

  result = mPolicy->SetAlternateDesktop(true);
  if (NS_WARN_IF(result != sandbox::SBOX_ALL_OK)) {
    LOG_W("SetAlternateDesktop failed, result: %i, last error: %x", result,
          ::GetLastError());
  }

  result = mPolicy->SetIntegrityLevel(sandbox::INTEGRITY_LEVEL_LOW);
  MOZ_ASSERT(sandbox::SBOX_ALL_OK == result,
             "SetIntegrityLevel should never fail with these arguments, what "
             "happened?");

  result =
      mPolicy->SetDelayedIntegrityLevel(sandbox::INTEGRITY_LEVEL_UNTRUSTED);
  SANDBOX_ENSURE_SUCCESS(result,
                         "SetIntegrityLevel should never fail with these "
                         "arguments, what happened?");

  mPolicy->SetLockdownDefaultDacl();
  mPolicy->AddRestrictingRandomSid();

  sandbox::MitigationFlags mitigations =
      sandbox::MITIGATION_BOTTOM_UP_ASLR | sandbox::MITIGATION_HEAP_TERMINATE |
      sandbox::MITIGATION_SEHOP | sandbox::MITIGATION_EXTENSION_POINT_DISABLE |
      sandbox::MITIGATION_DEP_NO_ATL_THUNK | sandbox::MITIGATION_DEP;

  if (StaticPrefs::security_sandbox_gmp_shadow_stack_enabled()) {
    mitigations |= sandbox::MITIGATION_CET_COMPAT_MODE;
  }

  result = mPolicy->SetProcessMitigations(mitigations);
  SANDBOX_ENSURE_SUCCESS(result, "Invalid flags for SetProcessMitigations.");

  // Chromium only implements win32k disable for PPAPI on Win10 or later,
  // believed to be due to the interceptions required for OPM.
  if (StaticPrefs::security_sandbox_gmp_win32k_disable() && IsWin10OrLater()) {
    result = AddWin32kLockdownPolicy(mPolicy, true);
    SANDBOX_ENSURE_SUCCESS(result, "Failed to add the win32k lockdown policy");
  }

  mitigations = sandbox::MITIGATION_STRICT_HANDLE_CHECKS |
                sandbox::MITIGATION_DLL_SEARCH_ORDER;

  result = mPolicy->SetDelayedProcessMitigations(mitigations);
  SANDBOX_ENSURE_SUCCESS(result,
                         "Invalid flags for SetDelayedProcessMitigations.");

  // Add the policy for the client side of a pipe. It is just a file
  // in the \pipe\ namespace. We restrict it to pipes that start with
  // "chrome." so the sandboxed process cannot connect to system services.
  result = mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_FILES,
                            sandbox::TargetPolicy::FILES_ALLOW_ANY,
                            L"\\??\\pipe\\chrome.*");
  SANDBOX_ENSURE_SUCCESS(
      result,
      "With these static arguments AddRule should never fail, what happened?");

  // Add the policy for the client side of the crash server pipe.
  result = mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_FILES,
                            sandbox::TargetPolicy::FILES_ALLOW_ANY,
                            L"\\??\\pipe\\gecko-crash-server-pipe.*");
  SANDBOX_ENSURE_SUCCESS(
      result,
      "With these static arguments AddRule should never fail, what happened?");

#ifdef DEBUG
  // The plugin process can't create named events, but we'll
  // make an exception for the events used in logging. Removing
  // this will break EME in debug builds.
  result = mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_SYNC,
                            sandbox::TargetPolicy::EVENTS_ALLOW_ANY,
                            L"ChromeIPCLog.*");
  SANDBOX_ENSURE_SUCCESS(
      result,
      "With these static arguments AddRule should never fail, what happened?");
#endif

  // The following rules were added because, during analysis of an EME
  // plugin during development, these registry keys were accessed when
  // loading the plugin. Commenting out these policy exceptions caused
  // plugin loading to fail, so they are necessary for proper functioning
  // of at least one EME plugin.
  result = mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_REGISTRY,
                            sandbox::TargetPolicy::REG_ALLOW_READONLY,
                            L"HKEY_CURRENT_USER");
  SANDBOX_ENSURE_SUCCESS(
      result,
      "With these static arguments AddRule should never fail, what happened?");

  result = mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_REGISTRY,
                            sandbox::TargetPolicy::REG_ALLOW_READONLY,
                            L"HKEY_CURRENT_USER\\Control Panel\\Desktop");
  SANDBOX_ENSURE_SUCCESS(
      result,
      "With these static arguments AddRule should never fail, what happened?");

  result = mPolicy->AddRule(
      sandbox::TargetPolicy::SUBSYS_REGISTRY,
      sandbox::TargetPolicy::REG_ALLOW_READONLY,
      L"HKEY_CURRENT_USER\\Control Panel\\Desktop\\LanguageConfiguration");
  SANDBOX_ENSURE_SUCCESS(
      result,
      "With these static arguments AddRule should never fail, what happened?");

  result = mPolicy->AddRule(
      sandbox::TargetPolicy::SUBSYS_REGISTRY,
      sandbox::TargetPolicy::REG_ALLOW_READONLY,
      L"HKEY_LOCAL_"
      L"MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\SideBySide");
  SANDBOX_ENSURE_SUCCESS(
      result,
      "With these static arguments AddRule should never fail, what happened?");

  // The following rules were added because, during analysis of an EME
  // plugin during development, these registry keys were accessed when
  // loading the plugin. Commenting out these policy exceptions did not
  // cause anything to break during initial testing, but might cause
  // unforeseen issues down the road.
  result = mPolicy->AddRule(
      sandbox::TargetPolicy::SUBSYS_REGISTRY,
      sandbox::TargetPolicy::REG_ALLOW_READONLY,
      L"HKEY_LOCAL_MACHINE\\SOFTWARE\\Policies\\Microsoft\\MUI\\Settings");
  SANDBOX_ENSURE_SUCCESS(
      result,
      "With these static arguments AddRule should never fail, what happened?");

  result = mPolicy->AddRule(
      sandbox::TargetPolicy::SUBSYS_REGISTRY,
      sandbox::TargetPolicy::REG_ALLOW_READONLY,
      L"HKEY_CURRENT_USER\\Software\\Policies\\Microsoft\\Control "
      L"Panel\\Desktop");
  SANDBOX_ENSURE_SUCCESS(
      result,
      "With these static arguments AddRule should never fail, what happened?");

  result = mPolicy->AddRule(
      sandbox::TargetPolicy::SUBSYS_REGISTRY,
      sandbox::TargetPolicy::REG_ALLOW_READONLY,
      L"HKEY_CURRENT_USER\\Control Panel\\Desktop\\PreferredUILanguages");
  SANDBOX_ENSURE_SUCCESS(
      result,
      "With these static arguments AddRule should never fail, what happened?");

  result = mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_REGISTRY,
                            sandbox::TargetPolicy::REG_ALLOW_READONLY,
                            L"HKEY_LOCAL_"
                            L"MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVer"
                            L"sion\\SideBySide\\PreferExternalManifest");
  SANDBOX_ENSURE_SUCCESS(
      result,
      "With these static arguments AddRule should never fail, what happened?");

  // The following rules were added to allow a GMP to be loaded when any
  // AppLocker DLL rules are specified. If the rules specifically block the DLL
  // then it will not load.
  result = mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_FILES,
                            sandbox::TargetPolicy::FILES_ALLOW_READONLY,
                            L"\\Device\\SrpDevice");
  SANDBOX_ENSURE_SUCCESS(
      result,
      "With these static arguments AddRule should never fail, what happened?");
  result = mPolicy->AddRule(
      sandbox::TargetPolicy::SUBSYS_REGISTRY,
      sandbox::TargetPolicy::REG_ALLOW_READONLY,
      L"HKEY_LOCAL_MACHINE\\System\\CurrentControlSet\\Control\\Srp\\GP\\");
  SANDBOX_ENSURE_SUCCESS(
      result,
      "With these static arguments AddRule should never fail, what happened?");
  // On certain Windows versions there is a double slash before GP in the path.
  result = mPolicy->AddRule(
      sandbox::TargetPolicy::SUBSYS_REGISTRY,
      sandbox::TargetPolicy::REG_ALLOW_READONLY,
      L"HKEY_LOCAL_MACHINE\\System\\CurrentControlSet\\Control\\Srp\\\\GP\\");
  SANDBOX_ENSURE_SUCCESS(
      result,
      "With these static arguments AddRule should never fail, what happened?");

  // The GMP process needs to be able to share memory with the main process for
  // crash reporting. On arm64 when we are launching remotely via an x86 broker,
  // we need the rule to be HANDLES_DUP_ANY, because we still need to duplicate
  // to the main process not the child's broker.
  result = mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_HANDLES,
                            aIsRemoteLaunch
                                ? sandbox::TargetPolicy::HANDLES_DUP_ANY
                                : sandbox::TargetPolicy::HANDLES_DUP_BROKER,
                            L"Section");
  SANDBOX_ENSURE_SUCCESS(
      result,
      "With these static arguments AddRule should never fail, what happened?");

  return true;
}
#undef SANDBOX_ENSURE_SUCCESS

bool SandboxBroker::AllowReadFile(wchar_t const* file) {
  if (!mPolicy) {
    return false;
  }

  auto result =
      mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_FILES,
                       sandbox::TargetPolicy::FILES_ALLOW_READONLY, file);
  if (sandbox::SBOX_ALL_OK != result) {
    LOG_E("Failed (ResultCode %d) to add read access to: %S", result, file);
    return false;
  }

  return true;
}

/* static */
bool SandboxBroker::AddTargetPeer(HANDLE aPeerProcess) {
  if (!sBrokerService) {
    return false;
  }

  sandbox::ResultCode result = sBrokerService->AddTargetPeer(aPeerProcess);
  return (sandbox::SBOX_ALL_OK == result);
}

void SandboxBroker::AddHandleToShare(HANDLE aHandle) {
  mPolicy->AddHandleToShare(aHandle);
}

void SandboxBroker::ApplyLoggingPolicy() {
  MOZ_ASSERT(mPolicy);

  // Add dummy rules, so that we can log in the interception code.
  // We already have a file interception set up for the client side of pipes.
  // Also, passing just "dummy" for file system policy causes win_utils.cc
  // IsReparsePoint() to loop.
  mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_NAMED_PIPES,
                   sandbox::TargetPolicy::NAMEDPIPES_ALLOW_ANY, L"dummy");
  mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_PROCESS,
                   sandbox::TargetPolicy::PROCESS_MIN_EXEC, L"dummy");
  mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_REGISTRY,
                   sandbox::TargetPolicy::REG_ALLOW_READONLY,
                   L"HKEY_CURRENT_USER\\dummy");
  mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_SYNC,
                   sandbox::TargetPolicy::EVENTS_ALLOW_READONLY, L"dummy");
  mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_HANDLES,
                   sandbox::TargetPolicy::HANDLES_DUP_BROKER, L"dummy");
}

SandboxBroker::~SandboxBroker() {
  if (mPolicy) {
    mPolicy->Release();
    mPolicy = nullptr;
  }
}

#ifdef _ARM64_
// We can't include remoteSandboxBroker.h here directly, as it includes
// IPDL headers, which include a different copy of the chromium base
// libraries, which leads to conflicts.
extern AbstractSandboxBroker* CreateRemoteSandboxBroker();
#endif

// static
AbstractSandboxBroker* AbstractSandboxBroker::Create(
    GeckoProcessType aProcessType) {
#ifdef _ARM64_
  if (aProcessType == GeckoProcessType_GMPlugin) {
    return CreateRemoteSandboxBroker();
  }
#endif
  return new SandboxBroker();
}

}  // namespace mozilla
