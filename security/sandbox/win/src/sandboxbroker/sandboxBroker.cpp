/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "sandboxBroker.h"

#include <string>

#include "base/win/windows_version.h"
#include "mozilla/Assertions.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Logging.h"
#include "mozilla/NSPRLogModulesParser.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Telemetry.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsCOMPtr.h"
#include "nsDirectoryServiceDefs.h"
#include "nsIFile.h"
#include "nsIProperties.h"
#include "nsServiceManagerUtils.h"
#include "nsString.h"
#include "sandbox/win/src/sandbox.h"
#include "sandbox/win/src/security_level.h"
#include "WinUtils.h"

namespace mozilla
{

sandbox::BrokerServices *SandboxBroker::sBrokerService = nullptr;

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

static LazyLogModule sSandboxBrokerLog("SandboxBroker");

#define LOG_E(...) MOZ_LOG(sSandboxBrokerLog, LogLevel::Error, (__VA_ARGS__))
#define LOG_W(...) MOZ_LOG(sSandboxBrokerLog, LogLevel::Warning, (__VA_ARGS__))

/* static */
void
SandboxBroker::Initialize(sandbox::BrokerServices* aBrokerServices)
{
  sBrokerService = aBrokerServices;

  wchar_t exePath[MAX_PATH];
  if (!::GetModuleFileNameW(nullptr, exePath, MAX_PATH)) {
    return;
  }

  std::wstring exeString(exePath);
  if (!widget::WinUtils::ResolveJunctionPointsAndSymLinks(exeString)) {
    return;
  }

  wchar_t volPath[MAX_PATH];
  if (!::GetVolumePathNameW(exeString.c_str(), volPath, MAX_PATH)) {
    return;
  }

  sRunningFromNetworkDrive = (::GetDriveTypeW(volPath) == DRIVE_REMOTE);
}

static void
CacheDirAndAutoClear(nsIProperties* aDirSvc, const char* aDirKey,
                     UniquePtr<nsString>* cacheVar)
{
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
  if (Substring(**cacheVar, 0, 2).Equals(L"\\\\")) {
    (*cacheVar)->InsertLiteral(u"??\\UNC", 1);
  }
}

/* static */
void
SandboxBroker::CacheRulesDirectories()
{
  MOZ_ASSERT(NS_IsMainThread());

  nsresult rv;
  nsCOMPtr<nsIProperties> dirSvc =
    do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID, &rv);
  if (NS_FAILED(rv)) {
    MOZ_ASSERT(false, "Failed to get directory service, cannot cache directories for rules.");
    LOG_E("Failed to get directory service, cannot cache directories for rules.");
    return;
  }

  CacheDirAndAutoClear(dirSvc, NS_GRE_DIR, &sBinDir);
  CacheDirAndAutoClear(dirSvc, NS_APP_USER_PROFILE_50_DIR, &sProfileDir);
  CacheDirAndAutoClear(dirSvc, NS_APP_CONTENT_PROCESS_TEMP_DIR, &sContentTempDir);
  CacheDirAndAutoClear(dirSvc, NS_WIN_APPDATA_DIR, &sRoamingAppDataDir);
  CacheDirAndAutoClear(dirSvc, NS_WIN_LOCAL_APPDATA_DIR, &sLocalAppDataDir);
}

SandboxBroker::SandboxBroker()
{
  if (sBrokerService) {
    mPolicy = sBrokerService->CreatePolicy();
    if (sRunningFromNetworkDrive) {
      mPolicy->SetDoNotUseRestrictingSIDs();
    }
  } else {
    mPolicy = nullptr;
  }
}

