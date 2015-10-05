/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "sandboxBroker.h"

#include "base/win/windows_version.h"
#include "sandbox/win/src/sandbox.h"
#include "sandbox/win/src/sandbox_factory.h"
#include "sandbox/win/src/security_level.h"
#include "mozilla/sandboxing/sandboxLogging.h"

namespace mozilla
{

sandbox::BrokerServices *SandboxBroker::sBrokerService = nullptr;

SandboxBroker::SandboxBroker()
{
  // XXX: This is not thread-safe! Two threads could simultaneously try
  // to set `sBrokerService`
  if (!sBrokerService) {
    sBrokerService = sandbox::SandboxFactory::GetBrokerServices();
    if (sBrokerService) {
      sandbox::ResultCode result = sBrokerService->Init();
      if (result != sandbox::SBOX_ALL_OK) {
        sBrokerService = nullptr;
      }
    }
  }

  mPolicy = sBrokerService->CreatePolicy();
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
    mozilla::sandboxing::ApplyLoggingPolicy(*mPolicy);
  }

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
bool
SandboxBroker::SetSecurityLevelForContentProcess(int32_t aSandboxLevel)
{
  if (!mPolicy) {
    return false;
  }

  sandbox::JobLevel jobLevel;
  sandbox::TokenLevel accessTokenLevel;
  sandbox::IntegrityLevel initialIntegrityLevel;
  sandbox::IntegrityLevel delayedIntegrityLevel;

  // The setting of these levels is pretty arbitrary, but they are a useful (if
  // crude) tool while we are tightening the policy. Gaps are left to try and
  // avoid changing their meaning.
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
  } else {
    jobLevel = sandbox::JOB_NONE;
    accessTokenLevel = sandbox::USER_NON_ADMIN;
    initialIntegrityLevel = sandbox::INTEGRITY_LEVEL_MEDIUM;
    delayedIntegrityLevel = sandbox::INTEGRITY_LEVEL_MEDIUM;
  }

  sandbox::ResultCode result = mPolicy->SetJobLevel(jobLevel,
                                                    0 /* ui_exceptions */);
  bool ret = (sandbox::SBOX_ALL_OK == result);

  result = mPolicy->SetTokenLevel(sandbox::USER_RESTRICTED_SAME_ACCESS,
                                  accessTokenLevel);
  ret = ret && (sandbox::SBOX_ALL_OK == result);

  result = mPolicy->SetIntegrityLevel(initialIntegrityLevel);
  ret = ret && (sandbox::SBOX_ALL_OK == result);
  result = mPolicy->SetDelayedIntegrityLevel(delayedIntegrityLevel);
  ret = ret && (sandbox::SBOX_ALL_OK == result);

  if (aSandboxLevel > 2) {
    result = mPolicy->SetAlternateDesktop(true);
    ret = ret && (sandbox::SBOX_ALL_OK == result);
  }

  if (aSandboxLevel >= 1) {
    sandbox::MitigationFlags mitigations =
      sandbox::MITIGATION_BOTTOM_UP_ASLR |
      sandbox::MITIGATION_HEAP_TERMINATE |
      sandbox::MITIGATION_SEHOP |
      sandbox::MITIGATION_DEP_NO_ATL_THUNK |
      sandbox::MITIGATION_DEP;

    result = mPolicy->SetProcessMitigations(mitigations);
    ret = ret && (sandbox::SBOX_ALL_OK == result);

    mitigations =
      sandbox::MITIGATION_STRICT_HANDLE_CHECKS |
      sandbox::MITIGATION_DLL_SEARCH_ORDER;

    result = mPolicy->SetDelayedProcessMitigations(mitigations);
    ret = ret && (sandbox::SBOX_ALL_OK == result);
  }

