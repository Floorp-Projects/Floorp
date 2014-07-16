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

  mPolicy->SetJobLevel(sandbox::JOB_NONE, 0);
  mPolicy->SetTokenLevel(sandbox::USER_RESTRICTED_SAME_ACCESS,
                         sandbox::USER_RESTRICTED_SAME_ACCESS);
  mPolicy->SetDelayedIntegrityLevel(sandbox::INTEGRITY_LEVEL_LOW);
  mPolicy->SetAlternateDesktop(true);
  return true;
}

bool
SandboxBroker::SetSecurityLevelForPluginProcess()
{
  if (!mPolicy) {
    return false;
  }

  mPolicy->SetJobLevel(sandbox::JOB_NONE, 0);
  mPolicy->SetTokenLevel(sandbox::USER_UNPROTECTED,
                         sandbox::USER_UNPROTECTED);
  return true;
}

bool
SandboxBroker::SetSecurityLevelForIPDLUnitTestProcess()
{
  if (!mPolicy) {
    return false;
  }

  mPolicy->SetJobLevel(sandbox::JOB_NONE, 0);
  mPolicy->SetTokenLevel(sandbox::USER_RESTRICTED_SAME_ACCESS,
                         sandbox::USER_RESTRICTED_SAME_ACCESS);
  return true;
}

bool
SandboxBroker::SetSecurityLevelForGMPlugin()
{
  if (!mPolicy) {
    return false;
  }

  mPolicy->SetJobLevel(sandbox::JOB_LOCKDOWN, 0);
  mPolicy->SetTokenLevel(sandbox::USER_RESTRICTED_SAME_ACCESS,
                         sandbox::USER_RESTRICTED_SAME_ACCESS);
  mPolicy->SetDelayedIntegrityLevel(sandbox::INTEGRITY_LEVEL_LOW);
  mPolicy->SetAlternateDesktop(true);
  return true;
}


SandboxBroker::~SandboxBroker()
{
  if (mPolicy) {
    mPolicy->Release();
    mPolicy = nullptr;
  }
}

}