bool
SandboxBroker::LaunchApp(const wchar_t *aPath,
                         const wchar_t *aArguments,
                         const bool aEnableLogging,
                         void **aProcessHandle)
{
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
  wchar_t const* logFileName = _wgetenv(L"MOZ_LOG_FILE");
  char const* logFileModules = getenv("MOZ_LOG");
  if (logFileName && logFileModules) {
    bool rotate = false;
    NSPRLogModulesParser(logFileModules,
      [&rotate](const char* aName, LogLevel aLevel, int32_t aValue) mutable {
        if (strcmp(aName, "rotate") == 0) {
          // Less or eq zero means to turn rotate off.
          rotate = aValue > 0;
        }
      }
    );

    if (rotate) {
      wchar_t logFileNameWild[MAX_PATH + 2];
      _snwprintf(logFileNameWild, sizeof(logFileNameWild), L"%s.?", logFileName);

      mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_FILES,
                       sandbox::TargetPolicy::FILES_ALLOW_ANY, logFileNameWild);
    } else {
      mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_FILES,
                       sandbox::TargetPolicy::FILES_ALLOW_ANY, logFileName);
    }
  }

  logFileName = _wgetenv(L"NSPR_LOG_FILE");
  if (logFileName) {
    mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_FILES,
                     sandbox::TargetPolicy::FILES_ALLOW_ANY, logFileName);
  }

  // Ceate the sandboxed process
  PROCESS_INFORMATION targetInfo = {0};
  sandbox::ResultCode result;
  sandbox::ResultCode last_warning = sandbox::SBOX_ALL_OK;
  DWORD last_error = ERROR_SUCCESS;
  result = sBrokerService->SpawnTarget(aPath, aArguments, mPolicy,
                                       &last_warning, &last_error, &targetInfo);
  if (sandbox::SBOX_ALL_OK != result) {
    Telemetry::Accumulate(Telemetry::SANDBOX_FAILED_LAUNCH, result);
    LOG_E("Failed (ResultCode %d) to SpawnTarget with last_error=%d, last_warning=%d",
          result, last_error, last_warning);
    return false;
  } else if (sandbox::SBOX_ALL_OK != last_warning) {
    // If there was a warning (but the result was still ok), log it and proceed.
    LOG_W("Warning on SpawnTarget with last_error=%d, last_warning=%d",
          last_error, last_warning);
  }

  // The sandboxed process is started in a suspended state, resume it now that
  // we've set things up.
  ResumeThread(targetInfo.hThread);
  CloseHandle(targetInfo.hThread);

  // Return the process handle to the caller
  *aProcessHandle = targetInfo.hProcess;

  return true;
}

static void
AddCachedDirRule(sandbox::TargetPolicy* aPolicy,
                 sandbox::TargetPolicy::Semantics aAccess,
                 const UniquePtr<nsString>& aBaseDir,
                 const nsLiteralString& aRelativePath)
{
  if (!aBaseDir) {
    // This can only be an NS_WARNING, because it can null for xpcshell tests.
    NS_WARNING("Tried to add rule with null base dir.");
    LOG_E("Tried to add rule with null base dir. Relative path: %S, Access: %d",
          aRelativePath.get(), aAccess);
    return;
  }

  nsAutoString rulePath(*aBaseDir);
  rulePath.Append(aRelativePath);

  sandbox::ResultCode result =
    aPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_FILES, aAccess,
                     rulePath.get());
  if (sandbox::SBOX_ALL_OK != result) {
    NS_ERROR("Failed to add file policy rule.");
    LOG_E("Failed (ResultCode %d) to add %d access to: %S",
          result, aAccess, rulePath.get());
  }
}

#if defined(MOZ_CONTENT_SANDBOX)

