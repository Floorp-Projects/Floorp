/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "sandboxBroker.h"
#include "sandbox/win/src/sandbox.h"
#include "sandbox/win/src/sandbox_factory.h"
#include "sandbox/win/src/security_level.h"

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
                         void **aProcessHandle)
{
  if (!sBrokerService || !mPolicy) {
    return false;
  }

  // Set stdout and stderr, to allow inheritance for logging.
  mPolicy->SetStdoutHandle(::GetStdHandle(STD_OUTPUT_HANDLE));
  mPolicy->SetStderrHandle(::GetStdHandle(STD_ERROR_HANDLE));

  // Ceate the sandboxed process
  PROCESS_INFORMATION targetInfo;
  sandbox::ResultCode result;
  result = sBrokerService->SpawnTarget(aPath, aArguments, mPolicy, &targetInfo);

  // The sandboxed process is started in a suspended state, resume it now that
  // we've set things up.
  ResumeThread(targetInfo.hThread);
  CloseHandle(targetInfo.hThread);

  // Return the process handle to the caller
  *aProcessHandle = targetInfo.hProcess;

  return true;
}

bool
SandboxBroker::SetSecurityLevelForContentProcess()
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
  result =
    mPolicy->SetDelayedIntegrityLevel(sandbox::INTEGRITY_LEVEL_LOW);
  ret = ret && (sandbox::SBOX_ALL_OK == result);
  result = mPolicy->SetAlternateDesktop(true);
  ret = ret && (sandbox::SBOX_ALL_OK == result);
  return ret;
}

bool
SandboxBroker::SetSecurityLevelForPluginProcess()
{
  if (!mPolicy) {
    return false;
  }

  auto result = mPolicy->SetJobLevel(sandbox::JOB_NONE, 0);
  bool ret = (sandbox::SBOX_ALL_OK == result);
  result = mPolicy->SetTokenLevel(sandbox::USER_UNPROTECTED,
                                  sandbox::USER_UNPROTECTED);
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
  result =
    mPolicy->SetTokenLevel(sandbox::USER_RESTRICTED_SAME_ACCESS,
                           sandbox::USER_RESTRICTED);
  ret = ret && (sandbox::SBOX_ALL_OK == result);

  result = mPolicy->SetAlternateDesktop(true);
  ret = ret && (sandbox::SBOX_ALL_OK == result);

  // We can't use an alternate desktop/window station AND initially
  // set the process to low integrity. Upstream changes have been
  // made to allow this and we should uncomment this section once
  // we've rolled forward.
  // result =
  //   mPolicy->SetIntegrityLevel(sandbox::INTEGRITY_LEVEL_LOW);
  // ret = ret && (sandbox::SBOX_ALL_OK == result);

  result =
    mPolicy->SetDelayedIntegrityLevel(sandbox::INTEGRITY_LEVEL_UNTRUSTED);
  ret = ret && (sandbox::SBOX_ALL_OK == result);

  // Add the policy for the client side of a pipe. It is just a file
  // in the \pipe\ namespace. We restrict it to pipes that start with
  // "chrome." so the sandboxed process cannot connect to system services.
  result = mPolicy->AddRule(sandbox::TargetPolicy::SUBSYS_FILES,
                            sandbox::TargetPolicy::FILES_ALLOW_ANY,
                            L"\\??\\pipe\\chrome.*");
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

SandboxBroker::~SandboxBroker()
{
  if (mPolicy) {
    mPolicy->Release();
    mPolicy = nullptr;
  }
}

}