  // Add the policy for the client side of a pipe. It is just a file
  // in the \pipe\ namespace. We restrict it to pipes that start with
  // "chrome." so the sandboxed process cannot connect to system services.
  result = mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_FILES,
                            sandbox::TargetPolicy::FILES_ALLOW_ANY,
                            L"\\??\\pipe\\chrome.*");
  ret = ret && (sandbox::SBOX_ALL_OK == result);

  // Add the policy for the client side of the crash server pipe.
  result = mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_FILES,
                            sandbox::TargetPolicy::FILES_ALLOW_ANY,
                            L"\\??\\pipe\\gecko-crash-server-pipe.*");
  ret = ret && (sandbox::SBOX_ALL_OK == result);

  // The content process needs to be able to duplicate named pipes back to the
  // broker process, which are File type handles.
  result = mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_HANDLES,
                            sandbox::TargetPolicy::HANDLES_DUP_BROKER,
                            L"File");
  ret = ret && (sandbox::SBOX_ALL_OK == result);

  // The content process needs to be able to duplicate shared memory to the
  // broker process, which are Section type handles.
  result = mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_HANDLES,
                            sandbox::TargetPolicy::HANDLES_DUP_BROKER,
                            L"Section");
  ret = ret && (sandbox::SBOX_ALL_OK == result);

  return ret;
}
#endif

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
  bool ret = (sandbox::SBOX_ALL_OK == result);

  result = mPolicy->SetTokenLevel(sandbox::USER_RESTRICTED_SAME_ACCESS,
                                  accessTokenLevel);
  ret = ret && (sandbox::SBOX_ALL_OK == result);

  result = mPolicy->SetIntegrityLevel(initialIntegrityLevel);
  ret = ret && (sandbox::SBOX_ALL_OK == result);
  result = mPolicy->SetDelayedIntegrityLevel(delayedIntegrityLevel);
  ret = ret && (sandbox::SBOX_ALL_OK == result);

  sandbox::MitigationFlags mitigations =
    sandbox::MITIGATION_BOTTOM_UP_ASLR |
    sandbox::MITIGATION_HEAP_TERMINATE |
    sandbox::MITIGATION_SEHOP |
    sandbox::MITIGATION_DEP_NO_ATL_THUNK |
    sandbox::MITIGATION_DEP;

  result = mPolicy->SetProcessMitigations(mitigations);
  ret = ret && (sandbox::SBOX_ALL_OK == result);

  // Add the policy for the client side of a pipe. It is just a file
  // in the \pipe\ namespace. We restrict it to pipes that start with
  // "chrome." so the sandboxed process cannot connect to system services.
  result = mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_FILES,
                            sandbox::TargetPolicy::FILES_ALLOW_ANY,
                            L"\\??\\pipe\\chrome.*");
  ret = ret && (sandbox::SBOX_ALL_OK == result);

  // Add the policy for the client side of the crash server pipe.
  result = mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_FILES,
                            sandbox::TargetPolicy::FILES_ALLOW_ANY,
                            L"\\??\\pipe\\gecko-crash-server-pipe.*");
  ret = ret && (sandbox::SBOX_ALL_OK == result);

  // The NPAPI process needs to be able to duplicate shared memory to the
  // content process and broker process, which are Section type handles.
  // Content and broker are for e10s and non-e10s cases.
  result = mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_HANDLES,
                            sandbox::TargetPolicy::HANDLES_DUP_ANY,
                            L"Section");
  ret = ret && (sandbox::SBOX_ALL_OK == result);

  result = mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_HANDLES,
                            sandbox::TargetPolicy::HANDLES_DUP_BROKER,
                            L"Section");
  ret = ret && (sandbox::SBOX_ALL_OK == result);

  // The following is required for the Java plugin.
  result = mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_FILES,
                            sandbox::TargetPolicy::FILES_ALLOW_ANY,
                            L"\\??\\pipe\\jpi2_pid*_pipe*");
  ret = ret && (sandbox::SBOX_ALL_OK == result);

  return ret;
}

bool
SandboxBroker::SetSecurityLevelForIPDLUnitTestProcess()
{
  if (!mPolicy) {
    return false;
  }

  auto result = mPolicy->SetJobLevel(sandbox::JOB_NONE, 0);
  bool ret = (sandbox::SBOX_ALL_OK == result);
  result =
    mPolicy->SetTokenLevel(sandbox::USER_RESTRICTED_SAME_ACCESS,
                           sandbox::USER_RESTRICTED_SAME_ACCESS);
  ret = ret && (sandbox::SBOX_ALL_OK == result);
  return ret;
}

bool
SandboxBroker::SetSecurityLevelForGMPlugin()
{
  if (!mPolicy) {
    return false;
  }

  auto result = mPolicy->SetJobLevel(sandbox::JOB_LOCKDOWN, 0);
  bool ret = (sandbox::SBOX_ALL_OK == result);
  if (base::win::GetVersion() < base::win::VERSION_VISTA) {
    result = mPolicy->SetTokenLevel(sandbox::USER_RESTRICTED_SAME_ACCESS,
                                    sandbox::USER_LOCKDOWN);
  } else {
    result = mPolicy->SetTokenLevel(sandbox::USER_RESTRICTED_SAME_ACCESS,
                                    sandbox::USER_RESTRICTED);
  }
  ret = ret && (sandbox::SBOX_ALL_OK == result);

  result = mPolicy->SetAlternateDesktop(true);
  ret = ret && (sandbox::SBOX_ALL_OK == result);

  result = mPolicy->SetIntegrityLevel(sandbox::INTEGRITY_LEVEL_LOW);
  ret = ret && (sandbox::SBOX_ALL_OK == result);

  result =
    mPolicy->SetDelayedIntegrityLevel(sandbox::INTEGRITY_LEVEL_UNTRUSTED);
  ret = ret && (sandbox::SBOX_ALL_OK == result);

  sandbox::MitigationFlags mitigations =
    sandbox::MITIGATION_BOTTOM_UP_ASLR |
    sandbox::MITIGATION_HEAP_TERMINATE |
    sandbox::MITIGATION_SEHOP |
    sandbox::MITIGATION_DEP_NO_ATL_THUNK |
    sandbox::MITIGATION_DEP;

  result = mPolicy->SetProcessMitigations(mitigations);
  ret = ret && (sandbox::SBOX_ALL_OK == result);

  mitigations =
    sandbox::MITIGATION_STRICT_HANDLE_CHECKS |
    sandbox::MITIGATION_DLL_SEARCH_ORDER;

  result = mPolicy->SetDelayedProcessMitigations(mitigations);
  ret = ret && (sandbox::SBOX_ALL_OK == result);

  // Add the policy for the client side of a pipe. It is just a file
  // in the \pipe\ namespace. We restrict it to pipes that start with
  // "chrome." so the sandboxed process cannot connect to system services.
  result = mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_FILES,
                            sandbox::TargetPolicy::FILES_ALLOW_ANY,
                            L"\\??\\pipe\\chrome.*");
  ret = ret && (sandbox::SBOX_ALL_OK == result);

  // Add the policy for the client side of the crash server pipe.
  result = mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_FILES,
                            sandbox::TargetPolicy::FILES_ALLOW_ANY,
                            L"\\??\\pipe\\gecko-crash-server-pipe.*");
  ret = ret && (sandbox::SBOX_ALL_OK == result);