void
SandboxBroker::SetSecurityLevelForContentProcess(int32_t aSandboxLevel,
                                                 base::ChildPrivileges aPrivs)
{
  MOZ_RELEASE_ASSERT(mPolicy, "mPolicy must be set before this call.");

  sandbox::JobLevel jobLevel;
  sandbox::TokenLevel accessTokenLevel;
  sandbox::IntegrityLevel initialIntegrityLevel;
  sandbox::IntegrityLevel delayedIntegrityLevel;

  // The setting of these levels is pretty arbitrary, but they are a useful (if
  // crude) tool while we are tightening the policy. Gaps are left to try and
  // avoid changing their meaning.
  MOZ_RELEASE_ASSERT(aSandboxLevel >= 1, "Should not be called with aSandboxLevel < 1");
  if (aSandboxLevel >= 20) {
    jobLevel = sandbox::JOB_LOCKDOWN;
    accessTokenLevel = sandbox::USER_LOCKDOWN;
    initialIntegrityLevel = sandbox::INTEGRITY_LEVEL_LOW;
    delayedIntegrityLevel = sandbox::INTEGRITY_LEVEL_UNTRUSTED;
  } else if (aSandboxLevel >= 10) {
    jobLevel = sandbox::JOB_RESTRICTED;
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
  } else if (aSandboxLevel == 1) {
    jobLevel = sandbox::JOB_NONE;
    accessTokenLevel = sandbox::USER_NON_ADMIN;
    initialIntegrityLevel = sandbox::INTEGRITY_LEVEL_LOW;
    delayedIntegrityLevel = sandbox::INTEGRITY_LEVEL_LOW;
  }

  // If PRIVILEGES_FILEREAD required, don't allow settings that block reads.
  if (aPrivs == base::ChildPrivileges::PRIVILEGES_FILEREAD) {
    if (accessTokenLevel < sandbox::USER_NON_ADMIN) {
      accessTokenLevel = sandbox::USER_NON_ADMIN;
    }
    if (delayedIntegrityLevel > sandbox::INTEGRITY_LEVEL_LOW) {
      delayedIntegrityLevel = sandbox::INTEGRITY_LEVEL_LOW;
    }
  }

  sandbox::ResultCode result = mPolicy->SetJobLevel(jobLevel,
                                                    0 /* ui_exceptions */);
  MOZ_RELEASE_ASSERT(sandbox::SBOX_ALL_OK == result,
                     "Setting job level failed, have you set memory limit when jobLevel == JOB_NONE?");

  // If the delayed access token is not restricted we don't want the initial one
  // to be either, because it can interfere with running from a network drive.
  sandbox::TokenLevel initialAccessTokenLevel =
    (accessTokenLevel == sandbox::USER_UNPROTECTED ||
     accessTokenLevel == sandbox::USER_NON_ADMIN)
    ? sandbox::USER_UNPROTECTED : sandbox::USER_RESTRICTED_SAME_ACCESS;

  result = mPolicy->SetTokenLevel(initialAccessTokenLevel, accessTokenLevel);
  MOZ_RELEASE_ASSERT(sandbox::SBOX_ALL_OK == result,
                     "Lockdown level cannot be USER_UNPROTECTED or USER_LAST if initial level was USER_RESTRICTED_SAME_ACCESS");

  result = mPolicy->SetIntegrityLevel(initialIntegrityLevel);
  MOZ_RELEASE_ASSERT(sandbox::SBOX_ALL_OK == result,
                     "SetIntegrityLevel should never fail, what happened?");
  result = mPolicy->SetDelayedIntegrityLevel(delayedIntegrityLevel);
  MOZ_RELEASE_ASSERT(sandbox::SBOX_ALL_OK == result,
                     "SetDelayedIntegrityLevel should never fail, what happened?");

  if (aSandboxLevel > 3) {
    result = mPolicy->SetAlternateDesktop(true);
    MOZ_RELEASE_ASSERT(sandbox::SBOX_ALL_OK == result,
                       "Failed to create alternate desktop for sandbox.");
  }

  sandbox::MitigationFlags mitigations =
    sandbox::MITIGATION_BOTTOM_UP_ASLR |
    sandbox::MITIGATION_HEAP_TERMINATE |
    sandbox::MITIGATION_SEHOP |
    sandbox::MITIGATION_DEP_NO_ATL_THUNK |
    sandbox::MITIGATION_DEP;

  result = mPolicy->SetProcessMitigations(mitigations);
  MOZ_RELEASE_ASSERT(sandbox::SBOX_ALL_OK == result,
                     "Invalid flags for SetProcessMitigations.");

  mitigations =
    sandbox::MITIGATION_STRICT_HANDLE_CHECKS |
    sandbox::MITIGATION_DLL_SEARCH_ORDER;

  result = mPolicy->SetDelayedProcessMitigations(mitigations);
  MOZ_RELEASE_ASSERT(sandbox::SBOX_ALL_OK == result,
                     "Invalid flags for SetDelayedProcessMitigations.");

  // Add rule to allow read / write access to content temp dir. If for some
  // reason the addition of the content temp failed, this will give write access
  // to the normal TEMP dir. However such failures should be pretty rare and
  // without this printing will not currently work.
  AddCachedDirRule(mPolicy, sandbox::TargetPolicy::FILES_ALLOW_ANY,
                   sContentTempDir, NS_LITERAL_STRING("\\*"));

  // We still have edge cases where the child at low integrity can't read some
  // files, so add a rule to allow read access to everything when required.
  if (aSandboxLevel == 1 ||
      aPrivs == base::ChildPrivileges::PRIVILEGES_FILEREAD) {
    result = mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_FILES,
                              sandbox::TargetPolicy::FILES_ALLOW_READONLY,
                              L"*");
    MOZ_RELEASE_ASSERT(sandbox::SBOX_ALL_OK == result,
                       "With these static arguments AddRule should never fail, what happened?");
  } else {
    // Add rule to allow read access to installation directory.
    AddCachedDirRule(mPolicy, sandbox::TargetPolicy::FILES_ALLOW_READONLY,
                     sBinDir, NS_LITERAL_STRING("\\*"));

    // Add rule to allow read access chrome directory within profile.
    AddCachedDirRule(mPolicy, sandbox::TargetPolicy::FILES_ALLOW_READONLY,
                     sProfileDir, NS_LITERAL_STRING("\\chrome\\*"));

    // Add rule to allow read access extensions directory within profile.
    AddCachedDirRule(mPolicy, sandbox::TargetPolicy::FILES_ALLOW_READONLY,
                     sProfileDir, NS_LITERAL_STRING("\\extensions\\*"));
  }

  // Add the policy for the client side of a pipe. It is just a file
  // in the \pipe\ namespace. We restrict it to pipes that start with
  // "chrome." so the sandboxed process cannot connect to system services.
  result = mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_FILES,
                            sandbox::TargetPolicy::FILES_ALLOW_ANY,
                            L"\\??\\pipe\\chrome.*");
  MOZ_RELEASE_ASSERT(sandbox::SBOX_ALL_OK == result,
                     "With these static arguments AddRule should never fail, what happened?");

  // Add the policy for the client side of the crash server pipe.
  result = mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_FILES,
                            sandbox::TargetPolicy::FILES_ALLOW_ANY,
                            L"\\??\\pipe\\gecko-crash-server-pipe.*");
  MOZ_RELEASE_ASSERT(sandbox::SBOX_ALL_OK == result,
                     "With these static arguments AddRule should never fail, what happened?");

  // The content process needs to be able to duplicate named pipes back to the
  // broker process, which are File type handles.
  result = mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_HANDLES,
                            sandbox::TargetPolicy::HANDLES_DUP_BROKER,
                            L"File");
  MOZ_RELEASE_ASSERT(sandbox::SBOX_ALL_OK == result,
                     "With these static arguments AddRule should never fail, what happened?");

  // The content process needs to be able to duplicate shared memory handles,
  // which are Section handles, to the broker process and other child processes.
  result = mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_HANDLES,
                            sandbox::TargetPolicy::HANDLES_DUP_BROKER,
                            L"Section");
  MOZ_RELEASE_ASSERT(sandbox::SBOX_ALL_OK == result,
                     "With these static arguments AddRule should never fail, what happened?");
  result = mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_HANDLES,
                            sandbox::TargetPolicy::HANDLES_DUP_ANY,
                            L"Section");
  MOZ_RELEASE_ASSERT(sandbox::SBOX_ALL_OK == result,
                     "With these static arguments AddRule should never fail, what happened?");

  // The content process needs to be able to duplicate semaphore handles,
  // to the broker process and other child processes.
  result = mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_HANDLES,
                            sandbox::TargetPolicy::HANDLES_DUP_BROKER,
                            L"Semaphore");
  MOZ_RELEASE_ASSERT(sandbox::SBOX_ALL_OK == result,
                     "With these static arguments AddRule should never fail, what happened?");
  result = mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_HANDLES,
                            sandbox::TargetPolicy::HANDLES_DUP_ANY,
                            L"Semaphore");
  MOZ_RELEASE_ASSERT(sandbox::SBOX_ALL_OK == result,
                     "With these static arguments AddRule should never fail, what happened?");
}
#endif

