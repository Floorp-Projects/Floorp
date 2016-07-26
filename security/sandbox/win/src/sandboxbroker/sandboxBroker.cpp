/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "sandboxBroker.h"

#include "base/win/windows_version.h"
#include "mozilla/Assertions.h"
#include "sandbox/win/src/sandbox.h"
#include "sandbox/win/src/security_level.h"

namespace mozilla
{

sandbox::BrokerServices *SandboxBroker::sBrokerService = nullptr;

/* static */
void
SandboxBroker::Initialize(sandbox::BrokerServices* aBrokerServices)
{
  sBrokerService = aBrokerServices;
}

SandboxBroker::SandboxBroker()
{
  if (sBrokerService) {
    mPolicy = sBrokerService->CreatePolicy();
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

  // Ceate the sandboxed process
  PROCESS_INFORMATION targetInfo = {0};
  sandbox::ResultCode result;
  result = sBrokerService->SpawnTarget(aPath, aArguments, mPolicy, &targetInfo);
  if (sandbox::SBOX_ALL_OK != result) {
    return false;
  }

  // The sandboxed process is started in a suspended state, resume it now that
  // we've set things up.
  ResumeThread(targetInfo.hThread);
  CloseHandle(targetInfo.hThread);

  // Return the process handle to the caller
  *aProcessHandle = targetInfo.hProcess;

  return true;
}

#if defined(MOZ_CONTENT_SANDBOX)
void
SandboxBroker::SetSecurityLevelForContentProcess(int32_t aSandboxLevel)
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
  } else if (aSandboxLevel >= 2) {
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

  sandbox::ResultCode result = mPolicy->SetJobLevel(jobLevel,
                                                    0 /* ui_exceptions */);
  MOZ_RELEASE_ASSERT(sandbox::SBOX_ALL_OK == result,
                     "Setting job level failed, have you set memory limit when jobLevel == JOB_NONE?");

  result = mPolicy->SetTokenLevel(sandbox::USER_RESTRICTED_SAME_ACCESS,
                                  accessTokenLevel);
  MOZ_RELEASE_ASSERT(sandbox::SBOX_ALL_OK == result,
                     "Lockdown level cannot be USER_UNPROTECTED or USER_LAST if initial level was USER_RESTRICTED_SAME_ACCESS");

  result = mPolicy->SetIntegrityLevel(initialIntegrityLevel);
  MOZ_RELEASE_ASSERT(sandbox::SBOX_ALL_OK == result,
                     "SetIntegrityLevel should never fail, what happened?");
  result = mPolicy->SetDelayedIntegrityLevel(delayedIntegrityLevel);
  MOZ_RELEASE_ASSERT(sandbox::SBOX_ALL_OK == result,
                     "SetDelayedIntegrityLevel should never fail, what happened?");

  if (aSandboxLevel > 2) {
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
}
#endif

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

  // The following is required for the Java plugin.
  result = mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_FILES,
                            sandbox::TargetPolicy::FILES_ALLOW_ANY,
                            L"\\??\\pipe\\jpi2_pid*_pipe*");
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
  return (sandbox::SBOX_ALL_OK == result);
}

bool
SandboxBroker::AllowReadWriteFile(wchar_t const *file)
{
  if (!mPolicy) {
    return false;
  }

  auto result =
    mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_FILES,
                     sandbox::TargetPolicy::FILES_ALLOW_ANY,
                     file);
  return (sandbox::SBOX_ALL_OK == result);
}

bool
SandboxBroker::AllowDirectory(wchar_t const *dir)
{
  if (!mPolicy) {
    return false;
  }

  auto result =
    mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_FILES,
                     sandbox::TargetPolicy::FILES_ALLOW_DIR_ANY,
                     dir);
  return (sandbox::SBOX_ALL_OK == result);
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