#ifdef DEBUG
  // The plugin process can't create named events, but we'll
  // make an exception for the events used in logging. Removing
  // this will break EME in debug builds.
  result = mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_SYNC,
                            sandbox::TargetPolicy::EVENTS_ALLOW_ANY,
                            L"ChromeIPCLog.*");
  ret = ret && (sandbox::SBOX_ALL_OK == result);
#endif

  // The following rules were added because, during analysis of an EME
  // plugin during development, these registry keys were accessed when
  // loading the plugin. Commenting out these policy exceptions caused
  // plugin loading to fail, so they are necessary for proper functioning
  // of at least one EME plugin.
  result = mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_REGISTRY,
                            sandbox::TargetPolicy::REG_ALLOW_READONLY,
                            L"HKEY_CURRENT_USER");
  ret = ret && (sandbox::SBOX_ALL_OK == result);

  result = mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_REGISTRY,
                            sandbox::TargetPolicy::REG_ALLOW_READONLY,
                            L"HKEY_CURRENT_USER\\Control Panel\\Desktop");
  ret = ret && (sandbox::SBOX_ALL_OK == result);

  result = mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_REGISTRY,
                            sandbox::TargetPolicy::REG_ALLOW_READONLY,
                            L"HKEY_CURRENT_USER\\Control Panel\\Desktop\\LanguageConfiguration");
  ret = ret && (sandbox::SBOX_ALL_OK == result);

  result = mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_REGISTRY,
                            sandbox::TargetPolicy::REG_ALLOW_READONLY,
                            L"HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\SideBySide");
  ret = ret && (sandbox::SBOX_ALL_OK == result);


  // The following rules were added because, during analysis of an EME
  // plugin during development, these registry keys were accessed when
  // loading the plugin. Commenting out these policy exceptions did not
  // cause anything to break during initial testing, but might cause
  // unforeseen issues down the road.
  result = mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_REGISTRY,
                            sandbox::TargetPolicy::REG_ALLOW_READONLY,
                            L"HKEY_LOCAL_MACHINE\\SOFTWARE\\Policies\\Microsoft\\MUI\\Settings");
  ret = ret && (sandbox::SBOX_ALL_OK == result);

  result = mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_REGISTRY,
                            sandbox::TargetPolicy::REG_ALLOW_READONLY,
                            L"HKEY_CURRENT_USER\\Software\\Policies\\Microsoft\\Control Panel\\Desktop");
  ret = ret && (sandbox::SBOX_ALL_OK == result);

  result = mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_REGISTRY,
                            sandbox::TargetPolicy::REG_ALLOW_READONLY,
                            L"HKEY_CURRENT_USER\\Control Panel\\Desktop\\PreferredUILanguages");
  ret = ret && (sandbox::SBOX_ALL_OK == result);

  result = mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_REGISTRY,
                            sandbox::TargetPolicy::REG_ALLOW_READONLY,
                            L"HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\SideBySide\\PreferExternalManifest");
  ret = ret && (sandbox::SBOX_ALL_OK == result);

  return ret;
}

bool
SandboxBroker::AllowReadFile(wchar_t const *file)
{
  auto result =
    mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_FILES,
                     sandbox::TargetPolicy::FILES_ALLOW_READONLY,
                     file);
  return (sandbox::SBOX_ALL_OK == result);
}

bool
SandboxBroker::AllowReadWriteFile(wchar_t const *file)
{
  auto result =
    mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_FILES,
                     sandbox::TargetPolicy::FILES_ALLOW_ANY,
                     file);
  return (sandbox::SBOX_ALL_OK == result);
}

bool
SandboxBroker::AllowDirectory(wchar_t const *dir)
{
  auto result =
    mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_FILES,
                     sandbox::TargetPolicy::FILES_ALLOW_DIR_ANY,
                     dir);
  return (sandbox::SBOX_ALL_OK == result);
}

bool
SandboxBroker::AddTargetPeer(HANDLE aPeerProcess)
{
  sandbox::ResultCode result = sBrokerService->AddTargetPeer(aPeerProcess);
  return (sandbox::SBOX_ALL_OK == result);
}

SandboxBroker::~SandboxBroker()
{
  if (mPolicy) {
    mPolicy->Release();
    mPolicy = nullptr;
  }
}

}