void
SandboxBroker::SetSecurityLevelForGPUProcess(int32_t aSandboxLevel)
{
  MOZ_RELEASE_ASSERT(mPolicy, "mPolicy must be set before this call.");

  sandbox::JobLevel jobLevel;
  sandbox::TokenLevel accessTokenLevel;
  sandbox::IntegrityLevel initialIntegrityLevel;
  sandbox::IntegrityLevel delayedIntegrityLevel;

  // The setting of these levels is pretty arbitrary, but they are a useful (if
  // crude) tool while we are tightening the policy. Gaps are left to try and
  // avoid changing their meaning.
  MOZ_RELEASE_ASSERT(aSandboxLevel >= 1, "Should not be called with aSandboxLevel < 1");
  if (aSandboxLevel >= 2) {
    jobLevel = sandbox::JOB_NONE;
    accessTokenLevel = sandbox::USER_LIMITED;
    initialIntegrityLevel = sandbox::INTEGRITY_LEVEL_LOW;
    delayedIntegrityLevel = sandbox::INTEGRITY_LEVEL_LOW;
  } else if (aSandboxLevel == 1) {
    jobLevel = sandbox::JOB_NONE;
    accessTokenLevel = sandbox::USER_NON_ADMIN;
    initialIntegrityLevel = sandbox::INTEGRITY_LEVEL_LOW;
    delayedIntegrityLevel = sandbox::INTEGRITY_LEVEL_LOW;
  }

  sandbox::ResultCode result = mPolicy->SetJobLevel(jobLevel,
                                                    0 /* ui_exceptions */);
  MOZ_RELEASE_ASSERT(sandbox::SBOX_ALL_OK == result,
                     "Setting job level failed, have you set memory limit when jobLevel == JOB_NONE?");

  // If the delayed access token is not restricted we don't want the initial one
  // to be either, because it can interfere with running from a network drive.
  sandbox::TokenLevel initialAccessTokenLevel =
    (accessTokenLevel == sandbox::USER_UNPROTECTED ||
     accessTokenLevel == sandbox::USER_NON_ADMIN)
    ? sandbox::USER_UNPROTECTED : sandbox::USER_RESTRICTED_SAME_ACCESS;

  result = mPolicy->SetTokenLevel(initialAccessTokenLevel, accessTokenLevel);
  MOZ_RELEASE_ASSERT(sandbox::SBOX_ALL_OK == result,
                     "Lockdown level cannot be USER_UNPROTECTED or USER_LAST if initial level was USER_RESTRICTED_SAME_ACCESS");

  result = mPolicy->SetIntegrityLevel(initialIntegrityLevel);
  MOZ_RELEASE_ASSERT(sandbox::SBOX_ALL_OK == result,
                     "SetIntegrityLevel should never fail, what happened?");
  result = mPolicy->SetDelayedIntegrityLevel(delayedIntegrityLevel);
  MOZ_RELEASE_ASSERT(sandbox::SBOX_ALL_OK == result,
                     "SetDelayedIntegrityLevel should never fail, what happened?");

  sandbox::MitigationFlags mitigations =
    sandbox::MITIGATION_BOTTOM_UP_ASLR |
    sandbox::MITIGATION_HEAP_TERMINATE |
    sandbox::MITIGATION_SEHOP |
    sandbox::MITIGATION_DEP_NO_ATL_THUNK |
    sandbox::MITIGATION_DEP;

  result = mPolicy->SetProcessMitigations(mitigations);
  MOZ_RELEASE_ASSERT(sandbox::SBOX_ALL_OK == result,
                     "Invalid flags for SetProcessMitigations.");

  mitigations =
    sandbox::MITIGATION_STRICT_HANDLE_CHECKS |
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
  MOZ_RELEASE_ASSERT(sandbox::SBOX_ALL_OK == result,
                     "With these static arguments AddRule should never fail, what happened?");

  // Add the policy for the client side of the crash server pipe.
  result = mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_FILES,
                            sandbox::TargetPolicy::FILES_ALLOW_ANY,
                            L"\\??\\pipe\\gecko-crash-server-pipe.*");
  MOZ_RELEASE_ASSERT(sandbox::SBOX_ALL_OK == result,
                     "With these static arguments AddRule should never fail, what happened?");

  // The process needs to be able to duplicate shared memory handles,
  // which are Section handles, to the broker process and other child processes.
  result = mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_HANDLES,
                            sandbox::TargetPolicy::HANDLES_DUP_BROKER,
                            L"Section");
  MOZ_RELEASE_ASSERT(sandbox::SBOX_ALL_OK == result,
                     "With these static arguments AddRule should never fail, what happened?");
  result = mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_HANDLES,
                            sandbox::TargetPolicy::HANDLES_DUP_ANY,
                            L"Section");
  MOZ_RELEASE_ASSERT(sandbox::SBOX_ALL_OK == result,
                     "With these static arguments AddRule should never fail, what happened?");
}

#define SANDBOX_ENSURE_SUCCESS(result, message) \
  do { \
    MOZ_ASSERT(sandbox::SBOX_ALL_OK == result, message); \
    if (sandbox::SBOX_ALL_OK != result) \
      return false; \
  } while (0)

bool
SandboxBroker::SetSecurityLevelForPluginProcess(int32_t aSandboxLevel)
{
  if (!mPolicy) {
    return false;
  }

  sandbox::JobLevel jobLevel;
  sandbox::TokenLevel accessTokenLevel;
  sandbox::IntegrityLevel initialIntegrityLevel;
  sandbox::IntegrityLevel delayedIntegrityLevel;

  if (aSandboxLevel > 2) {
    jobLevel = sandbox::JOB_UNPROTECTED;
    accessTokenLevel = sandbox::USER_LIMITED;
    initialIntegrityLevel = sandbox::INTEGRITY_LEVEL_LOW;
    delayedIntegrityLevel = sandbox::INTEGRITY_LEVEL_LOW;
  } else if (aSandboxLevel == 2) {
    jobLevel = sandbox::JOB_UNPROTECTED;
    accessTokenLevel = sandbox::USER_INTERACTIVE;
    initialIntegrityLevel = sandbox::INTEGRITY_LEVEL_LOW;
    delayedIntegrityLevel = sandbox::INTEGRITY_LEVEL_LOW;
  } else {
    jobLevel = sandbox::JOB_NONE;
    accessTokenLevel = sandbox::USER_NON_ADMIN;
    initialIntegrityLevel = sandbox::INTEGRITY_LEVEL_MEDIUM;
    delayedIntegrityLevel = sandbox::INTEGRITY_LEVEL_MEDIUM;
  }

  sandbox::ResultCode result = mPolicy->SetJobLevel(jobLevel,
                                                    0 /* ui_exceptions */);
  SANDBOX_ENSURE_SUCCESS(result,
                         "Setting job level failed, have you set memory limit when jobLevel == JOB_NONE?");

  result = mPolicy->SetTokenLevel(sandbox::USER_RESTRICTED_SAME_ACCESS,
                                  accessTokenLevel);
  SANDBOX_ENSURE_SUCCESS(result,
                         "Lockdown level cannot be USER_UNPROTECTED or USER_LAST if initial level was USER_RESTRICTED_SAME_ACCESS");

  result = mPolicy->SetIntegrityLevel(initialIntegrityLevel);
  SANDBOX_ENSURE_SUCCESS(result,
                         "SetIntegrityLevel should never fail, what happened?");

  result = mPolicy->SetDelayedIntegrityLevel(delayedIntegrityLevel);
  SANDBOX_ENSURE_SUCCESS(result,
                         "SetDelayedIntegrityLevel should never fail, what happened?");

  sandbox::MitigationFlags mitigations =
    sandbox::MITIGATION_BOTTOM_UP_ASLR |
    sandbox::MITIGATION_HEAP_TERMINATE |
    sandbox::MITIGATION_SEHOP |
    sandbox::MITIGATION_DEP_NO_ATL_THUNK |
    sandbox::MITIGATION_DEP;

  result = mPolicy->SetProcessMitigations(mitigations);
  SANDBOX_ENSURE_SUCCESS(result,
                         "Invalid flags for SetProcessMitigations.");

  if (aSandboxLevel >= 2) {
    // Level 2 and above uses low integrity, so we need to give write access to
    // the Flash directories.
    AddCachedDirRule(mPolicy, sandbox::TargetPolicy::FILES_ALLOW_ANY,
                     sRoamingAppDataDir,
                     NS_LITERAL_STRING("\\Macromedia\\Flash Player\\*"));
    AddCachedDirRule(mPolicy, sandbox::TargetPolicy::FILES_ALLOW_ANY,
                     sLocalAppDataDir,
                     NS_LITERAL_STRING("\\Macromedia\\Flash Player\\*"));
    AddCachedDirRule(mPolicy, sandbox::TargetPolicy::FILES_ALLOW_ANY,
                     sRoamingAppDataDir,
                     NS_LITERAL_STRING("\\Adobe\\Flash Player\\*"));

    // Access also has to be given to create the parent directories as they may
    // not exist.
    AddCachedDirRule(mPolicy, sandbox::TargetPolicy::FILES_ALLOW_DIR_ANY,
                     sRoamingAppDataDir,
                     NS_LITERAL_STRING("\\Macromedia"));
    AddCachedDirRule(mPolicy, sandbox::TargetPolicy::FILES_ALLOW_DIR_ANY,
                     sRoamingAppDataDir,
                     NS_LITERAL_STRING("\\Macromedia\\Flash Player"));
    AddCachedDirRule(mPolicy, sandbox::TargetPolicy::FILES_ALLOW_DIR_ANY,
                     sLocalAppDataDir,
                     NS_LITERAL_STRING("\\Macromedia"));
    AddCachedDirRule(mPolicy, sandbox::TargetPolicy::FILES_ALLOW_DIR_ANY,
                     sLocalAppDataDir,
                     NS_LITERAL_STRING("\\Macromedia\\Flash Player"));
    AddCachedDirRule(mPolicy, sandbox::TargetPolicy::FILES_ALLOW_DIR_ANY,
                     sRoamingAppDataDir,
                     NS_LITERAL_STRING("\\Adobe"));
    AddCachedDirRule(mPolicy, sandbox::TargetPolicy::FILES_ALLOW_DIR_ANY,
                     sRoamingAppDataDir,
                     NS_LITERAL_STRING("\\Adobe\\Flash Player"));
  }

  // Add the policy for the client side of a pipe. It is just a file
  // in the \pipe\ namespace. We restrict it to pipes that start with
  // "chrome." so the sandboxed process cannot connect to system services.
  result = mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_FILES,
                            sandbox::TargetPolicy::FILES_ALLOW_ANY,
                            L"\\??\\pipe\\chrome.*");
  SANDBOX_ENSURE_SUCCESS(result,
                         "With these static arguments AddRule should never fail, what happened?");

  // Add the policy for the client side of the crash server pipe.
  result = mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_FILES,
                            sandbox::TargetPolicy::FILES_ALLOW_ANY,
                            L"\\??\\pipe\\gecko-crash-server-pipe.*");
  SANDBOX_ENSURE_SUCCESS(result,
                         "With these static arguments AddRule should never fail, what happened?");

  // The NPAPI process needs to be able to duplicate shared memory to the
  // content process and broker process, which are Section type handles.
  // Content and broker are for e10s and non-e10s cases.
  result = mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_HANDLES,
                            sandbox::TargetPolicy::HANDLES_DUP_ANY,
                            L"Section");
  SANDBOX_ENSURE_SUCCESS(result,
                         "With these static arguments AddRule should never fail, what happened?");

  result = mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_HANDLES,
                            sandbox::TargetPolicy::HANDLES_DUP_BROKER,
                            L"Section");
  SANDBOX_ENSURE_SUCCESS(result,
                         "With these static arguments AddRule should never fail, what happened?");

  // These register keys are used by the file-browser dialog box.  They
  // remember the most-recently-used folders.
  result = mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_REGISTRY,
                            sandbox::TargetPolicy::REG_ALLOW_ANY,
                            L"HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\ComDlg32\\OpenSavePidlMRU\\*");
  SANDBOX_ENSURE_SUCCESS(result,
                         "With these static arguments AddRule should never fail, what happened?");

  result = mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_REGISTRY,
                            sandbox::TargetPolicy::REG_ALLOW_ANY,
                            L"HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\ComDlg32\\LastVisitedPidlMRULegacy\\*");
  SANDBOX_ENSURE_SUCCESS(result,
                         "With these static arguments AddRule should never fail, what happened?");

  return true;
}

bool
SandboxBroker::SetSecurityLevelForGMPlugin(SandboxLevel aLevel)
{
  if (!mPolicy) {
    return false;
  }

  auto result = mPolicy->SetJobLevel(sandbox::JOB_LOCKDOWN, 0);
  SANDBOX_ENSURE_SUCCESS(result,
                         "SetJobLevel should never fail with these arguments, what happened?");
  auto level = (aLevel == Restricted) ?
    sandbox::USER_RESTRICTED : sandbox::USER_LOCKDOWN;
  result = mPolicy->SetTokenLevel(sandbox::USER_RESTRICTED_SAME_ACCESS, level);
  SANDBOX_ENSURE_SUCCESS(result,
                         "SetTokenLevel should never fail with these arguments, what happened?");

  result = mPolicy->SetAlternateDesktop(true);
  SANDBOX_ENSURE_SUCCESS(result,
                         "Failed to create alternate desktop for sandbox.");

  result = mPolicy->SetIntegrityLevel(sandbox::INTEGRITY_LEVEL_LOW);
  MOZ_ASSERT(sandbox::SBOX_ALL_OK == result,
             "SetIntegrityLevel should never fail with these arguments, what happened?");

  result =
    mPolicy->SetDelayedIntegrityLevel(sandbox::INTEGRITY_LEVEL_UNTRUSTED);
  SANDBOX_ENSURE_SUCCESS(result,
                         "SetIntegrityLevel should never fail with these arguments, what happened?");

  sandbox::MitigationFlags mitigations =
    sandbox::MITIGATION_BOTTOM_UP_ASLR |
    sandbox::MITIGATION_HEAP_TERMINATE |
    sandbox::MITIGATION_SEHOP |
    sandbox::MITIGATION_DEP_NO_ATL_THUNK |
    sandbox::MITIGATION_DEP;

  result = mPolicy->SetProcessMitigations(mitigations);
  SANDBOX_ENSURE_SUCCESS(result,
                         "Invalid flags for SetProcessMitigations.");

  mitigations =
    sandbox::MITIGATION_STRICT_HANDLE_CHECKS |
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
  SANDBOX_ENSURE_SUCCESS(result,
                         "With these static arguments AddRule should never fail, what happened?");

  // Add the policy for the client side of the crash server pipe.
  result = mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_FILES,
                            sandbox::TargetPolicy::FILES_ALLOW_ANY,
                            L"\\??\\pipe\\gecko-crash-server-pipe.*");
  SANDBOX_ENSURE_SUCCESS(result,
                         "With these static arguments AddRule should never fail, what happened?");

#ifdef DEBUG
  // The plugin process can't create named events, but we'll
  // make an exception for the events used in logging. Removing
  // this will break EME in debug builds.
  result = mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_SYNC,
                            sandbox::TargetPolicy::EVENTS_ALLOW_ANY,
                            L"ChromeIPCLog.*");
  SANDBOX_ENSURE_SUCCESS(result,
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
  SANDBOX_ENSURE_SUCCESS(result,
                         "With these static arguments AddRule should never fail, what happened?");

  result = mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_REGISTRY,
                            sandbox::TargetPolicy::REG_ALLOW_READONLY,
                            L"HKEY_CURRENT_USER\\Control Panel\\Desktop");
  SANDBOX_ENSURE_SUCCESS(result,
                         "With these static arguments AddRule should never fail, what happened?");

  result = mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_REGISTRY,
                            sandbox::TargetPolicy::REG_ALLOW_READONLY,
                            L"HKEY_CURRENT_USER\\Control Panel\\Desktop\\LanguageConfiguration");
  SANDBOX_ENSURE_SUCCESS(result,
                         "With these static arguments AddRule should never fail, what happened?");

  result = mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_REGISTRY,
                            sandbox::TargetPolicy::REG_ALLOW_READONLY,
                            L"HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\SideBySide");
  SANDBOX_ENSURE_SUCCESS(result,
                         "With these static arguments AddRule should never fail, what happened?");


  // The following rules were added because, during analysis of an EME
  // plugin during development, these registry keys were accessed when
  // loading the plugin. Commenting out these policy exceptions did not
  // cause anything to break during initial testing, but might cause
  // unforeseen issues down the road.
  result = mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_REGISTRY,
                            sandbox::TargetPolicy::REG_ALLOW_READONLY,
                            L"HKEY_LOCAL_MACHINE\\SOFTWARE\\Policies\\Microsoft\\MUI\\Settings");
  SANDBOX_ENSURE_SUCCESS(result,
                         "With these static arguments AddRule should never fail, what happened?");

  result = mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_REGISTRY,
                            sandbox::TargetPolicy::REG_ALLOW_READONLY,
                            L"HKEY_CURRENT_USER\\Software\\Policies\\Microsoft\\Control Panel\\Desktop");
  SANDBOX_ENSURE_SUCCESS(result,
                         "With these static arguments AddRule should never fail, what happened?");

  result = mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_REGISTRY,
                            sandbox::TargetPolicy::REG_ALLOW_READONLY,
                            L"HKEY_CURRENT_USER\\Control Panel\\Desktop\\PreferredUILanguages");
  SANDBOX_ENSURE_SUCCESS(result,
                         "With these static arguments AddRule should never fail, what happened?");

  result = mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_REGISTRY,
                            sandbox::TargetPolicy::REG_ALLOW_READONLY,
                            L"HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\SideBySide\\PreferExternalManifest");
  SANDBOX_ENSURE_SUCCESS(result,
                         "With these static arguments AddRule should never fail, what happened?");

  // The following rules were added to allow a GMP to be loaded when any
  // AppLocker DLL rules are specified. If the rules specifically block the DLL
  // then it will not load.
  result = mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_FILES,
                            sandbox::TargetPolicy::FILES_ALLOW_READONLY,
                            L"\\Device\\SrpDevice");
  SANDBOX_ENSURE_SUCCESS(result,
                         "With these static arguments AddRule should never fail, what happened?");
  result = mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_REGISTRY,
                            sandbox::TargetPolicy::REG_ALLOW_READONLY,
                            L"HKEY_LOCAL_MACHINE\\System\\CurrentControlSet\\Control\\Srp\\GP\\");
  SANDBOX_ENSURE_SUCCESS(result,
                         "With these static arguments AddRule should never fail, what happened?");
  // On certain Windows versions there is a double slash before GP in the path.
  result = mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_REGISTRY,
                            sandbox::TargetPolicy::REG_ALLOW_READONLY,
                            L"HKEY_LOCAL_MACHINE\\System\\CurrentControlSet\\Control\\Srp\\\\GP\\");
  SANDBOX_ENSURE_SUCCESS(result,
                         "With these static arguments AddRule should never fail, what happened?");


  return true;
}
#undef SANDBOX_ENSURE_SUCCESS

bool
SandboxBroker::AllowReadFile(wchar_t const *file)
{
  if (!mPolicy) {
    return false;
  }

  auto result =
    mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_FILES,
                     sandbox::TargetPolicy::FILES_ALLOW_READONLY,
                     file);
  if (sandbox::SBOX_ALL_OK != result) {
    LOG_E("Failed (ResultCode %d) to add read access to: %S", result, file);
    return false;
  }

  return true;
}

bool
SandboxBroker::AddTargetPeer(HANDLE aPeerProcess)
{
  if (!sBrokerService) {
    return false;
  }

  sandbox::ResultCode result = sBrokerService->AddTargetPeer(aPeerProcess);
  return (sandbox::SBOX_ALL_OK == result);
}

void
SandboxBroker::ApplyLoggingPolicy()
{
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

SandboxBroker::~SandboxBroker()
{
  if (mPolicy) {
    mPolicy->Release();
    mPolicy = nullptr;
  }
}

}
